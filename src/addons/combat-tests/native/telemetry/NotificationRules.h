#pragma once
#include <cstddef>
#include <cstdint>

namespace mod::combat_tests::telemetry::notification_rules {
    inline constexpr std::size_t Capacity = 1024;
    inline constexpr unsigned Shift = 10;
    static_assert((1u << Shift) == Capacity);

    constexpr std::uint64_t Ticket(std::uint64_t sequence, std::size_t slot) {
        return (sequence << Shift) | (slot & (Capacity - 1));
    }

    constexpr std::size_t Slot(std::uint64_t ticket) {
        return ticket & (Capacity - 1);
    }

    constexpr bool Current(std::uint64_t ticket, std::uint64_t generationStart) {
        return (ticket >> Shift) > generationStart;
    }
}
