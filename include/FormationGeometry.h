#pragma once

#include "FormationPath.h"
#include "FormationRules.h"
#include "FormationStyle.h"
#include <algorithm>
#include <array>
#include <optional>

namespace mod::formation_geometry {
    using formation_rules::Point;
    inline constexpr int Slots = formation_rules::Slots;

    struct SlotOffset {
        float behind;
        float side = 0;
    };

    using Layout = std::array<SlotOffset, Slots>;
    using Destinations = std::array<std::optional<Point>, Slots>;

    constexpr float Gap(int preset) {
        return preset == 0 ? 128.0f : preset == 2 ? 512.0f : 256.0f;
    }

    constexpr Layout Column(const std::array<int, Slots>& presets) {
        Layout result{};
        float behind = 0;
        for (int slot = 0; slot < Slots; ++slot) {
            behind += Gap(presets[slot]);
            result[slot] = {behind};
        }
        return result;
    }

    constexpr Layout Spread(const std::array<int, Slots>& presets, bool wedge) {
        Layout result{};
        // The player occupies the origin; every follower is on a flank.
        float behind = 0, left = 0, right = 0;
        for (int slot = 0; slot < Slots; ++slot) {
            const float spacing = slot >= 2 ? std::max(Gap(presets[slot - 2]), Gap(presets[slot])) : Gap(presets[slot]);
            if (slot % 2 == 0) {
                if (wedge)
                    behind += std::max(Gap(presets[slot]), Gap(presets[std::min(slot + 1, Slots - 1)]));
                left += spacing;
                result[slot] = {behind, -left};
            } else {
                right += spacing;
                result[slot] = {behind, right};
            }
        }
        return result;
    }

    constexpr Layout Arrange(Style style, const std::array<int, Slots>& presets) {
        switch (style) {
            case Style::Column:
                return Column(presets);
            case Style::Line:
                return Spread(presets, false);
            case Style::Wedge:
                return Spread(presets, true);
            case Style::Off:
                return {};
        }
        return {};
    }

    // Geometry produces destinations; the engine integration decides how to reach them.
    inline Destinations Project(const formation_rules::Path& route, const Layout& layout) {
        Destinations result{};
        const auto heading = route.Heading();
        for (int slot = 0; slot < Slots; ++slot) {
            result[slot] = route.Behind(layout[slot].behind);
            if (result[slot] && layout[slot].side != 0) {
                if (!heading) {
                    result[slot].reset();
                    continue;
                }
                // Right vector of the movement heading; player-facing rotation is irrelevant.
                result[slot]->x += heading->y * layout[slot].side;
                result[slot]->y -= heading->x * layout[slot].side;
            }
        }
        return result;
    }
}
