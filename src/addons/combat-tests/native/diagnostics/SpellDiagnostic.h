#pragma once
#include <cstdint>

namespace yliwf::testing {
    struct Host;
}

namespace mod::combat_tests::spell_diagnostic {
    struct Snapshot {
        std::uint32_t size = sizeof(Snapshot), active = 0, completed = 0;
        char status[256]{}, report[8192]{}, path[512]{};
    };

    // Runner mutex protects state; Tick/Reset alone access the engine, on the game thread.
    bool Busy();
    bool Request(std::uint32_t caster);
    void Abort();
    void Tick(float elapsed, const yliwf::testing::Host& host);
    void Reset();
    Snapshot Read();
}
