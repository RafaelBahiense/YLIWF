#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mod::combat_actions::taunt_rules {
    inline constexpr float Range = 512, Duration = 4, AcknowledgeTimeout = 2, Cooldown = 12, Cost = 20;
    enum class Phase : std::uint32_t { Idle, Pending, Holding };

    struct Opportunity {
        bool live = false, threatensAlly = false, visible = false, hostile = false, recovering = false;
        float distance = -1, stamina = 0, maximumStamina = 0, healthFraction = 0;
    };

    inline bool Eligible(const Opportunity& value) {
        return value.live && value.threatensAlly && value.visible && value.hostile && !value.recovering &&
               std::isfinite(value.distance) && value.distance >= 0 && value.distance <= Range &&
               std::isfinite(value.stamina) && std::isfinite(value.maximumStamina) && value.maximumStamina > 0 &&
               value.stamina >= Cost + value.maximumStamina * .2f && std::isfinite(value.healthFraction) &&
               value.healthFraction >= .4f;
    }

    class State {
    public:
        bool Request(std::uint32_t target) {
            if (!target || phase != Phase::Idle || cooldown > 0)
                return false;
            enemy = target;
            phase = Phase::Pending;
            remaining = Duration;
            waiting = 0;
            cooldown = Cooldown;
            ++requests;
            return true;
        }

        void Observe(float elapsed, bool valid, bool effectPresent, bool targetsDefender) {
            if (!std::isfinite(elapsed) || elapsed < 0)
                return;
            elapsed = std::min(elapsed, .25f);
            cooldown = std::max(0.0f, cooldown - elapsed);
            if (phase == Phase::Idle)
                return;
            remaining = std::max(0.0f, remaining - elapsed);
            waiting += elapsed;
            if (!valid) {
                ++cancelled;
                Release();
            } else if (effectPresent && targetsDefender && phase == Phase::Pending) {
                phase = Phase::Holding;
                ++observed;
            }
            if (phase == Phase::Pending && waiting >= AcknowledgeTimeout) {
                ++unobserved;
                Release();
            } else if (phase == Phase::Holding && (!effectPresent || remaining <= 0)) {
                ++released;
                Release();
            }
        }

        void Release() {
            phase = Phase::Idle;
            enemy = 0;
            remaining = waiting = 0;
        }

        Phase phase = Phase::Idle;
        std::uint32_t enemy = 0, requests = 0, observed = 0, unobserved = 0, released = 0, cancelled = 0;
        float remaining = 0, waiting = 0, cooldown = 0;
    };

    constexpr bool RejectTarget(std::uint32_t attacker, std::uint32_t candidate, std::uint32_t enemy,
                                std::uint32_t defender, bool fresh) {
        return fresh && attacker && candidate && enemy && defender && attacker == enemy && candidate != defender;
    }
}
