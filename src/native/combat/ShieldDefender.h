#pragma once

#include <cstdint>
#include <string>

namespace RE {
    class Actor;
}

namespace mod::shield_defender {
    void Configure();
    void Tick(float elapsed);
    bool NeedsObservation();
    void Invalidate();
    void UpdateCombatStyle(RE::Actor* actor);
    std::string Status(std::uint32_t actor);
}
