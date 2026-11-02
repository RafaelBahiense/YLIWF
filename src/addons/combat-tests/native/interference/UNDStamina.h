#pragma once
#include <cstdint>

namespace RE {
    class Actor;
}

namespace mod::combat_tests::interference::und {
    // Main thread only; private ABI of the fingerprint-verified UND profile.
    bool StaminaCost(RE::Actor* actor, std::uintptr_t base, float& cost);
}
