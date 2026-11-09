#pragma once
#include <cmath>
#include <cstdint>

namespace mod::combat_actions::evade_rules {
    enum class Phase : std::uint32_t { Idle, Pending, Active, Disabled };

    inline bool Affordable(float stamina, float maximum, float cost, bool recovering) {
        return !recovering && std::isfinite(stamina) && std::isfinite(maximum) && std::isfinite(cost) && maximum > 0 &&
               cost >= 0 && stamina >= cost + maximum * .2f;
    }

    inline bool UsefulDisplacement(float x, float y, float directionX, float directionY) {
        const float distance = std::hypot(x, y);
        const float length = std::hypot(directionX, directionY);
        return std::isfinite(distance) && std::isfinite(length) && length > 0 && distance >= 48 && distance <= 512 &&
               (x * directionX + y * directionY) / length >= distance * .5f;
    }

    class Execution {
    public:
        Phase phase = Phase::Idle;
        float elapsed = 0;

        bool Started() const {
            return observed_;
        }

        void Request() {
            phase = Phase::Pending;
            elapsed = 0;
            observed_ = false;
        }

        // Completion needs the actual dodge state to start and end. Displacement
        // is evaluated separately; an animation in place is not useful evasion.
        void Observe(bool readable, bool dodging, float delta) {
            if (phase != Phase::Pending && phase != Phase::Active)
                return;
            if (std::isfinite(delta) && delta > 0)
                elapsed += delta;
            if (readable && dodging) {
                phase = Phase::Active;
                observed_ = true;
            } else if (readable && observed_) {
                phase = Phase::Idle;
                return;
            }
            if (elapsed > (observed_ ? 2.0f : .6f))
                phase = Phase::Disabled;
        }

    private:
        bool observed_ = false;
    };
}
