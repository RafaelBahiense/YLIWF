#pragma once
#include <algorithm>
#include <cmath>

namespace mod::projectile_rules {
    struct Vector {
        float x = 0, y = 0, z = 0;
    };

    struct Approach {
        bool readable = false, incoming = false;
        float seconds = -1;
    };

    // Straight-line closest approach is a conservative short-horizon estimate,
    // not collision prediction. Gravity, homing and future shots remain unknown.
    inline Approach Observe(Vector offset, Vector velocity, float radius = 128, float horizon = 1.5f) {
        const float speed2 = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
        const float distance2 = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
        if (!std::isfinite(speed2) || !std::isfinite(distance2) || !std::isfinite(radius) || radius <= 0 ||
            !std::isfinite(horizon) || horizon <= 0)
            return {};
        if (speed2 < 1)
            return {true, distance2 <= radius * radius, 0};
        const float time = -(offset.x * velocity.x + offset.y * velocity.y + offset.z * velocity.z) / speed2;
        if (time < 0 || time > horizon)
            return {true, false, -1};
        const Vector closest{offset.x + velocity.x * time, offset.y + velocity.y * time, offset.z + velocity.z * time};
        return {true, closest.x * closest.x + closest.y * closest.y + closest.z * closest.z <= radius * radius, time};
    }

    class QuietWindow {
    public:
        bool Update(bool readable, bool danger, float elapsed) {
            if (!readable || danger || !std::isfinite(elapsed) || elapsed <= 0 || elapsed > .25f)
                quiet_ = 0;
            else
                quiet_ = std::min(.3f, quiet_ + elapsed);
            return quiet_ >= .3f;
        }

    private:
        float quiet_ = 0;
    };
}
