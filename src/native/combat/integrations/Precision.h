#pragma once
#include <cstdint>

namespace RE {
    class Actor;
}

namespace mod::precision {
    struct Observation {
        bool available = false, callbacks = false;
        float reach = 0;
        std::uint32_t outgoing = 0, incoming = 0;
    };

    void Configure();
    // Main thread. Bounded cache; hit callbacks only update tracked counters.
    Observation Observe(RE::Actor* actor);
    void Clear();
}
