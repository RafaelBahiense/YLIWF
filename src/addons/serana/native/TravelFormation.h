#pragma once

#include <cstdint>

namespace RE {
    class Actor;
}

namespace mod::serana_formation {
    void Configure();
    bool PermittedPackage(RE::Actor* actor, bool standDown = false);
    bool ApplyStandDown(RE::Actor* actor, std::uint32_t package);
    bool Apply(RE::Actor* actor, std::uint32_t package);
    void Release(RE::Actor* actor);
}
