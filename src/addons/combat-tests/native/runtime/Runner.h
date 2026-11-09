#pragma once

#include "CombatTestHost.h"
#include "../scenarios/DefenderSuite.h"
#include "../scenarios/Encounters.h"
#include "../testing/RunPlan.h"
#include "../testing/Selection.h"
#include "../telemetry/DefenseMetrics.h"
#include "../telemetry/EngagementMetrics.h"
#include "../telemetry/ChallengeComparison.h"
#include "../fixtures/NaturalOpponentRules.h"
#include "../arena/Travel.h"
#include "../diagnostics/SpellDiagnostic.h"
#include "../interference/Control.h"
#include "../interference/UND.h"
#include "../telemetry/Dodge.h"

namespace yliwf::testing {
    enum class Phase : std::uint32_t { Idle, Preparing, Countdown, Running, Restoring, Complete, Aborted, Failed };

    enum Experiment : std::uint32_t { DodgeBudget = 1, NativeBash = 2, NativePowerBash = 4 };

    struct Experiments {
        std::uint32_t flags = DodgeBudget | NativeBash | NativePowerBash;
        float reserveFraction = .2f, dodgeCooldown = 2;
    };

    struct TestPhase {
        std::uint32_t index = 0, count = 0, entered = 0, completed = 0;
        float elapsed = 0, duration = 0;
        char name[96]{};
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
        std::uint32_t suiteRepeats = mod::combat_tests::testing::DefaultRepeats;
        std::uint32_t suiteSelection =
            mod::combat_tests::suite::Catalog.Tagged(mod::combat_tests::suite::tags::Validation);
        Experiments experiments;
        DefenderTuning tuning;
        char tuningStatus[192]{};
        char opponent[256]{};
        mod::combat_tests::metrics::Snapshot metrics;
        TestPhase testPhase;
        std::uint32_t expandVariations = 0;

        mod::combat_tests::testing::RunOptions SuiteOptions() const {
            return {suiteRepeats, suiteSelection, expandVariations != 0};
        }

        mod::combat_tests::testing::RunPlan SuitePlan() const {
            return suite ? mod::combat_tests::testing::RunPlan{mod::combat_tests::suite::Catalog, SuiteOptions()}
                         : mod::combat_tests::testing::RunPlan{
                               mod::test_rules::EncounterCatalog(encounter),
                               {requestedRuns,
                                mod::combat_tests::testing::CaseMask(mod::test_rules::Scenarios[encounter].caseID),
                                false}};
        }
    };

    struct RunResult {
        mod::combat_tests::natural::Outcome encounterOutcome = mod::combat_tests::natural::Outcome::NotMeasured;
        Phase phase = Phase::Idle;
        float elapsed = 0, damage = 0;
        std::uint32_t attacks = 0, blocks = 0, bashes = 0, incoming = 0, blocked = 0, healing = 0;
        char report[8192]{};
        mod::combat_tests::suite::Verdict verdict = mod::combat_tests::suite::Verdict::NotAssessed;
        std::uint32_t powerHits = 0, dodges = 0, attackHandoffs = 0, reactionWaits = 0;
        mod::combat_tests::metrics::Snapshot metrics;
        mod::combat_tests::engagement::Metrics engagement;
        mod::combat_tests::comparison::Setup setup;
        std::uint32_t challengeEnabled = 1, comparisonEligible = 0, allyDefeated = 0;
        float allyDamage = 0, allyExposure = 0, firstIntervention = -1;
        std::uint32_t fixtureDriftSamples = 0, casterBashRequests = 0, casterBashCompleted = 0;
        std::uint32_t evadeRequests = 0, evadeCompleted = 0, evadeTimeouts = 0;
        std::array<TestPhase, 16> phases{};
        std::uint32_t phaseCount = 0;
    };

    struct API {
        std::uint32_t size = sizeof(API), version = Version;
        std::uint32_t (*configure)(const Host*) = nullptr;
        std::uint32_t (*read)(Snapshot*) = nullptr;
        std::uint32_t (*start)(std::uint32_t actor) = nullptr;
        void (*abort)() = nullptr;
        std::uint32_t (*recheckRecovery)() = nullptr;
        std::uint32_t (*readTravel)(mod::combat_tests::travel::Snapshot*) = nullptr;
        std::uint32_t (*travel)(bool returnToOrigin) = nullptr;
        std::uint32_t (*startEncounter)(std::uint32_t actor, std::uint32_t encounter) = nullptr;
        std::uint32_t (*startSeries)(std::uint32_t actor, std::uint32_t encounter, std::uint32_t runs) = nullptr;
        std::uint32_t (*readResult)(std::uint32_t index, RunResult*) = nullptr;
        std::uint32_t (*setInterruptionBypass)(bool enabled) = nullptr;
        std::uint32_t (*setDodgeBypass)(bool enabled) = nullptr;
        std::uint32_t (*startDefenderSuite)(std::uint32_t actor,
                                            mod::combat_tests::testing::RunOptions options) = nullptr;
        std::uint32_t (*setExperiments)(const Experiments*) = nullptr;
        std::uint32_t (*setTuning)(const DefenderTuning*) = nullptr;
        std::uint32_t (*saveTuning)() = nullptr;
        std::uint32_t (*reloadTuning)() = nullptr;
        std::uint32_t (*rollOpponent)() = nullptr;
        std::uint32_t (*startSpellDiagnostic)(std::uint32_t actor) = nullptr;
        std::uint32_t (*readSpellDiagnostic)(mod::combat_tests::spell_diagnostic::Snapshot*) = nullptr;
        void (*abortSpellDiagnostic)() = nullptr;
    };

    static_assert(sizeof(Snapshot) == 9540 + sizeof(Experiments) + sizeof(DefenderTuning) + 192 + 256 +
                                          sizeof(mod::combat_tests::metrics::Snapshot) + sizeof(TestPhase) + 4 &&
                  sizeof(API) == 176);
}

namespace mod::combat_tests {
    const yliwf::testing::API* RunnerAPI();
}
