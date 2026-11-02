#include "DefenderMovement.h"
#include "HealerMovement.h"
#include "DefenderMovementRules.h"
#include "DefenderProgress.h"
#include "HealingGeometry.h"
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
        auto desired = origin;
        if (motion == Motion::Close || motion == Motion::Intercept) {
            if (distance <= 192)
                return false;
            const float step = defender_movement_rules::ApproachStep(distance);
            desired.x += x * step;
            desired.y += y * step;
        } else if (motion == Motion::Retreat) {
            desired.x -= x * 256;
            desired.y -= y * 256;
        } else if (motion == Motion::Lateral) {
            desired.x += y * 256;
            desired.y -= x * 256;
        } else {
            return false;
        }
        std::vector<defender_movement_rules::Point> opponents;
        for (const auto& candidate : threats)
            if (auto* enemy = RE::TESForm::LookupByID<RE::Actor>(candidate.actor))
                opponents.push_back({enemy->GetPositionX(), enemy->GetPositionY()});
        const auto ground = [&](const RE::NiPoint3& requested, bool detour) -> std::optional<healer_movement::Point> {
            const auto projected = healer_movement::Ground(actor, {requested.x, requested.y, requested.z});
            return projected && !state.progress.Blocked({projected->x, projected->y}) &&
                           healing_geometry::Corridor(actor, {projected->x, projected->y, projected->z}) &&
                           defender_movement_rules::ClearStep({origin.x, origin.y}, {projected->x, projected->y},
                                                              opponents, motion == Motion::Retreat, detour)
                       ? projected
                       : std::nullopt;
        };
        auto point = state.progress.Detouring() ? std::optional<healer_movement::Point>{} : ground(desired, false);
        bool detour = !point;
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
        state.observation.x = destination.x;
        state.observation.y = destination.y;
        state.observation.z = destination.z;
        state.observation.status = detour ? Status::Detour : Status::Direct;
        state.progress.Command({destination.x, destination.y}, detour);
        return true;
    }
}
