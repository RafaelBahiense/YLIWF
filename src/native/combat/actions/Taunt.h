#pragma once
#include "TauntRules.h"
#include <cstdint>

namespace RE {
    class Actor;
}

namespace mod::shield_defender {
    struct Observation;
}

namespace mod::combat_actions::taunt {
    struct Observation {
        taunt_rules::State state;
        bool available = false, effectPresent = false;
        std::uint32_t targetRejections = 0;
        std::uint32_t spell = 0;
    };

    void Configure();
    void Begin(std::uint32_t actor, std::uint32_t token);
    void Update(RE::Actor* actor, const shield_defender::Observation& defender, float elapsed, bool live);
    void End(std::uint32_t actor, std::uint32_t token);
    void Clear();
    Observation Inspect(std::uint32_t actor);
}
