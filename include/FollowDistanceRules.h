#pragma once
#include <cstdint>

namespace mod::follow_distance {
    // 0..2: individual presets; 3..5: presets applied by the party control.
    // Both encode an actual distance so actor preferences survive game saves,
    // even when the user has not saved the INI default for future followers.
    struct Choice { std::int32_t preset = 1; bool individual = false; };
    constexpr bool ValidPreset(std::int32_t value) { return value >= 0 && value <= 2; }
    constexpr bool ValidRank(std::int32_t value) { return value >= 0 && value <= 5; }
    constexpr Choice Decode(std::int32_t rank, std::int32_t fallback) {
        return ValidRank(rank) ? Choice{rank % 3, rank < 3} : Choice{ValidPreset(fallback) ? fallback : 1, false};
    }
    constexpr std::int8_t Encode(std::int32_t preset, bool individual) {
        return static_cast<std::int8_t>(preset + (individual ? 0 : 3));
    }
}
