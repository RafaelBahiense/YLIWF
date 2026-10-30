#pragma once

#include "CombatTestHost.h"
#include "Travel.h"

namespace yliwf::testing {
    enum class Phase : std::uint32_t { Idle, Preparing, Countdown, Running, Restoring, Complete, Aborted, Failed };

    struct Snapshot {
        std::uint32_t size = sizeof(Snapshot);
        Phase phase = Phase::Idle;
        std::uint32_t active = 0, subject = 0, blocks = 0, bashes = 0, attacks = 0;
        float elapsed = 0, duration = 60, damage = 0;
        char status[256]{}, report[2048]{};
        std::uint32_t mode = 0;
        std::uint32_t canStart = 0, recoveryPending = 0;
        char startBlockedReason[256]{};
        std::uint32_t canRecheckRecovery = 0;
        std::uint32_t encounter = 0, healingEffects = 0, selfHealingEffects = 0, injuryWaves = 0;
        float playerHealthGain = 0, injuredSeconds = 0, firstHeal = -1;
    };

    struct API {
        std::uint32_t size = sizeof(API), version = Version;
        std::uint32_t (*configure)(const Host*) = nullptr;
        std::uint32_t (*read)(Snapshot*) = nullptr;
        std::uint32_t (*start)(std::uint32_t actor) = nullptr;
        void (*abort)() = nullptr;
        std::uint32_t (*startConfigured)(std::uint32_t actor, std::uint32_t mode) = nullptr;
        std::uint32_t (*recheckRecovery)() = nullptr;
        std::uint32_t (*readTravel)(mod::combat_tests::travel::Snapshot*) = nullptr;
        std::uint32_t (*travel)(bool returnToOrigin) = nullptr;
        std::uint32_t (*startEncounter)(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode) = nullptr;
    };

    static_assert(sizeof(Snapshot) == 2644 && sizeof(API) == 80);
}

namespace mod::combat_tests {
    const yliwf::testing::API* RunnerAPI();
}
