#pragma once
#include "CombatTestHost.h"

namespace RE {
    class Actor;
}

namespace mod::shield_defender {
    struct Observation;
}

namespace mod::combat_actions::evade {
    struct Observation {
        std::uint32_t available = 0, phase = 0, requests = 0, accepted = 0, observed = 0, completed = 0, rejected = 0,
                      timeouts = 0;
        float displacement = 0;
    };

    bool Configure(std::uint32_t actor, std::uint32_t token, const yliwf::testing::EvadeBackend& backend);
    // Returns true while preparing or executing a chosen evasion. Guard and
    // movement must yield; dispatch waits for a clean action channel.
    bool Update(RE::Actor* actor, const shield_defender::Observation& threat, float delta, bool live, bool clear);
    bool Busy(std::uint32_t actor);
    Observation Inspect(std::uint32_t actor);
    void End(std::uint32_t actor, std::uint32_t token);
    void Reset();
}
