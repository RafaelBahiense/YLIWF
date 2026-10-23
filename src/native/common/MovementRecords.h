#pragma once

#include <cstddef>
#include <cstdint>

namespace mod::movement_records {
    inline constexpr std::size_t Distances = 3;
    inline constexpr std::size_t Formations = 16;
    inline constexpr std::size_t Passive = 4;
    inline constexpr std::size_t StandDown = Passive + 32;

    constexpr std::uint32_t Distance(std::size_t index) {
        return index < Distances ? 0x990u + static_cast<std::uint32_t>(index) : 0;
    }

    constexpr std::uint32_t Formation(std::size_t index) {
        return index < Formations ? (index < 8 ? 0x9A0u : 0x9C0u) + static_cast<std::uint32_t>(index % 8) : 0;
    }

    constexpr std::uint32_t PassiveOrAvoidance(std::size_t index) {
        return index < StandDown ? (index < Passive ? 0xA11u + static_cast<std::uint32_t>(index)
                                                    : 0xA20u + static_cast<std::uint32_t>(index - Passive))
                                 : 0;
    }
}
