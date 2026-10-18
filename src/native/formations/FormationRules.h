#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace mod::formation_rules {
    inline constexpr int Slots = 8;

    // Saved faction ranks: 0..7 retain slots, 8..15 require offset cleanup
    // from the earlier preview, 16..31 select two Travel variants per slot.

    struct RequestIdentity {
        std::uint64_t session;
        std::uint32_t actor;
        int ticket;
        bool operator==(const RequestIdentity&) const = default;
    };

    constexpr bool CanExecute(RequestIdentity expected, RequestIdentity received, bool loaded, bool permitted) {
        return loaded && permitted && expected.ticket > 0 && expected == received;
    }

    constexpr bool ValidRank(int rank) {
        return rank >= 0 && rank < Slots * 4;
    }

    constexpr bool OwnsOffset(int rank) {
        return rank >= Slots && rank < Slots * 2;
    }

    constexpr bool Active(int rank) {
        return rank >= Slots * 2 && rank < Slots * 4;
    }

    constexpr std::int8_t NextTravelRank(int slot, int currentRank) {
        return static_cast<std::int8_t>(slot + (currentRank == slot + Slots * 2 ? Slots * 3 : Slots * 2));
    }

    constexpr int Slot(int rank) {
        return ValidRank(rank) ? rank % Slots : -1;
    }

    constexpr std::int8_t Rank(int slot, bool ownsOffset) {
        return static_cast<std::int8_t>(slot + (ownsOffset ? Slots : 0));
    }

    inline std::array<int, Slots> AssignSlots(const std::array<int, Slots>& preferences, int count) {
        std::array<int, Slots> result;
        result.fill(-1);
        std::array<bool, Slots> occupied{};
        // Preserve valid existing positions before placing new followers.
        for (int i = 0; i < count && i < Slots; ++i)
            if (preferences[i] >= 0 && preferences[i] < Slots && !occupied[preferences[i]]) {
                result[i] = preferences[i];
                occupied[preferences[i]] = true;
            }
        for (int i = 0; i < count && i < Slots; ++i)
            if (result[i] < 0)
                for (int slot = 0; slot < Slots; ++slot)
                    if (!occupied[slot]) {
                        result[i] = slot;
                        occupied[slot] = true;
                        break;
                    }
        return result;
    }
}
