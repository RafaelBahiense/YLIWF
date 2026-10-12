#pragma once

#include "FormationGeometry.h"

namespace RE {
    class Actor;
}

namespace mod::formation_ground {
    void Clear();
    void Refresh(RE::Actor* player);
    std::optional<formation_rules::Point> Anchor(formation_rules::Point player);
    formation_geometry::Destinations Adjust(const formation_geometry::Destinations& desired);
    std::size_t TriangleCount();
}
