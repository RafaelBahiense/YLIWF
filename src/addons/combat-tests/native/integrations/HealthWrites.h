#pragma once
#include "HealthWriteProfile.h"

namespace RE {
    class Actor;
}

namespace mod::combat_tests::health_writes {
    using Path = health_write_profile::Path;
    using Filter = float (*)(std::uint32_t actor, bool damage, bool health, float delta, Path path);
    void Install(Filter filter);
    bool Ready();
    const char* Status();
    // Invoke the installed combat call's actual destination, including its relay.
    bool ProbeCombat(RE::Actor* actor, float delta);
}
