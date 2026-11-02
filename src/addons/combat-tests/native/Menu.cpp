#include "Menu.h"
#include "Runner.h"
#include "Rules.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <cstdio>
#include <cctype>
#include <string_view>
#include <atomic>

namespace {
    using namespace yliwf::testing;

    constexpr std::array modes{"Styles only", "Recovery gate", "Guard requests", "Combined"};
    std::atomic_int menuRequest{0};

    void __stdcall UpdateMenu() {
        const auto request = menuRequest.exchange(0);
        if (!request)
            return;
        using GetWindow = SKSEMenuFramework::Model::WindowInterface* (*)();
        if (const auto get = SKSEMenuFramework::Model::Internal::GetFunction<GetWindow>("GetMainWindow"))
            if (auto* window = get())
                window->IsOpen = request > 0;
    }

    void BeginDisabled(bool disabled) {
        if (disabled) {
            ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_Alpha, ImGuiMCP::GetStyle()->Alpha * 0.35f);
            ImGuiMCP::PushItemFlag(ImGuiMCP::ImGuiItemFlags_Disabled, true);
        }
    }

    void EndDisabled(bool disabled) {
        if (disabled) {
            ImGuiMCP::PopItemFlag();
            ImGuiMCP::PopStyleVar();
        }
    }

    void Gap() {
        ImGuiMCP::Dummy({0, ImGuiMCP::GetFontSize() * 0.5f});
    }

    void RenderResults(const API& runner, const Snapshot& test) {
        if (!test.finishedRuns)
            return;
        Gap();
        ImGuiMCP::SeparatorText("RESULTS");
        ImGuiMCP::Text("%u of %u runs finished", test.finishedRuns, test.requestedRuns);
        if (test.reportPath[0])
            ImGuiMCP::TextWrapped("Report: %s", test.reportPath);
        if (test.suite) {
            for (std::uint32_t phase = 0; phase < mod::combat_tests::suite::Phases.size(); ++phase) {
                std::uint32_t passed = 0, failed = 0, inconclusive = 0;
                for (std::uint32_t i = phase * test.suiteRepeats;
                     i < test.finishedRuns && i < (phase + 1) * test.suiteRepeats; ++i) {
                    RunResult row;
                    if (runner.readResult(i, &row)) {
                        passed += row.verdict == mod::combat_tests::suite::Verdict::Passed;
                        failed += row.verdict == mod::combat_tests::suite::Verdict::Failed;
                        inconclusive += row.verdict == mod::combat_tests::suite::Verdict::Inconclusive;
                    }
                }
                ImGuiMCP::Text("%s: %u passed | %u failed | %u inconclusive",
                               mod::combat_tests::suite::Phases[phase].name, passed, failed, inconclusive);
            }
        }
        if (!ImGuiMCP::CollapsingHeader("Full report"))
            return;
        if (ImGuiMCP::BeginTable("Run results", 7,
                                 ImGuiMCP::ImGuiTableFlags_RowBg | ImGuiMCP::ImGuiTableFlags_BordersInnerH)) {
            for (auto title : {"Run", "Result", "Seconds", "Health lost", "Blocks", "Bashes", "Heals"})
                ImGuiMCP::TableSetupColumn(title);
            ImGuiMCP::TableHeadersRow();
            for (std::uint32_t i = 0; i < test.finishedRuns; ++i) {
                RunResult row;
                if (!runner.readResult(i, &row))
                    continue;
                ImGuiMCP::TableNextRow();
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%u", i + 1);
                ImGuiMCP::TableNextColumn();
                if (test.suite || mod::test_rules::DirectEncounter(test.encounter))
                    ImGuiMCP::TextUnformatted(mod::combat_tests::suite::Name(row.verdict));
                else
                    ImGuiMCP::TextUnformatted(row.phase == Phase::Complete  ? "Complete"
                                              : row.phase == Phase::Aborted ? "Aborted"
                                                                            : "Failed");
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%.1f", row.elapsed);
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%.1f", row.damage);
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%u", row.blocks);
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%u", row.bashes);
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%u", row.healing);
            }
            ImGuiMCP::EndTable();
        }
        Gap();
        for (std::uint32_t i = 0; i < test.finishedRuns; ++i) {
            char title[64];
            if (test.suite)
                std::snprintf(title, sizeof(title), "%s %u/%u##Run%u",
                              mod::combat_tests::suite::ForRun(i + 1, test.suiteRepeats)->name,
                              mod::combat_tests::suite::Repetition(i + 1, test.suiteRepeats), test.suiteRepeats, i + 1);
            else
                std::snprintf(title, sizeof(title), "Run %u details", i + 1);
            if (!ImGuiMCP::CollapsingHeader(title))
                continue;
            RunResult row;
            if (!runner.readResult(i, &row))
                continue;
            ImGuiMCP::Text("Incoming hit events %u | Blocked %u | Opponent attacks %u", row.incoming, row.blocked,
                           row.attacks);
            ImGuiMCP::Text("Damage/s %.2f | Low stamina %.1f s | Recovery exits %u / %u",
                           row.metrics.seconds > 0 ? row.metrics.damage / row.metrics.seconds : 0,
                           row.metrics.lowStaminaSeconds, row.metrics.recoveryExits, row.metrics.recoveryEntries);
            ImGuiMCP::Text("Dodge episodes %u | Surrounding net stamina loss %.1f", row.metrics.dodgeEpisodes,
                           row.metrics.dodgeNetLoss);

            Gap();
            std::string_view text(row.report);
            while (!text.empty()) {
                const auto end = text.find('\n');
                const auto line = text.substr(0, end);
                ImGuiMCP::TextWrapped("%.*s", static_cast<int>(line.size()), line.data());
                Gap();
                if (end == std::string_view::npos)
                    break;
                text.remove_prefix(end + 1);
            }
        }
    }

    void RenderArena(const API& runner) {
        mod::combat_tests::travel::Snapshot arena;
        if (!runner.readTravel(&arena))
            return;
        ImGuiMCP::SeparatorText("ARENA");
        ImGuiMCP::TextUnformatted(arena.atArena       ? "YLIWF Combat Arena"
                                  : arena.location[0] ? arena.location
                                                      : "Outside arena");
        BeginDisabled(!arena.canEnter);
        if (ImGuiMCP::Button("Enter arena"))
            runner.travel(false);
        EndDisabled(!arena.canEnter);
        ImGuiMCP::SameLine();
        BeginDisabled(!arena.canReturn);
        if (ImGuiMCP::Button("Return to previous location"))
            runner.travel(true);
        EndDisabled(!arena.canReturn);
        if (arena.status[0])
            ImGuiMCP::TextWrapped("%s", arena.status);
        if (arena.blocked[0])
            ImGuiMCP::TextWrapped("%s", arena.blocked);
        if (!arena.hasReturn && arena.atArena)
            ImGuiMCP::TextDisabled("No return location recorded. Enter through this menu to record one.");
        if (!arena.atArena)
            ImGuiMCP::TextWrapped("Have other followers wait outside before entering with the test follower.");
        Gap();
    }

    void RenderTest(const yliwf::menu::Context& state, const API& runner, const Snapshot& test, std::uint32_t encounter,
                    int& mode) {
        static std::uint32_t selected = 0;
        const std::span followers(state.followers, state.count);
        if (std::ranges::none_of(followers, [](const auto& follower) { return follower.actor == selected; }))
            selected = 0;
        const auto chosenName = [&]() -> const char* {
            for (const auto& follower : followers)
                if (follower.actor == selected)
                    return follower.name;
            return "Select follower";
        };

        static bool suite = false;
        static int suiteRepeats = static_cast<int>(mod::combat_tests::suite::DefaultRepeats);
        if (test.active) {
            suite = test.suite != 0;
            suiteRepeats = static_cast<int>(test.suiteRepeats);
        }
        const auto& scenario = mod::test_rules::Scenarios[encounter];
        ImGuiMCP::SeparatorText(scenario.name);
        ImGuiMCP::TextWrapped("%s", scenario.description);
        if (encounter == 0) {
            BeginDisabled(test.active != 0);
            ImGuiMCP::Checkbox("Combined validation suite", &suite);
            EndDisabled(test.active != 0);
        }
        const bool runSuite = encounter == 0 && suite;
        if (runSuite) {
            BeginDisabled(test.active != 0);
            ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
            ImGuiMCP::InputInt("Runs per phase", &suiteRepeats);
            suiteRepeats = std::clamp(suiteRepeats, 1, static_cast<int>(mod::combat_tests::suite::MaxRepeats));
            EndDisabled(test.active != 0);
            const auto count = static_cast<std::uint32_t>(suiteRepeats);
            ImGuiMCP::Text("Combined | %u runs | %.0f active minutes plus setup",
                           mod::combat_tests::suite::Runs(count),
                           mod::combat_tests::suite::ActiveSeconds(count) / 60);
            ImGuiMCP::TextWrapped("One consolidated report. Leave other followers outside the arena.");
            if (ImGuiMCP::CollapsingHeader("Suite phases"))
                for (const auto& phase : mod::combat_tests::suite::Phases) {
                    ImGuiMCP::Text("%s | %d x %.0f s", phase.name, suiteRepeats, phase.seconds);
                    ImGuiMCP::TextWrapped("%s", phase.description);
                }
        } else if (mod::test_rules::DirectEncounter(encounter))
            ImGuiMCP::TextDisabled("Ends on evidence | Up to 20 active seconds | One explicit request per action");
        else
            ImGuiMCP::TextDisabled("60 active seconds | One-handed sword opponent");

        Gap();
        ImGuiMCP::SeparatorText("FOLLOWER");
        BeginDisabled(test.active != 0);
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 13);
        if (ImGuiMCP::BeginCombo("##CombatTestFollower", chosenName())) {
            for (const auto& follower : followers) {
                if (!follower.actor || follower.dead)
                    continue;
                ImGuiMCP::PushID(static_cast<int>(follower.actor));
                if (ImGuiMCP::Selectable(follower.name, selected == follower.actor))
                    selected = follower.actor;
                ImGuiMCP::PopID();
            }
            ImGuiMCP::EndCombo();
        }
        EndDisabled(test.active != 0);
        const auto chosen =
            std::ranges::find_if(followers, [](const auto& follower) { return follower.actor == selected; });
        const bool eligible =
            chosen != followers.end() && chosen->actor && !chosen->dead &&
            (!chosen->loaded || (chosen->readyRoles & (1u << mod::test_rules::EquipmentRole(encounter))));
        if (selected && !eligible)
            ImGuiMCP::TextWrapped("Follower must have combat permission. %s", scenario.requirement);
        else if (!selected)
            ImGuiMCP::TextWrapped("Select the follower to evaluate.");
        else if (!chosen->loaded || chosen->waiting)
            ImGuiMCP::TextWrapped(
                "The runner will summon this follower and temporarily enable following. "
                "Equipment is checked after loading.");

        Gap();
        if ((encounter == 0 || mod::test_rules::DirectEncounter(encounter)) &&
            ImGuiMCP::CollapsingHeader(encounter == 0 ? "Comparison settings" : "Interference settings")) {
            if (encounter == 0) {
                BeginDisabled(test.active != 0 || runSuite);
                if (ImGuiMCP::BeginCombo("Control mode", modes[runSuite ? 3 : mode])) {
                    for (int i = 0; i < static_cast<int>(modes.size()); ++i)
                        if (ImGuiMCP::Selectable(modes[i], mode == i))
                            mode = i;
                    ImGuiMCP::EndCombo();
                }
                EndDisabled(test.active != 0 || runSuite);
                ImGuiMCP::TextWrapped("Native action controls require Skyrim 1.6.1170. Styles only is the baseline.");
                Gap();
                auto options = test.experiments;
                bool budget = (options.flags & DodgeBudget) != 0;
                bool nativeBash = (options.flags & NativeBash) != 0;
                const bool budgetUnavailable = test.und.support != mod::combat_tests::interference::Support::Ready;
                BeginDisabled(test.active || budgetUnavailable);
                if (ImGuiMCP::Checkbox("Budget UND dodge stamina", &budget)) {
                    options.flags = budget ? options.flags | DodgeBudget : options.flags & ~DodgeBudget;
                    runner.setExperiments(&options);
                }
                EndDisabled(test.active || budgetUnavailable);
                if (budget) {
                    BeginDisabled(test.active != 0);
                    int reserve = static_cast<int>(options.reserveFraction * 100 + .5f);
                    int cooldown = static_cast<int>(options.dodgeCooldown + .5f);
                    ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
                    if (ImGuiMCP::InputInt("Stamina reserve (%)", &reserve)) {
                        options.reserveFraction = static_cast<float>(std::clamp(reserve, 0, 75)) / 100;
                        runner.setExperiments(&options);
                    }
                    ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
                    if (ImGuiMCP::InputInt("Dodge cooldown (seconds)", &cooldown)) {
                        options.dodgeCooldown = static_cast<float>(std::clamp(cooldown, 0, 10));
                        runner.setExperiments(&options);
                    }
                    EndDisabled(test.active != 0);
                    ImGuiMCP::TextWrapped(
                        "Reserves stamina after UND's estimated maximum dodge cost. "
                        "Cooldown spaces permitted decisions, not confirmed dodges. Full UND exclusion takes "
                        "priority.");
                    ImGuiMCP::Text("Dodge decisions: %u allowed | reserve denied %u | cooldown denied %u | stale %u",
                                   test.und.budgetAllowed, test.und.reserveDenied, test.und.cooldownDenied,
                                   test.und.snapshotDenied);
                }
                const bool bashDisabled = test.active || (!nativeBash && !((runSuite ? 3 : mode) & 2));
                BeginDisabled(bashDisabled);
                if (ImGuiMCP::Checkbox("Native defensive bashes", &nativeBash)) {
                    options.flags =
                        nativeBash ? options.flags | NativeBash : options.flags & ~(NativeBash | NativePowerBash);
                    runner.setExperiments(&options);
                }
                EndDisabled(bashDisabled);
                if (nativeBash) {
                    bool power = (options.flags & NativePowerBash) != 0;
                    BeginDisabled(test.active != 0);
                    if (ImGuiMCP::Checkbox("Use power bashes", &power)) {
                        options.flags = power ? options.flags | NativePowerBash : options.flags & ~NativePowerBash;
                        runner.setExperiments(&options);
                    }
                    EndDisabled(test.active != 0);
                }
                ImGuiMCP::TextWrapped(
                    "Bashes against close frontal windups while owning a guard. Power bashes require engine perk/idle "
                    "eligibility and a larger stamina reserve. "
                    "Requires Guard or Combined. Both experiments are off for the baseline.");

                Gap();
            }
            bool bypass = test.bypassInterruptions != 0;
            const bool blocked = test.active || (!bypass && test.interference.support !=
                                                                mod::combat_tests::interference::Support::Ready);
            BeginDisabled(blocked);
            if (ImGuiMCP::Checkbox("Suppress PermaBlock interruptions", &bypass))
                runner.setInterruptionBypass(bypass);
            EndDisabled(blocked);
            ImGuiMCP::TextWrapped(
                "Filters Fix NPC PermaBlock's blockStop and bashStop events for the test follower. "
                "Compare identical runs with this off and on.");
            if (test.interference.support != mod::combat_tests::interference::Support::Ready)
                ImGuiMCP::TextWrapped("%s", mod::combat_tests::interference::Status(test.interference.support));
            Gap();
            bool excludeDodge = test.bypassDodging != 0;
            const bool dodgeBlocked =
                test.active || (!excludeDodge && test.und.support != mod::combat_tests::interference::Support::Ready);
            BeginDisabled(dodgeBlocked);
            if (ImGuiMCP::Checkbox("Exclude follower from UND dodging", &excludeDodge))
                runner.setDodgeBypass(excludeDodge);
            EndDisabled(dodgeBlocked);
            ImGuiMCP::TextWrapped(
                "Overrides UND's exclusion and dodge eligibility checks for the test follower. "
                "Other actors and dodge mods are unchanged.");
            if (test.und.support != mod::combat_tests::interference::Support::Ready)
                ImGuiMCP::TextWrapped("%s", mod::combat_tests::interference::und::Status(test.und.support));
            ImGuiMCP::Text("UND: checks %u | exclusions %u | eligibility %u | denied %u", test.und.checks,
                           test.und.exclusions, test.und.attempts, test.und.denied);
            const auto graphFlag = [&test](std::uint32_t bit) {
                return !(test.dodgeTelemetry.readable & bit) ? "unavailable"
                       : (test.dodgeTelemetry.flags & bit)   ? "yes"
                                                             : "no";
            };
            ImGuiMCP::Text("Dodge graph: %s | Dodging: %s | Roll style: %s",
                           test.dodgeTelemetry.attached ? "observing" : "not attached", graphFlag(1), graphFlag(2));
            ImGuiMCP::Text("Graph callbacks: dodge %u | guard %u | sampled dodging %.1f s",
                           test.dodgeTelemetry.dodgeEvents, test.dodgeTelemetry.guardEvents,
                           test.dodgeTelemetry.dodgingSeconds);
            ImGuiMCP::Text("Animation requests: %s | %u sent | %u completed | %u returned false",
                           !test.active                          ? "inactive"
                           : test.dodgeTelemetry.requestObserver ? "observing"
                                                                 : "unavailable",
                           test.dodgeTelemetry.notifications, test.dodgeTelemetry.notificationCompleted,
                           test.dodgeTelemetry.notificationFalse);
        }
        if (encounter == 1)
            ImGuiMCP::TextWrapped(
                "Player health is lowered to 45% at twelve-second intervals and restored after the run. Keep other "
                "followers outside and do not heal yourself.");
        Gap();
        ImGuiMCP::SeparatorText("RUN");
        static int repeats = 1;
        if (runSuite)
            ImGuiMCP::Text("%d repetitions per phase", suiteRepeats);
        else {
            BeginDisabled(test.active != 0);
            ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
            ImGuiMCP::InputInt("Repeats", &repeats);
            repeats = std::clamp(repeats, 1, static_cast<int>(mod::test_rules::MaxRuns));
            EndDisabled(test.active != 0);
        }
        BeginDisabled(!eligible || !test.canStart);
        if (ImGuiMCP::Button(runSuite ? "Start Combined suite" : "Start test")) {
            if (runSuite)
                runner.startDefenderSuite(selected, static_cast<std::uint32_t>(suiteRepeats));
            else
                runner.startSeries(selected, encounter,
                                   mod::test_rules::DirectEncounter(encounter) ? 3
                                   : encounter == 0                            ? static_cast<std::uint32_t>(mode)
                                                                               : 0,
                                   static_cast<std::uint32_t>(repeats));
        }
        EndDisabled(!eligible || !test.canStart);
        if (test.active) {
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Abort and restore"))
                runner.abort();
            ImGuiMCP::Text("Run %u / %u", test.run, test.requestedRuns);
            if (test.suite)
                ImGuiMCP::Text("%s | repetition %u/%u",
                               mod::combat_tests::suite::ForRun(test.run, test.suiteRepeats)->name,
                               mod::combat_tests::suite::Repetition(test.run, test.suiteRepeats), test.suiteRepeats);
            ImGuiMCP::Text("%.1f / %.0f active seconds | Opponent attacks %u", test.elapsed, test.duration,
                           test.attacks);
            if (test.encounter == 1)
                ImGuiMCP::Text("Healing effects: ally %u | self %u | Injury waves %u", test.healingEffects,
                               test.selfHealingEffects, test.injuryWaves);
            else
                ImGuiMCP::Text("Block starts %u | Bash starts %u", test.blocks, test.bashes);
            if (test.encounter == 0)
                ImGuiMCP::Text("PermaBlock interruptions: block %u | bash %u | suppressed %u",
                               test.interference.blockStops, test.interference.bashStops, test.interference.suppressed);
        }
        if (!test.canStart && !test.active && test.startBlockedReason[0])
            ImGuiMCP::TextWrapped("%s", test.startBlockedReason);
        if (test.status[0])
            ImGuiMCP::TextWrapped("%s", test.status);
        if (test.canRecheckRecovery && ImGuiMCP::Button("Recheck guard recovery"))
            runner.recheckRecovery();
        ImGuiMCP::TextWrapped(
            "Observe without attacking or casting. The menu closes during travel and tests, "
            "then reopens on its last selected page.");

        if (test.report[0]) {
            Gap();
            ImGuiMCP::SeparatorText("LAST RESULT");
            if (mod::test_rules::ValidEncounter(test.encounter))
                ImGuiMCP::TextUnformatted(mod::test_rules::Scenarios[test.encounter].name);
            BeginDisabled(!test.canStart || !test.subject);
            if (ImGuiMCP::Button(test.suite ? "Repeat Combined suite" : "Repeat last test")) {
                if (test.suite)
                    runner.startDefenderSuite(test.subject, static_cast<std::uint32_t>(suiteRepeats));
                else
                    runner.startSeries(test.subject, test.encounter, test.mode, static_cast<std::uint32_t>(repeats));
            }
            EndDisabled(!test.canStart || !test.subject);
        }
        RenderResults(runner, test);
    }

    void __stdcall Render(void*, const yliwf::menu::Context* state) {
        ImGuiMCP::BeginChild("Combat tests body", {0, 0});
        if (!state || state->size < sizeof(*state) || !state->ready) {
            ImGuiMCP::TextWrapped("Load a game to configure and run combat tests.");
            ImGuiMCP::EndChild();
            return;
        }
        const auto& runner = *mod::combat_tests::RunnerAPI();
        Snapshot test;
        if (!runner.read(&test)) {
            ImGuiMCP::TextWrapped("Combat test state is unavailable.");
            ImGuiMCP::EndChild();
            return;
        }
        RenderArena(runner);
        static std::uint32_t selectedSet = 0;
        static int mode = 0;
        if (test.active && mod::test_rules::ValidEncounter(test.encounter)) {
            selectedSet = test.encounter;
            mode = static_cast<int>(test.mode);
        }
        ImGuiMCP::ImVec2 available;
        ImGuiMCP::GetContentRegionAvail(&available);
        const bool wide = available.x >= ImGuiMCP::GetFontSize() * 30;
        if (wide)
            ImGuiMCP::BeginChild("Combat test sets", {ImGuiMCP::GetFontSize() * 10, 0}, true);
        ImGuiMCP::SeparatorText("TEST SETS");
        std::string_view category;
        for (std::uint32_t i = 0; i < mod::test_rules::Scenarios.size(); ++i) {
            const auto& scenario = mod::test_rules::Scenarios[i];
            if (category != scenario.category) {
                if (!category.empty())
                    Gap();
                category = scenario.category;
                char heading[64]{};
                std::snprintf(heading, sizeof(heading), "%s", scenario.category);
                for (char& ch : heading)
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                ImGuiMCP::TextColored({0.62f, 0.62f, 0.62f, 1}, "%s", heading);
                ImGuiMCP::Separator();
            }
            const auto indent = ImGuiMCP::GetFontSize() * 0.6f;
            ImGuiMCP::Indent(indent);
            BeginDisabled(test.active != 0);
            if (ImGuiMCP::Selectable(scenario.name, selectedSet == i))
                selectedSet = i;
            EndDisabled(test.active != 0);
            ImGuiMCP::Unindent(indent);
        }
        if (wide) {
            ImGuiMCP::EndChild();
            ImGuiMCP::SameLine();
            ImGuiMCP::BeginChild("Combat test details", {0, 0}, true);
        } else
            Gap();
        RenderTest(*state, runner, test, selectedSet, mode);
        if (wide)
            ImGuiMCP::EndChild();
        ImGuiMCP::EndChild();
    }
}

bool mod::combat_tests::RegisterMenu(const yliwf::menu::API* api) {
    if (!api || api->size < sizeof(*api) || api->version != yliwf::menu::Version || !api->registerTab)
        return false;
    yliwf::menu::Tab tab;
    std::snprintf(tab.id, sizeof(tab.id), "yliwf.combat-tests");
    std::snprintf(tab.title, sizeof(tab.title), "Combat tests");
    tab.render = Render;
    return api->registerTab(&tab) != 0;
}

void mod::combat_tests::RegisterMenuControl() {
    SKSEMenuFramework::AddHudElement(UpdateMenu);
}

void mod::combat_tests::RequestMenu(bool open) {
    menuRequest = open ? 1 : -1;
}

void mod::combat_tests::ClearMenuRequest() {
    menuRequest = 0;
}
