#pragma once
#include <cstdint>

namespace mod::combat_tests::fixture_protection {
    struct Snapshot {
        std::uint32_t blocked = 0;
        float damage = 0;
        std::uint32_t publicWrites = 0, combatWrites = 0, otherWrites = 0;
    };

    void Install();
    bool Begin(std::uint32_t primary);
    bool Add(std::uint32_t secondary);
    bool Protected(std::uint32_t actor);
    Snapshot Read();
    const char* Status();
    bool ProbeCombat(std::uint32_t actor, float damage);
    void End();
    void Reset();
}
