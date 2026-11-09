#pragma once
#include <cstdint>

namespace yliwf::testing {
    struct Host;
}

namespace mod::combat_tests::scene_control {
    bool Register(const yliwf::testing::Host& host);
    void Install();
    bool Begin(std::uint32_t actor, std::uint32_t token, bool healer);
    bool StartCombat(std::uint32_t actor, std::uint32_t token);
    void Refresh();
    void End(std::uint32_t actor, std::uint32_t token);
    // Game-state reset: no engine calls or package replay.
    void Reset();
    std::uint32_t Requested(std::uint32_t actor);
}
