#pragma once
#include "combat/HealerMovementRules.h"
#include <array>

namespace mod::combat_order_rules {
    constexpr bool Eligible(bool recruited, bool dead, bool scene, bool foreignInterrupt) {
        return recruited && !dead && !scene && !foreignInterrupt;
    }

    using formation_rules::Point;
    inline constexpr float DangerDistance = 512;
    inline constexpr float Clearance = 384;

    constexpr float FollowRadius(int preset) {
        constexpr std::array radii{192.0f, 384.0f, 768.0f};
        return radii[preset >= 0 && preset < 3 ? preset : 1];
    }

    inline bool KeepDestination(Point goal, Point player, std::span<const Point> threats, float followRadius) {
        return !threats.empty() && healing_rules::NearestThreat(goal, threats) >= Clearance &&
               healing_rules::SpatialDistance(goal, player) <= std::max(followRadius, 640.0f) + 192;
    }

    inline std::optional<float> AvoidanceScore(Point origin, Point player, Point candidate,
                                               std::span<const Point> threats, float followRadius) {
        const float distance = healing_rules::SpatialDistance(player, candidate);
        if (!std::isfinite(distance) || distance > std::max(followRadius, 640.0f) + 192 ||
            healing_rules::NearestThreat(candidate, threats) < Clearance)
            return {};
        // Do not choose a safe endpoint whose first step moves toward the pursuer.
        if (healing_rules::NearestThreat(origin, threats) < Clearance &&
            !healing_rules::EscapeDirection(origin, candidate, threats))
            return {};
        return healing_rules::SpatialDistance(origin, candidate) + std::max(0.0f, distance - followRadius) * 2;
    }
}
