#pragma once
#include <cstdint>
#include <span>

namespace RE {
    class Actor;
}

namespace mod::projectile_observation {
    struct Shot {
        std::uint32_t shooter = 0;
        float seconds = -1;
    };

    struct Snapshot {
        bool readable = false;
        std::uint32_t examined = 0;
        Shot incoming[128]{};
        std::uint32_t count = 0;
    };

    // Main-thread snapshot; a full bounded scan is required before claiming a gap.
    Snapshot Read(RE::Actor* actor);
}
