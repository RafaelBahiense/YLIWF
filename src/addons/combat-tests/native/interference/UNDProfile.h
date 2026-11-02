#pragma once
#include <array>
#include <bit>
#include <cstdint>

namespace mod::combat_tests::interference::und::profile {
    constexpr bool Reachable(std::uintptr_t site, std::uintptr_t destination) {
        const auto next = site + 5;
        return destination >= next ? destination - next <= 0x7FFFFFFF : next - destination <= 0x80000000;
    }

    constexpr bool DirectCall(const std::array<std::uint8_t, 5>& bytes, std::uintptr_t site,
                              std::uintptr_t expectedTarget) {
        const auto raw = std::uint32_t(bytes[1]) | (std::uint32_t(bytes[2]) << 8) | (std::uint32_t(bytes[3]) << 16) |
                         (std::uint32_t(bytes[4]) << 24);
        const auto displacement = std::bit_cast<std::int32_t>(raw);
        return bytes[0] == 0xE8 && site + 5 + displacement == expectedTarget;
    }
}
