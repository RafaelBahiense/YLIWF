#pragma once

#include "CombatTestHost.h"
#include "DefenderSuite.h"
#include "DefenseMetrics.h"
#include "Travel.h"
#include "interference/Control.h"
#include "interference/UND.h"
#include "telemetry/Dodge.h"

namespace yliwf::testing {
    enum class Phase : std::uint32_t { Idle, Preparing, Countdown, Running, Restoring, Complete, Aborted, Failed };

    enum Experiment : std::uint32_t { DodgeBudget = 1, NativeBash = 2, NativePowerBash = 4 };

    struct Experiments {
        std::uint32_t flags = 0;
        float reserveFraction = .2f, dodgeCooldown = 2;
    };

    struct Snapshot {
        std::uint32_t size = sizeof(Snapshot);
        Phase phase = Phase::Idle;
        std::uint32_t active = 0, subject = 0, blocks = 0, bashes = 0, attacks = 0;
        float elapsed = 0, duration = 60, damage = 0;
        char status[256]{}, report[8192]{};
        std::uint32_t mode = 0;
        std::uint32_t canStart = 0, recoveryPending = 0;
        char startBlockedReason[256]{};
        std::uint32_t canRecheckRecovery = 0;
        std::uint32_t encounter = 0, healingEffects = 0, selfHealingEffects = 0, injuryWaves = 0;
        float playerHealthGain = 0, injuredSeconds = 0, firstHeal = -1;
        std::uint32_t run = 1, requestedRuns = 1, finishedRuns = 0;
        char reportPath[512]{};
        mod::combat_tests::interference::Snapshot interference;
        std::uint32_t bypassInterruptions = 0;
        mod::combat_tests::interference::und::Snapshot und;
        std::uint32_t bypassDodging = 0;
        mod::combat_tests::telemetry::DodgeSnapshot dodgeTelemetry;
        std::uint32_t suite = 0;
        std::uint32_t suiteRepeats = mod::combat_tests::suite::DefaultRepeats;
        Experiments experiments;
        mod::combat_tests::metrics::Snapshot metrics;
    };

    struct RunResult {
        Phase phase = Phase::Idle;
        float elapsed = 0, damage = 0;
        std::uint32_t attacks = 0, blocks = 0, bashes = 0, incoming = 0, blocked = 0, healing = 0;
        char report[8192]{};
        mod::combat_tests::suite::Verdict verdict = mod::combat_tests::suite::Verdict::NotAssessed;
        std::uint32_t powerHits = 0, dodges = 0, attackHandoffs = 0, reactionWaits = 0;
        mod::combat_tests::metrics::Snapshot metrics;
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
        std::uint32_t (*startSeries)(std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode,
                                     std::uint32_t runs) = nullptr;
        std::uint32_t (*readResult)(std::uint32_t index, RunResult*) = nullptr;
        std::uint32_t (*setInterruptionBypass)(bool enabled) = nullptr;
        std::uint32_t (*setDodgeBypass)(bool enabled) = nullptr;
        std::uint32_t (*startDefenderSuite)(std::uint32_t actor, std::uint32_t repeats) = nullptr;
        std::uint32_t (*setExperiments)(const Experiments*) = nullptr;
    };

    static_assert(sizeof(Snapshot) == 9532 + sizeof(Experiments) + sizeof(mod::combat_tests::metrics::Snapshot) &&
                  sizeof(API) == 128);
}

namespace mod::combat_tests {
    const yliwf::testing::API* RunnerAPI();
}
