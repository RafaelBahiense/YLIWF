#include "../native/Runner.h"
#include "../native/Menu.h"
#include "../native/Encounters.h"
#include "AddonTabs.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

extern "C" const yliwf::menu::API* YLIWF_GetMenuAPI(std::uint32_t);

namespace {
    preview::AddonTab registration{[] { mod::combat_tests::RegisterMenu(YLIWF_GetMenuAPI(yliwf::menu::Version)); }};
}

const yliwf::testing::API* mod::combat_tests::RunnerAPI() {
    using namespace yliwf::testing;
    static yliwf::testing::Snapshot test;
    static travel::Snapshot arena;
    static std::chrono::steady_clock::time_point started;
    static const API api{
        sizeof(API),
        Version,
        [](const Host*) -> std::uint32_t { return 1; },
        [](yliwf::testing::Snapshot* output) -> std::uint32_t {
            if (!output || output->size < sizeof(*output))
                return 0;
            if (test.active) {
                test.elapsed =
                    std::min(std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count(), 60.0f);
                test.blocks = static_cast<std::uint32_t>(test.elapsed / 2);
                test.bashes = static_cast<std::uint32_t>(test.elapsed / 9);
                test.attacks = static_cast<std::uint32_t>(test.elapsed);
                if (test.encounter == 1) {
                    test.injuryWaves = std::min(5u, 1u + static_cast<std::uint32_t>(test.elapsed / 12));
                    test.healingEffects = static_cast<std::uint32_t>(test.elapsed / 4);
                    test.selfHealingEffects = static_cast<std::uint32_t>(test.elapsed / 15);
                    test.firstHeal = test.healingEffects ? 4.0f : -1.0f;
                }
                if (test.elapsed >= 60) {
                    test.active = 0;
                    test.phase = Phase::Complete;
                    std::snprintf(test.report, sizeof(test.report),
                                  "Preview result: observed blocks and bashes. No Skyrim actors were changed.");
                }
            }
            *output = test;
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
            test.subject = actor;
            test.phase = Phase::Running;
            test.active = 1;
            started = std::chrono::steady_clock::now();
            std::snprintf(test.status, sizeof(test.status), "Preview: simulated Shield defense.");
            return 1;
        },
        [] {
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
        }};
    return &api;
}
