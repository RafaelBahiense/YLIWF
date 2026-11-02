#include "../native/Runner.h"
#include "../native/Menu.h"
#include "../native/Rules.h"
#include "AddonTabs.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

extern "C" const yliwf::menu::API* YLIWF_GetMenuAPI(std::uint32_t);

namespace {
    preview::AddonTab registration{[] { mod::combat_tests::RegisterMenu(YLIWF_GetMenuAPI(yliwf::menu::Version)); }};
}

const yliwf::testing::API* mod::combat_tests::RunnerAPI() {
    using namespace yliwf::testing;
    static yliwf::testing::Snapshot test;
    static travel::Snapshot arena;
    static std::chrono::steady_clock::time_point started;
    static std::vector<RunResult> results;
    static bool bypass = false;
    static bool excludeDodge = false;
    static Experiments experiments;
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
                    const float duration =
                        test.suite ? mod::combat_tests::suite::ForRun(finished + 1, test.suiteRepeats)->seconds
                        : mod::test_rules::DirectEncounter(test.encounter) ? 20
                                                                           : 60;
                    if (remaining < duration)
                        break;
                    remaining -= duration;
                    ++finished;
                }
                while (results.size() < finished) {
                    RunResult row;
                    row.phase = Phase::Complete;
                    row.verdict = test.suite || mod::test_rules::DirectEncounter(test.encounter)
                                      ? mod::combat_tests::suite::Verdict::Inconclusive
                                      : mod::combat_tests::suite::Verdict::NotAssessed;
                    row.elapsed = test.suite ? mod::combat_tests::suite::ForRun(
                                                   static_cast<std::uint32_t>(results.size()) + 1, test.suiteRepeats)
                                                   ->seconds
                                  : mod::test_rules::DirectEncounter(test.encounter) ? 20
                                                                                     : 60;
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
                test.duration = test.suite ? mod::combat_tests::suite::ForRun(test.run, test.suiteRepeats)->seconds
                                : mod::test_rules::DirectEncounter(test.encounter) ? 20
                                                                                   : 60;
                test.elapsed = finished == test.requestedRuns ? test.duration : remaining;
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
        [](std::uint32_t actor, std::uint32_t mode) -> std::uint32_t {
            if (mode > 3)
                return 0;
            const auto accepted = api.start(actor);
            if (accepted)
                test.mode = mode;
            return accepted;
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
        [](std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode) -> std::uint32_t {
            if (!mod::test_rules::ValidConfiguration(encounter, mode))
                return 0;
            const auto accepted = api.start(actor);
            if (accepted) {
                test.encounter = encounter;
                test.mode = mode;
                std::snprintf(test.status, sizeof(test.status), "Preview: simulated %s.",
                              mod::test_rules::Scenarios[encounter].name);
            }
            return accepted;
        },
        [](std::uint32_t actor, std::uint32_t encounter, std::uint32_t mode, std::uint32_t runs) -> std::uint32_t {
            if (runs < 1 || runs > mod::test_rules::MaxRuns)
                return 0;
            const auto accepted = api.startEncounter(actor, encounter, mode);
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
        [](std::uint32_t actor, std::uint32_t repeats) -> std::uint32_t {
            if (!mod::combat_tests::suite::ValidRepeats(repeats))
                return 0;
            const auto accepted = api.startEncounter(actor, 0, 3);
            if (accepted) {
                test.suite = 1;
                test.suiteRepeats = repeats;
                test.requestedRuns = mod::combat_tests::suite::Runs(repeats);
                std::snprintf(test.status, sizeof(test.status), "Preview: Combined suite; synthetic coverage only.");
            }
            return accepted;
        },
        [](const Experiments* value) -> std::uint32_t {
            if (!value || test.active)
                return 0;
            experiments = *value;
            return 1;
        }};
    return &api;
}
