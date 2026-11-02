#pragma once
#include <algorithm>
#include <cmath>
#include <span>

namespace mod::defender_movement_rules {
    struct Point {
        float x = 0, y = 0;
    };

    inline float Distance(Point a, Point b) {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    inline float ApproachStep(float distance) {
        // Packages arrive within 64 units. A 160-unit standoff let the actor
        // stop 224 units away, outside the 192-unit engagement threshold.
        return std::isfinite(distance) && distance > 192 ? std::min(distance - 112, 384.0f) : 0;
    }

    inline bool WithinBounds(Point anchor, Point destination, float radius) {
        return std::isfinite(radius) && radius > 0 && radius <= 4096 && Distance(anchor, destination) <= radius;
    }

    inline bool ClearStep(Point origin, Point destination, std::span<const Point> threats, bool retreat,
                          bool detour = false) {
        const float length = Distance(origin, destination);
        if (!std::isfinite(length) || length < 32 || length > 512)
            return false;
        float oldNearest = 100000, newNearest = 100000;
        for (const auto threat : threats) {
            const float before = Distance(origin, threat), after = Distance(destination, threat);
            if (!std::isfinite(before) || !std::isfinite(after))
                return false;
            oldNearest = std::min(oldNearest, before);
            newNearest = std::min(newNearest, after);
            const float projection = std::clamp(((threat.x - origin.x) * (destination.x - origin.x) +
                                                 (threat.y - origin.y) * (destination.y - origin.y)) /
                                                    (length * length),
                                                0.0f, 1.0f);
            const Point closest{origin.x + projection * (destination.x - origin.x),
                                origin.y + projection * (destination.y - origin.y)};
            if (Distance(closest, threat) < std::min(96.0f, before * .8f))
                return false;
        }
        return !retreat || (detour ? newNearest >= oldNearest - 16 : newNearest > oldNearest + 32);
    }
}
