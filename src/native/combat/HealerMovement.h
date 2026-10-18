#pragma once

#include "formations/FormationGroundRules.h"
#include <vector>

namespace RE {
    class Actor;
}

namespace mod::healer_movement {
    using formation_rules::Point;
    enum class Purpose { Seek, CombatSeek, Retreat };
    std::vector<Point> Threats(RE::Actor* healer, bool includePlayerEnemies = false);
    std::optional<Point> PassiveDestination(RE::Actor* actor, RE::Actor* player, float followRadius,
                                            std::span<const Point> threats);
    float ThreatDistance(Point position, std::span<const Point> threats);
    std::optional<Point> Destination(RE::Actor* healer, RE::Actor* ally, float range, std::span<const Point> threats,
                                     Purpose purpose = Purpose::Seek);
    void Clear();
}
