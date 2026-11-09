#include "Evade.h"
#include "EvadeRules.h"
#include "combat/ShieldDefender.h"
#include "combat/DefenderMovement.h"
#include "combat/HealerMovement.h"
#include "combat/HealingGeometry.h"
#include "combat/DefenderMovementRules.h"
#include <array>
#include <algorithm>

namespace mod::combat_actions::evade {
    namespace {
        struct Scope {
            std::uint32_t actor = 0, token = 0;
            yliwf::testing::EvadeBackend backend;
            evade_rules::Execution execution;
            Observation observation;
            RE::NiPoint3 origin;
            RE::NiPoint3 direction;
            float cooldown = 0;
            float costRefresh = 0, cost = -1;
            bool busy = false, closing = false;
        } scope;
    }

    bool Configure(std::uint32_t actor, std::uint32_t token, const yliwf::testing::EvadeBackend& backend) {
        if (!actor || !token || (scope.actor && (!scope.closing || scope.busy)) || backend.size != sizeof(backend) ||
            !backend.cost || !backend.request || !std::isfinite(backend.clearance) || backend.clearance < 128 ||
            backend.clearance > 512)
            return false;
        scope = {};
        scope.actor = actor;
        scope.token = token;
        scope.backend = backend;
        return true;
    }

    bool Busy(std::uint32_t actor) {
        return actor && scope.actor == actor && scope.busy;
    }

    Observation Inspect(std::uint32_t actor) {
        return actor && scope.actor == actor ? scope.observation : Observation{};
    }

    void Reset() {
        scope = {};
    }

    void End(std::uint32_t actor, std::uint32_t token) {
        if (actor == scope.actor && token == scope.token) {
            scope.closing = true;
            scope.backend = {};
            scope.busy = scope.execution.phase == evade_rules::Phase::Pending ||
                         scope.execution.phase == evade_rules::Phase::Active;
        }
    }

    bool Update(RE::Actor* actor, const shield_defender::Observation& threat, float delta, bool live, bool clear) {
        using Phase = evade_rules::Phase;
        using Direction = yliwf::testing::EvadeDirection;
        if (!actor || actor->GetFormID() != scope.actor)
            return false;
        scope.cooldown = std::max(0.0f, scope.cooldown - delta);
        auto& execution = scope.execution;
        if (execution.phase == Phase::Pending || execution.phase == Phase::Active) {
            const auto previous = execution.phase;
            bool dodging = false;
            const bool readable = actor->GetGraphVariableBool("bIsDodging", dodging);
            execution.Observe(readable, dodging, delta);
            if (previous == Phase::Pending && execution.Started())
                ++scope.observation.observed;
            scope.observation.displacement = actor->GetPosition().GetDistance(scope.origin);
            if (execution.phase == Phase::Idle) {
                const auto position = actor->GetPosition();
                if (evade_rules::UsefulDisplacement(position.x - scope.origin.x, position.y - scope.origin.y,
                                                    scope.direction.x, scope.direction.y))
                    ++scope.observation.completed;
                else
                    ++scope.observation.rejected;
                scope.cooldown = 2;
            } else if (execution.phase == Phase::Disabled)
                ++scope.observation.timeouts;
        }
        scope.observation.phase = static_cast<std::uint32_t>(execution.phase);
        if (scope.closing) {
            scope.busy = execution.phase == Phase::Pending || execution.phase == Phase::Active;
            return scope.busy;
        }
        scope.costRefresh -= delta;
        if (scope.costRefresh <= 0) {
            scope.cost = scope.backend.cost ? scope.backend.cost(scope.actor) : -1;
            scope.costRefresh = .2f;
        }
        const float cost = scope.cost;
        scope.observation.available = cost >= 0 && execution.phase != Phase::Disabled;
        const bool heavy = threat.threatKind == static_cast<std::uint32_t>(defender_tactics::Kind::HeavyMelee) &&
                           threat.distance <= 320 && (threat.threatAttacking || threat.threatPreparing) &&
                           !threat.tactic.interrupt;
        const bool missile =
            threat.incomingProjectiles && threat.threatKind != static_cast<std::uint32_t>(defender_tactics::Kind::Bow);
        const bool casting = threat.threatKind == static_cast<std::uint32_t>(defender_tactics::Kind::Concentration) &&
                             threat.threatAttacking && threat.distance > 144 && threat.distance < 650;
        const bool wanted = live && scope.observation.available && scope.cooldown <= 0 && threat.targeting &&
                            (heavy || missile || casting) &&
                            evade_rules::Affordable(threat.stamina, threat.maximumStamina, cost, threat.recovering);
        scope.busy = execution.phase == Phase::Pending || execution.phase == Phase::Active;
        if (!wanted || scope.busy)
            return scope.busy;
        if (!clear)
            return scope.busy = true;
        bool dodging = false;
        if (!actor->GetGraphVariableBool("bIsDodging", dodging) || dodging || actor->IsBlocking() ||
            actor->IsAttacking())
            return false;
        const auto origin = actor->GetPosition();
        const float angle = actor->GetAngleZ();
        const std::array directions = heavy ? std::array{Direction::Back, Direction::Left, Direction::Right}
                                            : std::array{Direction::Left, Direction::Right, Direction::Back};
        for (const auto direction : directions) {
            const float localX = direction == Direction::Left    ? -scope.backend.clearance
                                 : direction == Direction::Right ? scope.backend.clearance
                                                                 : 0;
            const float localY = direction == Direction::Back ? -scope.backend.clearance : 0;
            const RE::NiPoint3 destination{origin.x + std::cos(angle) * localX + std::sin(angle) * localY,
                                           origin.y - std::sin(angle) * localX + std::cos(angle) * localY, origin.z};
            if (!defender_movement::ClearEvade(actor, destination))
                continue;
            ++scope.observation.requests;
            if (!scope.backend.request(scope.actor, direction)) {
                ++scope.observation.rejected;
                scope.cooldown = 2;
                break;
            }
            scope.origin = origin;
            scope.direction = destination - origin;
            execution.Request();
            ++scope.observation.accepted;
            scope.observation.phase = static_cast<std::uint32_t>(execution.phase);
            return scope.busy = true;
        }
        return scope.busy = false;
    }
}
