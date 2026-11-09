#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace mod::combat_tests::health_write_profile {
    enum class Path : std::uint32_t { QueuedFirst, QueuedSecond, Transfer, Public, Combat, Count };

    struct Site {
        std::uint64_t id;
        std::uintptr_t offset;
        Path path;
    };

    // Verified Skyrim 1.6.1170 direct callers of the shared actor-value backend.
    inline constexpr std::uint64_t Backend = 38468;
    inline constexpr std::array Sites{Site{35084, 0x9E, Path::QueuedFirst}, Site{35084, 0x119, Path::QueuedSecond},
                                      Site{37335, 0x151, Path::Transfer}, Site{38467, 0x14, Path::Public},
                                      Site{38586, 0x70C, Path::Combat}};

    inline std::uintptr_t CallTarget(std::span<const std::uint8_t> bytes, std::uintptr_t address) {
        if (bytes.size() < 5 || bytes[0] != 0xE8 || address > std::numeric_limits<std::uintptr_t>::max() - 5)
            return 0;
        std::int32_t delta = 0;
        std::memcpy(&delta, bytes.data() + 1, sizeof(delta));
        const auto next = address + 5;
        if (delta < 0) {
            const auto distance = static_cast<std::uintptr_t>(-static_cast<std::int64_t>(delta));
            return distance <= next ? next - distance : 0;
        }
        return static_cast<std::uintptr_t>(delta) <= std::numeric_limits<std::uintptr_t>::max() - next
                   ? next + static_cast<std::uintptr_t>(delta)
                   : 0;
    }

    constexpr bool Reachable(std::uintptr_t call, std::uintptr_t destination) {
        if (call > std::numeric_limits<std::uintptr_t>::max() - 5)
            return false;
        const auto next = call + 5;
        return destination >= next ? destination - next <= 0x7FFFFFFF : next - destination <= 0x80000000;
    }
}
