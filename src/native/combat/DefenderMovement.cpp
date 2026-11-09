#include "DefenderMovement.h"
#include "HealerMovement.h"
#include "DefenderMovementRules.h"
#include "DefenderProgress.h"
#include "DefenderTuning.h"
#include "HealingGeometry.h"
#include "DefenderObservation.h"
#include <array>
#include <cmath>

namespace mod::defender_movement {
    namespace {
        std::array<RE::TESPackage*, Slots * 4> packages{};
        std::array<RE::TESObjectREFR*, Slots> markers{};
        RE::TESFaction* activity = nullptr;

        struct State {
            std::uint32_t actor = 0;
            RE::FormID area = 0;
            defender_movement_rules::Progress progress;
            Observation observation;
            defender_tactics::Motion motion = defender_tactics::Motion::None;
            std::uint32_t threat = 0;
        };

        std::array<State, Slots> states{};

        struct TestBounds {
            RE::FormID actor = 0, area = 0;
            defender_movement_rules::Point anchor;
            float radius = 0;
        } testBounds;

        RE::FormID Area(RE::Actor* actor) {
            const auto* world = actor->GetWorldspace();
            const auto* cell = actor->GetParentCell();
            return world ? world->GetFormID() : cell ? cell->GetFormID() : 0;
        }

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return;
        activity = data->LookupForm<RE::TESFaction>(0xB28, info::PluginFile);
        for (std::size_t i = 0; i < packages.size(); ++i)
            packages[i] = data->LookupForm<RE::TESPackage>(static_cast<RE::FormID>(0xB00 + i), info::PluginFile);
        for (std::size_t i = 0; i < markers.size(); ++i)
            markers[i] = data->LookupForm<RE::TESObjectREFR>(static_cast<RE::FormID>(0xB20 + i), info::PluginFile);
    }

    bool Owned(const RE::TESPackage* package) {
        return package && std::ranges::find(packages, package) != packages.end();
    }

    bool Ready(std::size_t slot) {
        return slot < Slots && activity && markers[slot] && packages[slot] && packages[slot + Slots] &&
               packages[slot + Slots * 2] && packages[slot + Slots * 3];
    }

    void Release(RE::Actor* actor) {
        if (actor)
            for (auto& state : states)
                if (state.actor == actor->GetFormID())
                    state.observation.active = false;
        if (!actor || !activity || !actor->IsInFaction(activity))
            return;
        actor->RemoveFromFaction(activity);
        if (Owned(Interrupt(actor)))
            actor->EndInterruptPackage(true);
        actor->EvaluatePackage(true, false);
    }

    void Sample(RE::Actor* actor, float elapsed) {
        if (!actor)
            return;
        for (auto& state : states)
            if (state.actor == actor->GetFormID()) {
                if (state.area != Area(actor)) {
                    state.progress = {};
                    state.observation = {};
                    state.area = Area(actor);
                }
                state.progress.Sample({actor->GetPositionX(), actor->GetPositionY()}, elapsed,
                                      state.observation.active);
                state.observation.stalls = state.progress.Stalls();
                state.observation.detours = state.progress.Detours();
                state.observation.progress = state.progress.Measured();
                state.observation.waiting = state.progress.Waiting();
            }
    }

    Observation Inspect(std::uint32_t actor) {
        for (const auto& state : states)
            if (state.actor == actor)
                return state.observation;
        return {};
    }

    void Clear() {
        states = {};
        ClearTestBounds();
    }

    void ClearTestBounds() {
        testBounds = {};
    }

    bool SetTestBounds(RE::Actor* actor, RE::TESObjectREFR* anchor, float radius) {
        if (!actor || !anchor || actor->GetParentCell() != anchor->GetParentCell() ||
            !defender_movement_rules::WithinBounds({anchor->GetPositionX(), anchor->GetPositionY()},
                                                   {actor->GetPositionX(), actor->GetPositionY()}, radius))
            return false;
        testBounds = {actor->GetFormID(), Area(actor), {anchor->GetPositionX(), anchor->GetPositionY()}, radius};
        return true;
    }

