#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RE {
    class Actor;
    class TESNPC;
    class SpellItem;
}

namespace mod::combat_tests::fixture_spells {
    struct Spell {
        std::uint32_t id = 0;
        std::string name;
    };

    struct Result {
        bool owned = false, bounded = true;
        std::vector<Spell> removed, remaining;
    };

    // Main-thread operations on the runner's generated records only.
    void Configure();
    bool DetachPassives(RE::TESNPC* destination, const RE::TESNPC* source);
    Result RestrictBase(RE::TESNPC* base, RE::SpellItem* expected);
    Result Restrict(RE::Actor* actor, RE::TESNPC* base, RE::SpellItem* expected);
    Result Inspect(RE::Actor* actor, RE::TESNPC* base, RE::SpellItem* expected);
}
