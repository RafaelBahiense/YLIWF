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
        bool preCallbacks = false;
        std::uint32_t preOutgoing = 0, lastCandidate = 0;
        std::int32_t active = -1, controllerHittable = -1;
    };

    void Configure();
    // Main thread. Bounded cache; hit callbacks only update tracked counters.
    Observation Observe(RE::Actor* actor);
    void Clear();
}
