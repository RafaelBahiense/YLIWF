#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include "CombatTestHost.h"

namespace RE {
    class Actor;
}

namespace mod::combat_tests::telemetry {
    struct DodgeSnapshot {
        std::uint32_t readable = 0, flags = 0, attached = 0;
        std::uint32_t events = 0, dodgeEvents = 0, guardEvents = 0, overwritten = 0;
        std::uint32_t samples = 0, readableSamples = 0;
        float dodgingSeconds = 0;
        char lastEvent[64]{};
        std::uint32_t notifications = 0, notificationCompleted = 0, notificationFalse = 0, notificationSuppressed = 0,
                      requestObserver = 0;
    };

    // Main thread: graph binding, variable reads and cleanup.
    void Attach(RE::Actor* actor);
    void ObserveAttacks(std::uint32_t actor, std::uint32_t token, const yliwf::testing::Host& host);
    void ObserveBashes(std::uint32_t actor, std::uint32_t token, const yliwf::testing::Host& host);
    void Sample(RE::Actor* actor, float elapsed, const char* phase, const yliwf::testing::DefenderObservation& action);
    void End();
    void Reset();
    // Engine-free snapshots; graph callbacks only capture bounded records.
    DodgeSnapshot Read();
    std::string Report(bool trace);
    bool Watching(std::uint32_t actor);
    std::uint64_t BeginNotification(std::uint32_t actor, std::string_view event, std::string_view module,
                                    std::uintptr_t offset);
    void EndNotification(std::uint64_t ticket, bool result, bool suppressed);
}
