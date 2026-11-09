#include "../native/runtime/Runner.h"
#include "../native/testing/EncounterOutcome.h"
#include "../native/ui/Menu.h"
#include "../native/runtime/Rules.h"
#include "../native/configuration/TuningProfile.h"
#include "AddonTabs.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

extern "C" const yliwf::menu::API* YLIWF_GetMenuAPI(std::uint32_t);

namespace {
    preview::AddonTab registration{[] { mod::combat_tests::RegisterMenu(YLIWF_GetMenuAPI(yliwf::menu::Version)); }};

    yliwf::testing::TestPhase PreviewPhase(const mod::combat_tests::testing::TestCase& test, float elapsed,
                                           std::size_t index) {
        yliwf::testing::TestPhase result;
        float start = 0;
        for (std::size_t i = 0; i < index; ++i)
            start += test.phases[i].seconds;
        result.index = static_cast<std::uint32_t>(index + 1);
        result.count = static_cast<std::uint32_t>(test.phases.size());
        result.duration = test.phases[index].seconds;
        result.elapsed = std::clamp(elapsed - start, 0.0f, result.duration);
        result.entered = elapsed >= start;
        result.completed = elapsed >= start + result.duration;
        std::snprintf(result.name, sizeof(result.name), "%s", test.phases[index].name.c_str());
        return result;
    }
}