    bool ClearEvade(RE::Actor* actor, const RE::NiPoint3& destination) {
        if (!actor || testBounds.actor != actor->GetFormID() || testBounds.area != Area(actor) ||
            !defender_movement_rules::WithinBounds(testBounds.anchor, {destination.x, destination.y},
                                                   testBounds.radius))
            return false;
        const auto ground = healer_movement::Ground(actor, {destination.x, destination.y, destination.z});
        if (!ground || std::abs(ground->z - destination.z) > 48 ||
            !healing_geometry::Corridor(actor, {ground->x, ground->y, ground->z}))
            return false;
        const auto origin = actor->GetPosition();
        for (const float fraction : {.25f, .5f, .75f}) {
            const auto sample =
                healer_movement::Ground(actor, {origin.x + (destination.x - origin.x) * fraction,
                                                origin.y + (destination.y - origin.y) * fraction, origin.z});
            if (!sample || std::abs(sample->z - origin.z) > 48)
                return false;
        }
        std::vector<defender_movement_rules::Point> opponents;
        for (const auto& threat : defender_observation::Threats(actor))
            if (auto* enemy = RE::TESForm::LookupByID<RE::Actor>(threat.actor))
                opponents.push_back({enemy->GetPositionX(), enemy->GetPositionY()});
        return defender_movement_rules::ClearStep({actor->GetPositionX(), actor->GetPositionY()},
                                                  {ground->x, ground->y}, opponents, false);
    }

