#pragma once

#include <array>
#include <cstdint>

namespace mod::formation_rules {
    // Skyrim.esm: LocTypeCity, LocTypeTown, LocTypeSettlement, LocTypeOrcStronghold.
    inline constexpr std::array<std::uint32_t, 4> SettlementKeywordForms{0x13168, 0x13166, 0x13167, 0x130E9};

    template <class Location, class Predicate>
    bool InSettlement(const Location* location, Predicate matches) {
        // Nested locations inherit the travel restriction. A malformed ancestry
        // chain must not hang the game or enable movement through an unknown area.
        for (int depth = 0; location && depth < 64; ++depth) {
            if (matches(location))
                return true;
            location = location->parentLoc;
        }
        return location != nullptr;
    }
}