const yliwf::testing::API* mod::combat_tests::RunnerAPI() {
    using namespace yliwf::testing;
    static yliwf::testing::Snapshot test;
    static travel::Snapshot arena;
    static std::chrono::steady_clock::time_point started;
    static std::vector<RunResult> results;
    static bool bypass = false;
    static bool excludeDodge = false;
    static Experiments experiments{DodgeBudget | NativeBash | NativePowerBash};
    static DefenderTuning tuningProfile;
    static std::string savedTuning;
    static mod::combat_tests::spell_diagnostic::Snapshot spellDiagnostic;
    static const API api{
        sizeof(API),
        Version,
        [](const Host*) -> std::uint32_t { return 1; },
        [](yliwf::testing::Snapshot* output) -> std::uint32_t {
            if (!output || output->size < sizeof(*output))
                return 0;
            if (test.active) {
                const auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count();
                auto remaining = elapsed;
                std::uint32_t finished = 0;
                while (finished < test.requestedRuns) {
                    const float duration = test.SuitePlan().ForRun(finished + 1)->Seconds();
                    if (remaining < duration)
                        break;
                    remaining -= duration;
                    ++finished;
                }
                while (results.size() < finished) {
                    RunResult row;
                    if (test.SuitePlan().ForRun(static_cast<std::uint32_t>(results.size() + 1))
                            ->Expectations<mod::combat_tests::testing::OutcomeAssessment>())
                        row.encounterOutcome = mod::combat_tests::natural::Outcome::TimeLimit;
                    row.phase = Phase::Complete;
                    row.verdict = row.encounterOutcome == mod::combat_tests::natural::Outcome::NotMeasured &&
                                  (test.suite || mod::test_rules::DirectEncounter(test.encounter))
                                      ? mod::combat_tests::suite::Verdict::Inconclusive
                                      : mod::combat_tests::suite::Verdict::NotAssessed;
                    row.elapsed = test.SuitePlan().ForRun(static_cast<std::uint32_t>(results.size()) + 1)->Seconds();
                    {
                        auto settings = tuningProfile;
                        test.SuitePlan().Configure(static_cast<std::uint32_t>(results.size() + 1), settings);
                        row.challengeEnabled = settings.challengeEnabled;
                        const auto* selected = test.SuitePlan().ForRun(static_cast<std::uint32_t>(results.size() + 1));
                        const auto* definition = selected;
                        row.phaseCount = static_cast<std::uint32_t>(definition->phases.size());
                        for (std::size_t i = 0; i < row.phaseCount; ++i)
                            row.phases[i] = PreviewPhase(*definition, row.elapsed, i);
                    }
                    row.attacks = 60;
                    row.blocks = 30;
                    row.bashes = 6;
                    row.healing = test.encounter == 1 ? 15 : 0;
                    std::snprintf(row.report, sizeof(row.report),
                                  "Preview result: simulated observations.\n"
                                  "Temporary position, role, resources and following state restored.");
                    results.push_back(row);
                }
                test.finishedRuns = finished;
                test.run = std::min(test.requestedRuns, finished + 1);
                test.duration = test.SuitePlan().ForRun(test.run)->Seconds();
                test.elapsed = finished == test.requestedRuns ? test.duration : remaining;
                {
                    const auto* selected = test.SuitePlan().ForRun(test.run);
                    const auto* definition = selected;
                    for (std::size_t i = 0; i < definition->phases.size(); ++i) {
                        test.testPhase = PreviewPhase(*definition, test.elapsed, i);
                        if (!test.testPhase.completed)
                            break;
                    }
                }
                test.blocks = static_cast<std::uint32_t>(test.elapsed / 2);
                test.bashes = static_cast<std::uint32_t>(test.elapsed / 9);
                test.attacks = static_cast<std::uint32_t>(test.elapsed);
                if (test.encounter == 1) {
                    test.injuryWaves = std::min(5u, 1u + static_cast<std::uint32_t>(test.elapsed / 12));
                    test.healingEffects = static_cast<std::uint32_t>(test.elapsed / 4);
                    test.selfHealingEffects = static_cast<std::uint32_t>(test.elapsed / 15);
                    test.firstHeal = test.healingEffects ? 4.0f : -1.0f;
                }
                if (finished == test.requestedRuns) {
                    test.active = 0;
                    test.phase = Phase::Complete;
                    std::snprintf(test.report, sizeof(test.report),
                                  "Preview result: observed blocks and bashes. No Skyrim actors were changed.");
                }
            }
            *output = test;
            output->experiments = experiments;
            output->tuning = tuningProfile;
            if (test.suite && test.active)
                test.SuitePlan().Configure(test.run, output->tuning);
            output->bypassInterruptions = bypass;
            output->interference.support = interference::Support::Ready;
            output->bypassDodging = excludeDodge;
            output->und.support = interference::Support::Ready;
            output->dodgeTelemetry.attached = test.active;
            output->dodgeTelemetry.requestObserver = test.active;
            output->dodgeTelemetry.readable = test.active ? 15 : 0;
            output->canStart = !test.active && arena.atArena;
            if (!arena.atArena)
                std::snprintf(output->startBlockedReason, sizeof(output->startBlockedReason),
                              "Enter the arena before starting a test.");
            return 1;
        },
        [](std::uint32_t actor) -> std::uint32_t {
            if (test.active || !actor || !arena.atArena)
                return 0;
            test = {};
            results.clear();
            test.subject = actor;
            test.phase = Phase::Running;
            test.active = 1;
            started = std::chrono::steady_clock::now();
            std::snprintf(test.status, sizeof(test.status), "Preview: simulated Shield defense.");
            return 1;
        },
        [] {
            if (test.active) {
                RunResult row;
                row.phase = Phase::Aborted;
                row.elapsed = test.elapsed;
                std::snprintf(row.report, sizeof(row.report), "Preview: aborted; simulated restoration complete.");
                results.push_back(row);
                test.finishedRuns = static_cast<std::uint32_t>(results.size());
            }
            test.active = 0;
            test.phase = Phase::Aborted;
            std::snprintf(test.status, sizeof(test.status), "Preview test aborted.");
            std::snprintf(test.report, sizeof(test.report),
                          "Simulated restoration complete; no game actions were performed.");
        },
        []() -> std::uint32_t { return 0; },
        [](travel::Snapshot* output) -> std::uint32_t {
            if (!output || output->size < sizeof(*output))
                return 0;
            *output = arena;
            output->canEnter = !test.active && !arena.atArena;
            output->canReturn = !test.active && arena.hasReturn;
            std::snprintf(output->location, sizeof(output->location), "%s",
                          arena.atArena ? "YLIWF Combat Arena" : "Preview location");
            if (test.active)
                std::snprintf(output->blocked, sizeof(output->blocked), "Finish the test before travelling.");
            return 1;
        },
        [](bool returning) -> std::uint32_t {
            if (test.active || (returning ? !arena.hasReturn : arena.atArena))
                return 0;
            arena.atArena = arena.hasReturn = !returning;
            std::snprintf(arena.status, sizeof(arena.status), "%s",
                          returning ? "Preview: returned to previous location." : "Preview: entered arena.");
            return 1;
        },
        [](std::uint32_t actor, std::uint32_t encounter) -> std::uint32_t {
            if (!mod::test_rules::ValidEncounter(encounter))
                return 0;
            const auto accepted = api.start(actor);
            if (accepted) {
                test.encounter = encounter;
                if (mod::test_rules::NaturalEncounter(encounter))
                    std::snprintf(test.opponent, sizeof(test.opponent), "Preview fire mage | level 40 | synthetic NPC");
                test.mode = mod::test_rules::Control(encounter);
                std::snprintf(test.status, sizeof(test.status), "Preview: simulated %s.",
                              mod::test_rules::Scenarios[encounter].name);
            }
            return accepted;
        },
        [](std::uint32_t actor, std::uint32_t encounter, std::uint32_t runs) -> std::uint32_t {
            if (runs < 1 || runs > mod::test_rules::MaxRuns)
                return 0;
            const auto accepted = api.startEncounter(actor, encounter);
            if (accepted)
                test.requestedRuns = runs;
            return accepted;
        },
        [](std::uint32_t index, RunResult* output) -> std::uint32_t {
            if (!output || index >= results.size())
                return 0;
            *output = results[index];
            return 1;
        },
        [](bool enabled) -> std::uint32_t {
            if (test.active)
                return 0;
            bypass = enabled;
            return 1;
        },
        [](bool enabled) -> std::uint32_t {
            if (test.active)
                return 0;
            excludeDodge = enabled;
            return 1;
        },
        [](std::uint32_t actor, mod::combat_tests::testing::RunOptions options) -> std::uint32_t {
            const mod::combat_tests::testing::RunPlan run{mod::combat_tests::suite::Catalog, options};
            if (!run.Valid())
                return 0;
            const auto accepted = api.startEncounter(actor, 0);
            if (accepted) {
                test.suite = 1;
                test.suiteRepeats = options.repeats;
                test.suiteSelection = options.selection;
                test.expandVariations = options.expandVariations;
                test.requestedRuns = run.Runs();
                std::snprintf(test.status, sizeof(test.status),
                              "Preview: Defender validation; synthetic coverage only.");
            }
            return accepted;
        },
        [](const Experiments* value) -> std::uint32_t {
            if (!value || test.active)
                return 0;
            experiments = *value;
            return 1;
        },
        [](const DefenderTuning* value) -> std::uint32_t {
            if (!value || test.active || TuningError(*value))
                return 0;
            const auto revision = tuningProfile.revision + 1;
            tuningProfile = *value;
            tuningProfile.revision = revision;
            std::snprintf(test.tuningStatus, sizeof(test.tuningStatus), "Preview profile applied.");
            return 1;
        },
        []() -> std::uint32_t {
            if (test.active)
                return 0;
            savedTuning = mod::combat_tests::tuning::Serialize(tuningProfile);
            std::snprintf(test.tuningStatus, sizeof(test.tuningStatus), "Preview profile saved in memory.");
            return 1;
        },
        []() -> std::uint32_t {
            if (test.active)
                return 0;
            const auto value = mod::combat_tests::tuning::Parse(savedTuning);
            return value ? api.setTuning(&*value) : 0;
        },
        []() -> std::uint32_t {
            if (test.active)
                return 0;
            test.opponent[0] = '\0';
            return 1;
        },
        [](std::uint32_t actor) -> std::uint32_t {
            if (!actor || test.active)
                return 0;
            spellDiagnostic = {};
            spellDiagnostic.completed = 4;
            std::snprintf(spellDiagnostic.status, sizeof(spellDiagnostic.status),
                          "Preview: four synthetic cases completed.");
            std::snprintf(spellDiagnostic.report, sizeof(spellDiagnostic.report),
                          "Preview sample only.\nControl: application observed in both conditions.\nChallenge: "
                          "application not observed.\nNo game actors or spells were used.");
            return 1;
        },
        [](mod::combat_tests::spell_diagnostic::Snapshot* output) -> std::uint32_t {
            if (!output || output->size < sizeof(*output))
                return 0;
            *output = spellDiagnostic;
            return 1;
        },
        []() { spellDiagnostic.active = 0; }};
    return &api;
}
