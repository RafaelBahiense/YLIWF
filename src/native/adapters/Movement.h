#pragma once

#include "adapters/MovementRules.h"
#include <functional>
#include <vector>

namespace RE {
    class Actor;
}

namespace mod::adapter_movement {
    using Inspect = std::function<adapter_movement_rules::Snapshot(std::uint32_t, std::uint32_t)>;
    void Configure();
    bool KnownPackage(std::uint32_t package);
    std::uint32_t IsPackage(std::uint32_t package);
    bool Permitted(std::uint32_t actor, yliwf::sdk::Movement feature, const Inspect& inspect);
    bool SetDistance(std::uint32_t actor, int preset, const Inspect& inspect);
    bool SetPackage(std::uint32_t actor, yliwf::sdk::Movement feature, std::uint32_t package, const Inspect& inspect);
    void Refresh(std::uint32_t actor, const Inspect& inspect);
    void Suspend(std::uint32_t actor);
    void Forget(std::uint32_t actor);
    bool ClearSaved(std::uint32_t actor);
    std::vector<std::uint32_t> Actors();
    bool HasWork();
    bool Tracks(std::uint32_t actor);
    void Invalidate();
}