    bool Move(RE::Actor* actor, RE::Actor* threat, std::size_t slot, defender_tactics::Motion motion,
              std::span<const defender_tactics::Threat> threats, bool guarded) {
        using defender_tactics::Motion;
        if (!actor || !threat || !Ready(slot) || (Interrupt(actor) && !Owned(Interrupt(actor))))
            return false;
        auto& state = states[slot];
        if (state.actor != actor->GetFormID() || state.area != Area(actor)) {
            state = {};
            state.actor = actor->GetFormID();
            state.area = Area(actor);
            state.progress.Sample({actor->GetPositionX(), actor->GetPositionY()}, .1f, false);
        }
        if (state.progress.Yielding()) {
            state.observation.status = Status::Yield;
            return false;
        }
        const auto origin = actor->GetPosition(), target = threat->GetPosition();
        const float distance = std::hypot(target.x - origin.x, target.y - origin.y);
        if (!std::isfinite(distance) || distance < 1 || distance > 2400)
            return false;
        const float x = (target.x - origin.x) / distance, y = (target.y - origin.y) / distance;
        const auto tuning = defender_tuning::TestScope.Read(actor->GetFormID());
        auto desired = origin;
        if (motion == Motion::Close || motion == Motion::Intercept) {
            if (distance <= 192)
                return false;
            const float step = defender_movement_rules::ApproachStep(distance, tuning.approachStep);
            desired.x += x * step;
            desired.y += y * step;
        } else if (motion == Motion::Retreat) {
            desired.x -= x * tuning.retreatStep;
            desired.y -= y * tuning.retreatStep;
        } else if (motion == Motion::Lateral) {
            desired.x += y * tuning.lateralStep;
            desired.y -= x * tuning.lateralStep;
        } else if (motion == Motion::AdvanceLateral) {
            const auto step =
                defender_movement_rules::AdvanceLateralStep(distance, tuning.approachStep, tuning.lateralStep);
            desired.x += x * step.x + y * step.y;
            desired.y += y * step.x - x * step.y;
        } else {
            return false;
        }
        // A refresh validates the existing short waypoint rather than moving it
        // away from an actor that has not reached it. Stalls still select detours.
        const auto& previousDestination = state.observation;
        const bool keep = defender_movement_rules::KeepWaypoint(
            state.observation.active &&
                (motion == Motion::Retreat || motion == Motion::Lateral || motion == Motion::AdvanceLateral),
            state.motion == motion && state.threat == threat->GetFormID(), state.progress.Detouring(),
            {origin.x, origin.y}, {previousDestination.x, previousDestination.y});
        if (keep)
            desired = {previousDestination.x, previousDestination.y, previousDestination.z};
        std::vector<defender_movement_rules::Point> opponents;
        for (const auto& candidate : threats)
            if (auto* enemy = RE::TESForm::LookupByID<RE::Actor>(candidate.actor))
                opponents.push_back({enemy->GetPositionX(), enemy->GetPositionY()});
        const auto ground = [&](const RE::NiPoint3& requested, bool detour) -> std::optional<healer_movement::Point> {
            const auto projected = healer_movement::Ground(actor, {requested.x, requested.y, requested.z});
            auto* player = RE::PlayerCharacter::GetSingleton();
            const bool scoped = testBounds.actor == actor->GetFormID() && testBounds.area == Area(actor);
            const bool inside =
                projected && (scoped ? defender_movement_rules::WithinBounds(
                                           testBounds.anchor, {projected->x, projected->y}, testBounds.radius)
                                     : !player || std::hypot(projected->x - player->GetPositionX(),
                                                             projected->y - player->GetPositionY()) <= 2400);
            return inside && !state.progress.Blocked({projected->x, projected->y}) &&
                           healing_geometry::Corridor(actor, {projected->x, projected->y, projected->z}) &&
                           defender_movement_rules::ClearStep({origin.x, origin.y}, {projected->x, projected->y},
                                                              opponents, motion == Motion::Retreat, detour)
                       ? projected
                       : std::nullopt;
        };
        auto point = state.progress.Detouring() ? std::optional<healer_movement::Point>{} : ground(desired, false);
        bool detour = !point;
        if (motion == Motion::Retreat && (!keep || !point)) {
            float best =
                point ? defender_movement_rules::FormationScore({point->x, point->y}, {target.x, target.y}, opponents)
                      : -100000;
            for (const float angle : {.785398f, -.785398f, 1.570796f, -1.570796f}) {
                const float dx = -x * tuning.retreatStep, dy = -y * tuning.retreatStep;
                const auto candidate = ground({origin.x + std::cos(angle) * dx - std::sin(angle) * dy,
                                               origin.y + std::sin(angle) * dx + std::cos(angle) * dy, origin.z},
                                              true);
                if (!candidate)
                    continue;
                const float score = defender_movement_rules::FormationScore({candidate->x, candidate->y},
                                                                            {target.x, target.y}, opponents);
                if (score > best) {
                    point = candidate;
                    best = score;
                    detour = true;
                }
            }
        }
        if (!point && motion == Motion::AdvanceLateral) {
            for (const auto side : {1.0f, -1.0f}) {
                const RE::NiPoint3 lateral{origin.x + side * y * tuning.lateralStep,
                                           origin.y - side * x * tuning.lateralStep, origin.z};
                point = ground(lateral, true);
                if (point)
                    break;
            }
        }
        const float dx = desired.x - origin.x, dy = desired.y - origin.y;
        // A short lateral waypoint can open a route around a column. Keep the
        // first segment clear; Skyrim remains responsible for actual pathfinding.
        for (const auto angle : {.785398f, -.785398f, 1.570796f, -1.570796f, 2.356194f, -2.356194f}) {
            if (point)
                break;
            const RE::NiPoint3 alternative{origin.x + std::cos(angle) * dx - std::sin(angle) * dy,
                                           origin.y + std::sin(angle) * dx + std::cos(angle) * dy, origin.z};
            point = ground(alternative, true);
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        const bool scoped = testBounds.actor == actor->GetFormID() && testBounds.area == Area(actor);
        const bool tethered =
            point &&
            (scoped
                 ? !defender_movement_rules::WithinBounds(testBounds.anchor, {point->x, point->y}, testBounds.radius)
                 : player && std::hypot(point->x - player->GetPositionX(), point->y - player->GetPositionY()) > 2400);
        if (!point || tethered) {
            state.observation.status = point ? Status::Tether : Status::Blocked;
            return false;
        }
        auto* marker = markers[slot];
        const RE::NiPoint3 destination{point->x, point->y, point->z};
        const bool areaChanged =
            marker->GetWorldspace() != actor->GetWorldspace() || marker->GetParentCell() != actor->GetParentCell();
        const bool changed = areaChanged || marker->GetPosition().GetDistance(destination) > 96;
        if (areaChanged)
            marker->MoveTo(actor);
        if (changed)
            marker->SetPosition(destination);
        const auto previous = actor->GetFactionRank(activity, false);
        const auto family = guarded ? Slots * 2 : 0;
        const int baseRank = static_cast<int>(slot + family);
        if (!changed && (previous == baseRank || previous == baseRank + static_cast<int>(Slots))) {
            state.motion = motion;
            state.threat = threat->GetFormID();
            if (!state.observation.active) {
                state.observation.active = true;
                state.observation.x = marker->GetPositionX();
                state.observation.y = marker->GetPositionY();
                state.observation.z = marker->GetPositionZ();
                state.progress.Command({state.observation.x, state.observation.y}, false);
            }
            return true;
        }
        const int rank = baseRank + (previous == baseRank ? static_cast<int>(Slots) : 0);
        actor->AddToFaction(activity, static_cast<std::int8_t>(rank));
        actor->EvaluatePackage(true, false);
        state.observation.active = true;
        state.motion = motion;
        state.threat = threat->GetFormID();
        state.observation.x = destination.x;
        state.observation.y = destination.y;
        state.observation.z = destination.z;
        state.observation.status = detour ? Status::Detour : Status::Direct;
        state.progress.Command({destination.x, destination.y}, detour);
        return true;
    }
}
