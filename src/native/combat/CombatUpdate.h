#pragma once

#include <cstdint>

namespace RE {
    class Actor;
}

namespace mod::combat_update {
    void Install();
    void Acquire(RE::Actor* actor);
    void Release(RE::Actor* actor);
    void Clear();
    std::uint64_t TakeSuppressed(RE::Actor* actor);
}
