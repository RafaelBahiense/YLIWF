#pragma once

#include <atomic>

namespace RE {
    class Actor;
}

namespace mod::combat_orders {
    inline std::atomic_bool Enabled{false}, Ready{false};
    inline std::atomic_size_t Affected{0}, Unavailable{0};
    inline std::atomic_uint64_t SuppressedUpdates{0}, Reentries{0};
    void Configure();
    void Set(bool enabled);
    void Tick();
    void Loaded();
    void Invalidate();
    void ActorLoaded(RE::Actor* actor);
}
