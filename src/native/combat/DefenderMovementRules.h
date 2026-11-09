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

    inline float ApproachStep(float distance, float maximum = 384) {
        // Packages arrive within 64 units. A 160-unit standoff let the actor
        // stop 224 units away, outside the 192-unit engagement threshold.
        return std::isfinite(distance) && distance > 192 ? std::min(distance - 112, maximum) : 0;
    }

    inline Point AdvanceLateralStep(float distance, float forwardMaximum, float lateralMaximum) {
        if (!std::isfinite(distance) || !std::isfinite(forwardMaximum) || !std::isfinite(lateralMaximum) ||
            forwardMaximum <= 0 || lateralMaximum < 0)
            return {};
        const float forward = ApproachStep(distance, std::min(forwardMaximum, 384.0f));
        // A perpendicular-only step increases distance from a stationary caster.
        // Keep the lateral component smaller than the forward approach.
        return {forward, std::min(lateralMaximum, forward * .5f)};
    }

    inline bool WithinBounds(Point anchor, Point destination, float radius) {
        return std::isfinite(radius) && radius > 0 && radius <= 4096 && Distance(anchor, destination) <= radius;
    }

    inline bool KeepWaypoint(bool active, bool sameIntent, bool detouring, Point actor, Point destination) {
        const float distance = Distance(actor, destination);
        return active && sameIntent && !detouring && std::isfinite(distance) && distance > 80 && distance <= 512;
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

    inline float FormationScore(Point destination, Point focus, std::span<const Point> threats) {
        const float dx = focus.x - destination.x, dy = focus.y - destination.y;
        const float focusDistance = std::hypot(dx, dy);
        if (!std::isfinite(focusDistance) || focusDistance < 1)
            return -100000;
        float nearest = 100000, widest = 0;
        for (const auto enemy : threats) {
            const float x = enemy.x - destination.x, y = enemy.y - destination.y;
            const float distance = std::hypot(x, y);
            if (!std::isfinite(distance) || distance < 1)
                return -100000;
            nearest = std::min(nearest, distance);
            const float cosine = std::clamp((dx * x + dy * y) / (focusDistance * distance), -1.0f, 1.0f);
            widest = std::max(widest, std::acos(cosine));
        }
        // Prefer separation and an arc that can be faced with a shield. A clear
        // path alone does not make a waypoint useful against several attackers.
        return std::min(nearest, 512.0f) - widest * 160;
    }
}
