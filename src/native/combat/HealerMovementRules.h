#pragma once

#include "formations/FormationPath.h"
#include "combat/HealingRules.h"
#include <limits>

namespace mod::healing_rules {
    using formation_rules::Point;

    inline float SpatialDistance(Point a, Point b) {
        return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    inline float NearestThreat(Point position, std::span<const Point> threats) {
        float nearest = std::numeric_limits<float>::max();
        for (const auto threat : threats)
            nearest = std::min(nearest, SpatialDistance(position, threat));
        return nearest;
    }

    inline bool EscapeDirection(Point origin, Point candidate, std::span<const Point> threats) {
        if (threats.empty())
            return true;
        const float length = SpatialDistance(origin, candidate);
        if (!std::isfinite(length) || length <= 1)
            return false;
        const float fraction = std::min(1.0f, 96.0f / length);
        const Point firstStep{origin.x + (candidate.x - origin.x) * fraction,
                              origin.y + (candidate.y - origin.y) * fraction,
                              origin.z + (candidate.z - origin.z) * fraction};
        // A safe endpoint beyond an enemy is not a safe escape direction.
        return NearestThreat(firstStep, threats) > NearestThreat(origin, threats) + 8;
    }

    inline std::optional<float> MovementScore(Point origin, Point ally, Point candidate, float range,
                                              std::span<const Point> threats, float clearance = ThreatDistance) {
        const auto distance = SpatialDistance(candidate, ally);
        const auto threatDistance = NearestThreat(candidate, threats);
        if (!std::isfinite(distance) || !std::isfinite(range) || range <= 0 || distance > range ||
            threatDistance < clearance)
            return {};
        return SpatialDistance(origin, candidate) + std::max(0.0f, SafeDistance - threatDistance) * 4;
    }
}
