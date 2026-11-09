#pragma once
#include <cstdint>
#include "../testing/EncounterOutcome.h"

namespace mod::combat_tests::natural {
    constexpr bool SingleActor(std::uint32_t entries, std::uint32_t count, bool npc) {
        return entries == 1 && count == 1 && npc;
    }

    using testing::Name;
    using testing::Outcome;
    using testing::Resolve;

    constexpr bool Owned(bool generated, bool handleMatches, bool baseMatches, bool tokenMatches) {
        return generated && handleMatches && baseMatches && tokenMatches;
    }
}
