#pragma once

#include <atomic>

namespace mod::combat_orders {
    inline std::atomic_bool Enabled{false}, Ready{false};
    inline std::atomic_size_t Affected{0}, Unavailable{0};
    void Configure();
    void Set(bool enabled);
    void Tick();
    void Loaded();
    void Invalidate();
}
