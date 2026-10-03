#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mod::follower_view {
    struct Space {
        std::uint32_t cell = 0, world = 0;
        bool interior = false, loaded = false;
    };

    inline bool Comparable(const Space& actor, const Space& player) {
        if (!actor.loaded || !player.loaded || !actor.cell || !player.cell) return false;
        if (actor.cell == player.cell) return true;
        return !actor.interior && !player.interior && actor.world && actor.world == player.world;
    }

    inline std::optional<double> DistanceMeters(const Space& actor, const Space& player,
        const std::array<float, 3>& position, const std::array<float, 3>& playerPosition) {
        if (!Comparable(actor, player)) return {};
        for (std::size_t i = 0; i < position.size(); ++i)
            if (!std::isfinite(position[i]) || !std::isfinite(playerPosition[i])) return {};
        // Approximate Skyrim scale: about 70 world units per metre.
        return std::hypot(static_cast<double>(position[0]) - playerPosition[0],
            static_cast<double>(position[1]) - playerPosition[1],
            static_cast<double>(position[2]) - playerPosition[2]) / 70.0;
    }
}
