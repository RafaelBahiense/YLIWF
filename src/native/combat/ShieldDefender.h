#pragma once

#include <cstdint>
#include <string>

namespace RE {
    class Actor;
}

namespace mod::shield_defender {
    struct Observation {
        bool available = false, requested = false, owned = false, yielded = false;
        bool incoming = false, committed = false, recovering = false, counter = false, targeting = false;
        std::uint32_t stance = 0, style = 0, threat = 0;
        float distance = -1, heading = 0, stamina = 0, maximumStamina = 0, guardRemaining = 0, age = -1;
        std::string reason;
    };

    void Configure();
    void Tick(float elapsed);
    bool NeedsObservation();
    void Invalidate();
    void UpdateCombatStyle(RE::Actor* actor);
    std::string Status(std::uint32_t actor);
    Observation Inspect(std::uint32_t actor);
}
