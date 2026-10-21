#pragma once

#include <cstdint>
#include <string>

namespace RE {
    class Actor;
}

namespace mod::package_control {
    struct Snapshot {
        bool enabled = false, active = false;
        std::uint32_t previousPackage = 0, savedInterrupt = 0, requestedPackage = 0;
        std::string status;
    };

    void Install();
    void Tick();
    bool HasWork();
    void Release(RE::Actor* actor);
    void Suspend(RE::Actor* actor);
    void Withdraw(std::uint32_t actorID);
    void Withdraw(std::uint32_t actorID);
    void Invalidate();
    Snapshot Inspect(RE::Actor* actor);
    void Request(std::uint32_t actorID, std::uint32_t aliasID, bool enabled, std::uint64_t generation);
}
