#include "Menu.h"
#include "../runtime/Runner.h"
#include "../runtime/Rules.h"
#include "../testing/EncounterOutcome.h"
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

    void RenderTuning(const API& runner, const Snapshot& test) {
        if (!ImGuiMCP::CollapsingHeader(mod::combat_tests::suite::Definition.configurationSection.c_str()))
            return;
        static DefenderTuning draft;
        static std::uint32_t displayedRevision = ~0u;
        static bool dirty = false;
        if (displayedRevision != test.tuning.revision) {
            draft = test.tuning;
            displayedRevision = test.tuning.revision;
            dirty = false;
        }
        ImGuiMCP::Text("Applied: %s | revision %u", test.tuning.name, test.tuning.revision);
        ImGuiMCP::TextWrapped("Changes apply to the next series. Running tests keep their captured profile.");
        mod::combat_tests::travel::Snapshot travel;
        const bool traveling = runner.readTravel && runner.readTravel(&travel) && travel.pending;
        const bool blocked = test.active || test.recoveryPending || traveling;
        BeginDisabled(blocked);
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 15);
        dirty |= ImGuiMCP::InputText("Profile name", draft.name, sizeof(draft.name));
        for (const auto& field : TuningFields) {
            ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 7);
            dirty |= ImGuiMCP::InputFloat(field.label, &(draft.*field.member), 0, 0, "%.3f");
            ImGuiMCP::SameLine();
            ImGuiMCP::TextDisabled("[%.2f, %.2f]", field.minimum, field.maximum);
        }
        const auto* error = TuningError(draft);
        if (error)
            ImGuiMCP::TextWrapped("Invalid profile: %s", error);
        BeginDisabled(error != nullptr);
        if (ImGuiMCP::Button("Apply profile") && runner.setTuning && runner.setTuning(&draft))
            dirty = false;
        EndDisabled(error != nullptr);
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Reset values")) {
            draft = {};
            dirty = true;
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Discard edits")) {
            draft = test.tuning;
            dirty = false;
        }
        BeginDisabled(dirty);
        if (ImGuiMCP::Button("Save applied profile") && runner.saveTuning)
            runner.saveTuning();
        EndDisabled(dirty);
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Reload profile file") && runner.reloadTuning)
            runner.reloadTuning();
        EndDisabled(blocked);
        ImGuiMCP::TextWrapped("SKSE/YLIWF/CombatTests/defender-tuning.ini");
        if (dirty)
            ImGuiMCP::TextDisabled("Unapplied edits");
        if (test.tuningStatus[0])
            ImGuiMCP::TextWrapped("%s", test.tuningStatus);
        Gap();
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
            for (const auto phase : mod::combat_tests::suite::Catalog.Order()) {
                if (!(test.suiteSelection & (1u << phase)))
                    continue;
                std::uint32_t passed = 0, failed = 0, inconclusive = 0;
                mod::combat_tests::testing::OutcomeCounts outcomes;
                for (std::uint32_t i = 0; i < test.finishedRuns; ++i) {
                    if (test.SuitePlan().At(i + 1).id != phase)
                        continue;
                    RunResult row;
                    if (runner.readResult(i, &row)) {
                        outcomes.Add(row.encounterOutcome);
                        passed += row.verdict == mod::combat_tests::suite::Verdict::Passed;
                        failed += row.verdict == mod::combat_tests::suite::Verdict::Failed;
                        inconclusive += row.verdict == mod::combat_tests::suite::Verdict::Inconclusive;
                    }
                }
                if (mod::combat_tests::suite::Catalog.Find(phase)
                        ->Expectations<mod::combat_tests::testing::OutcomeAssessment>())
                    ImGuiMCP::Text("%s: %u victories | %u defeats | %u time limits | %u cancelled | %u invalid",
                                   mod::combat_tests::suite::Catalog.Find(phase)->name.c_str(), outcomes.victories,
                                   outcomes.defeats, outcomes.timeLimits, outcomes.cancelled, outcomes.invalid);
                else
                    ImGuiMCP::Text("%s: %u passed | %u failed | %u inconclusive",
                                   mod::combat_tests::suite::Catalog.Find(phase)->name.c_str(), passed, failed,
                                   inconclusive);
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
                if (row.encounterOutcome != mod::combat_tests::natural::Outcome::NotMeasured)
                    ImGuiMCP::TextUnformatted(mod::combat_tests::natural::Name(row.encounterOutcome).data());
                else if (test.suite || mod::test_rules::DirectEncounter(test.encounter))
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
            if (test.suite) {
                const auto scheduled = test.SuitePlan().At(i + 1);
                const auto* variant = test.SuitePlan().VariantForRun(i + 1);
                std::snprintf(title, sizeof(title), "%s %u/%u%s%s##Run%u", test.SuitePlan().ForRun(i + 1)->name.c_str(),
                              scheduled.repetition, test.suiteRepeats, variant ? " | " : "",
                              variant ? variant->name.c_str() : "", i + 1);
            } else
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
            ImGuiMCP::Text("First within 256 units %.2f s | First hit %.2f s (-1: none)", row.engagement.firstReach,
                           row.engagement.firstContact);
            ImGuiMCP::Text("Damage outside range %.1f | Caster bashes requested/completed %u/%u",
                           row.engagement.closingDamage, row.casterBashRequests, row.casterBashCompleted);
            ImGuiMCP::Text("Combat resource-change samples %u", row.fixtureDriftSamples);

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

    void RenderConfiguration(const API& runner, const Snapshot& test) {
        const bool locked = test.active || test.recoveryPending;
        ImGuiMCP::TextWrapped(
            "Shared settings for every scenario and repetition. Integrations affect only the selected test subject "
            "during a run.");
        Gap();
        ImGuiMCP::SeparatorText("DEFENDER CHALLENGE");
        bool challenge = test.tuning.challengeEnabled != 0;
        BeginDisabled(locked);
        if (ImGuiMCP::Checkbox("Enable Challenge (taunt)", &challenge) && runner.setTuning) {
            auto tuning = test.tuning;
            tuning.challengeEnabled = challenge ? 1u : 0u;
            runner.setTuning(&tuning);
        }
        EndDisabled(locked);
        ImGuiMCP::TextWrapped("Applies to the next series. Disable to compare ally protection without taunting.");
        Gap();
        ImGuiMCP::SeparatorText("DODGING");
        const bool undUnavailable = test.und.support != mod::combat_tests::interference::Support::Ready;
        bool allowDodge = !test.bypassDodging;
        BeginDisabled(locked || undUnavailable);
        if (ImGuiMCP::Checkbox("Allow UND dodging", &allowDodge))
            runner.setDodgeBypass(!allowDodge);
        auto options = test.experiments;
        bool budget = (options.flags & DodgeBudget) != 0;
        if (ImGuiMCP::Checkbox("Budget dodge stamina", &budget)) {
            options.flags = budget ? options.flags | DodgeBudget : options.flags & ~DodgeBudget;
            runner.setExperiments(&options);
        }
        EndDisabled(locked || undUnavailable);
        ImGuiMCP::TextWrapped("%s",
                              "Allow UND dodging also enables directed evasions when its TK backend is available. "
                              "Directed requests keep a 20% stamina reserve and a two-second cooldown.");
        BeginDisabled(locked || undUnavailable || !budget || !allowDodge);
        int reserve = static_cast<int>(options.reserveFraction * 100 + .5f);
        float cooldown = options.dodgeCooldown;
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
        if (ImGuiMCP::InputInt("Stamina reserve (%)", &reserve)) {
            options.reserveFraction = static_cast<float>(std::clamp(reserve, 0, 75)) / 100;
            runner.setExperiments(&options);
        }
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
        if (ImGuiMCP::InputFloat("Dodge cooldown (seconds)", &cooldown)) {
            options.dodgeCooldown = std::clamp(cooldown, 0.0f, 10.0f);
            runner.setExperiments(&options);
        }
        EndDisabled(locked || undUnavailable || !budget || !allowDodge);
        ImGuiMCP::TextWrapped(
            "Recovery conserves stamina by declining discretionary dodges. Imminent heavy attacks and hostile casting "
            "retain emergency dodge eligibility. Turning dodging off overrides the budget.");
        if (undUnavailable)
            ImGuiMCP::TextWrapped("%s", mod::combat_tests::interference::und::Status(test.und.support));
        Gap();
        ImGuiMCP::SeparatorText("COMBAT INTEGRATIONS");
        bool suppress = test.bypassInterruptions != 0;
        const bool permaUnavailable = test.interference.support != mod::combat_tests::interference::Support::Ready;
        BeginDisabled(locked || permaUnavailable);
        if (ImGuiMCP::Checkbox("Suppress PermaBlock interruptions", &suppress))
            runner.setInterruptionBypass(suppress);
        EndDisabled(locked || permaUnavailable);
        ImGuiMCP::TextWrapped("Filters Fix NPC PermaBlock's blockStop and bashStop requests for the test subject.");
        if (permaUnavailable)
            ImGuiMCP::TextWrapped("%s", mod::combat_tests::interference::Status(test.interference.support));
        Gap();
        ImGuiMCP::TextWrapped(
            "Defender scenarios include guard, recovery and defensive bashes. Power bashes are selected when eligible "
            "and affordable; ordinary bashes remain the fallback. Individual action checks bypass tactical selection.");
        Gap();
        RenderTuning(runner, test);
    }

    void RenderTest(const yliwf::menu::Context& state, const API& runner, const Snapshot& test,
                    std::uint32_t encounter) {
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

        static int suiteRepeats = static_cast<int>(mod::combat_tests::testing::DefaultRepeats);
        static std::uint32_t suiteSelection =
            mod::combat_tests::suite::Catalog.Select(mod::combat_tests::suite::DefenderAcceptance).selection;
        static bool expandVariations = true;
        if (test.active) {
            suiteRepeats = static_cast<int>(test.suiteRepeats);
            suiteSelection = test.suiteSelection;
            expandVariations = test.expandVariations != 0;
        }
        const auto& scenario = mod::test_rules::Scenarios[encounter];
        ImGuiMCP::SeparatorText(scenario.name);
        ImGuiMCP::TextWrapped("%s", scenario.description);
        if (mod::test_rules::NaturalEncounter(encounter) ||
            (encounter == 0 && (suiteSelection & mod::combat_tests::testing::CaseMask(static_cast<std::uint32_t>(
                                                     mod::combat_tests::suite::Fixture::LeveledMage))))) {
            ImGuiMCP::TextWrapped(
                "%s", test.opponent[0] ? test.opponent : "The first run resolves a fire mage from the leveled list.");
            ImGuiMCP::TextWrapped(
                "The resolved NPC base is reused until you roll another opponent. Distributed spells and natural use "
                "of both hands remain enabled.");
            const bool blocked = test.active || test.recoveryPending;
            BeginDisabled(blocked);
            if (ImGuiMCP::Button("Roll new opponent") && runner.rollOpponent)
                runner.rollOpponent();
            EndDisabled(blocked);
        }
        const bool runSuite = encounter == 0;
        if (runSuite) {
            BeginDisabled(test.active != 0);
            ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
            ImGuiMCP::InputInt("Runs per case", &suiteRepeats);
            suiteRepeats = std::clamp(suiteRepeats, 1, static_cast<int>(mod::combat_tests::testing::MaxRepeats));
            EndDisabled(test.active != 0);
            const auto count = static_cast<std::uint32_t>(suiteRepeats);
            const auto seconds = static_cast<std::uint32_t>(mod::combat_tests::testing::RunPlan{
                mod::combat_tests::suite::Catalog,
                {count, suiteSelection, expandVariations}}.Seconds());
            ImGuiMCP::Text("Validation | %u runs | %u min %02u s plus setup",
                           mod::combat_tests::testing::RunPlan{mod::combat_tests::suite::Catalog,
                                                               {count, suiteSelection, expandVariations}}
                               .Runs(),
                           seconds / 60, seconds % 60);
            ImGuiMCP::TextWrapped("One consolidated report. Leave other followers outside the arena.");
            BeginDisabled(test.active != 0);
            if (ImGuiMCP::Button("Defender acceptance")) {
                const auto preset =
                    mod::combat_tests::suite::Catalog.Select(mod::combat_tests::suite::DefenderAcceptance);
                suiteSelection = preset.selection;
                expandVariations = preset.expandVariations;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Defender essentials")) {
                const auto preset =
                    mod::combat_tests::suite::Catalog.Select(mod::combat_tests::suite::DefenderValidation);
                suiteSelection = preset.selection;
                expandVariations = preset.expandVariations;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("All cases")) {
                suiteSelection = mod::combat_tests::suite::Catalog.Mask();
                expandVariations = false;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Ranged and magic")) {
                suiteSelection = mod::combat_tests::suite::Catalog.Tagged(mod::combat_tests::suite::tags::Ranged);
                expandVariations = false;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Protection review")) {
                const auto preset =
                    mod::combat_tests::suite::Catalog.Select(mod::combat_tests::suite::ProtectionReview);
                suiteSelection = preset.selection;
                expandVariations = preset.expandVariations;
            }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Clear selection")) {
                suiteSelection = 0;
                expandVariations = false;
            }
            if (ImGuiMCP::CollapsingHeader("Select cases", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto renderPhase = [](std::uint32_t i) {
                    const auto& phase = *mod::combat_tests::suite::Catalog.Find(i);
                    bool enabled = (suiteSelection & (1u << i)) != 0;
                    if (ImGuiMCP::Checkbox(phase.name.c_str(), &enabled))
                        suiteSelection = enabled ? suiteSelection | (1u << i) : suiteSelection & ~(1u << i);
                    ImGuiMCP::SameLine();
                    const auto* definition = mod::combat_tests::suite::Definition.Find(i);
                    if (definition && definition->phases.size() > 1)
                        ImGuiMCP::TextDisabled("%d x %.0f s | %u phases", suiteRepeats, phase.Seconds(),
                                               static_cast<std::uint32_t>(definition->phases.size()));
                    else
                        ImGuiMCP::TextDisabled("%d x %.0f s", suiteRepeats, phase.Seconds());
                };
                std::uint32_t additional = 0;
                for (const auto i : mod::combat_tests::suite::Catalog.Order()) {
                    if (mod::combat_tests::suite::Catalog.Tagged(mod::combat_tests::suite::tags::Acceptance) &
                        (1u << i))
                        renderPhase(i);
                    else
                        additional += (suiteSelection & (1u << i)) != 0;
                }
                char label[64]{};
                std::snprintf(label, sizeof(label), "Additional cases (%u selected)###Additional cases", additional);
                if (ImGuiMCP::CollapsingHeader(label))
                    for (const auto i : mod::combat_tests::suite::Catalog.Order())
                        if (!(mod::combat_tests::suite::Catalog.Tagged(mod::combat_tests::suite::tags::Acceptance) &
                              (1u << i)))
                            renderPhase(i);
            }
            if (mod::combat_tests::suite::Catalog.HasVariations(suiteSelection)) {
                ImGuiMCP::Checkbox("Run declared variations", &expandVariations);
                if (expandVariations)
                    ImGuiMCP::TextWrapped(
                        "Each selected case runs its declared variations, alternating order when configured.");
            }
            EndDisabled(test.active != 0);
            if (!suiteSelection)
                ImGuiMCP::TextWrapped("Select at least one phase.");
        } else if (mod::test_rules::DirectEncounter(encounter))
            ImGuiMCP::TextDisabled("Ends on evidence | Up to 20 active seconds | One explicit request per action");
        else if (mod::test_rules::NaturalEncounter(encounter))
            ImGuiMCP::TextDisabled("Up to 60 active seconds | Killable leveled mage | Normal resources");
        else
            ImGuiMCP::TextDisabled("60 active seconds | Controlled sword opponent");

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
        if (encounter == 1)
            ImGuiMCP::TextWrapped(
                "%s",
                "Player health is lowered to 45% at twelve-second intervals and restored after the run. Keep other "
                "followers outside and do not heal yourself.");
        Gap();
        ImGuiMCP::SeparatorText("RUN");
        static int repeats = 1;
        if (runSuite)
            ImGuiMCP::Text("%d repetitions per case", suiteRepeats);
        else {
            BeginDisabled(test.active != 0);
            ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 6);
            ImGuiMCP::InputInt("Repeats", &repeats);
            repeats = std::clamp(repeats, 1, static_cast<int>(mod::test_rules::MaxRuns));
            EndDisabled(test.active != 0);
        }
        BeginDisabled(!eligible || !test.canStart || (runSuite && !suiteSelection));
        if (ImGuiMCP::Button(runSuite ? "Start validation" : "Start test")) {
            if (runSuite)
                runner.startDefenderSuite(selected,
                                          {static_cast<std::uint32_t>(suiteRepeats), suiteSelection, expandVariations});
            else
                runner.startSeries(selected, encounter, static_cast<std::uint32_t>(repeats));
        }
        EndDisabled(!eligible || !test.canStart || (runSuite && !suiteSelection));
        if (test.active) {
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Abort and restore"))
                runner.abort();
            ImGuiMCP::Text("Run %u / %u", test.run, test.requestedRuns);
            if (test.suite)
                if (const auto* variant = test.SuitePlan().VariantForRun(test.run))
                    ImGuiMCP::Text("Variation: %s", variant->name.c_str());
            if (test.suite)
                ImGuiMCP::Text("%s | repetition %u/%u", test.SuitePlan().ForRun(test.run)->name.c_str(),
                               test.SuitePlan().At(test.run).repetition, test.suiteRepeats);
            ImGuiMCP::Text("%.1f / %.0f active seconds | Opponent attacks %u", test.elapsed, test.duration,
                           test.attacks);
            if (test.testPhase.count)
                ImGuiMCP::Text("Phase %u/%u: %s | %.1f / %.0f s", test.testPhase.index, test.testPhase.count,
                               test.testPhase.name, test.testPhase.elapsed, test.testPhase.duration);
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
            if (ImGuiMCP::Button(test.suite ? "Repeat validation" : "Repeat last test")) {
                if (test.suite)
                    runner.startDefenderSuite(test.subject, {static_cast<std::uint32_t>(suiteRepeats),
                                                             test.suiteSelection, test.expandVariations != 0});
                else
                    runner.startSeries(test.subject, test.encounter, static_cast<std::uint32_t>(repeats));
            }
            EndDisabled(!test.canStart || !test.subject);
        }
        RenderResults(runner, test);
    }

    void RenderSpellDiagnostic(const yliwf::menu::Context& context, const API& runner, const Snapshot& test) {
        mod::combat_tests::spell_diagnostic::Snapshot diagnostic;
        if (!runner.readSpellDiagnostic || !runner.readSpellDiagnostic(&diagnostic)) {
            ImGuiMCP::TextWrapped("Spell diagnostics are unavailable.");
            return;
        }
        ImGuiMCP::SeparatorText("Challenge application");
        ImGuiMCP::TextWrapped(
            "Compare Challenge and a harmless water-breathing control with target invulnerability on and off. Four "
            "casts, about ten active seconds, one report.");
        ImGuiMCP::TextWrapped(
            "Enter the arena and select a follower. The diagnostic summons unloaded casters, waits for loading, "
            "then restores their previous location and AI. The caster must be out of combat and outside scenes.");
        static std::uint32_t selected = 0;
        const char* name = "Select caster";
        for (const auto& follower : std::span(context.followers, context.count))
            if (follower.actor == selected)
                name = follower.name;
        BeginDisabled(diagnostic.active != 0);
        if (ImGuiMCP::BeginCombo("Caster##SpellDiagnostic", name)) {
            for (const auto& follower : std::span(context.followers, context.count))
                if (follower.actor && !follower.dead && ImGuiMCP::Selectable(follower.name, selected == follower.actor))
                    selected = follower.actor;
            ImGuiMCP::EndCombo();
        }
        EndDisabled(diagnostic.active != 0);
        const bool blocked = !selected || !test.canStart || diagnostic.active;
        BeginDisabled(blocked);
        if (ImGuiMCP::Button("Run application diagnostic") && runner.startSpellDiagnostic)
            runner.startSpellDiagnostic(selected);
        EndDisabled(blocked);
        if (diagnostic.active) {
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Abort diagnostic") && runner.abortSpellDiagnostic)
                runner.abortSpellDiagnostic();
        } else if (!test.canStart)
            ImGuiMCP::TextWrapped("%s", test.startBlockedReason);
        Gap();
        ImGuiMCP::Text("Cases completed: %u / 4", diagnostic.completed);
        ImGuiMCP::TextWrapped("%s", diagnostic.status);
        if (diagnostic.path[0])
            ImGuiMCP::TextWrapped("Report: %s", diagnostic.path);
        if (diagnostic.report[0]) {
            Gap();
            ImGuiMCP::SeparatorText("Application evidence");
            ImGuiMCP::TextWrapped("%s", diagnostic.report);
        }
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
        if (ImGuiMCP::BeginTabBar("Combat test pages")) {
            if (ImGuiMCP::BeginTabItem("Scenarios")) {
                static std::uint32_t selectedSet = 0;
                if (test.active && mod::test_rules::ValidEncounter(test.encounter)) {
                    selectedSet = test.encounter;
                }
                ImGuiMCP::ImVec2 available;
                ImGuiMCP::GetContentRegionAvail(&available);
                const bool wide = available.x >= ImGuiMCP::GetFontSize() * 30;
                if (wide)
                    ImGuiMCP::BeginChild("Combat test sets", {ImGuiMCP::GetFontSize() * 10, 0}, true);
                ImGuiMCP::SeparatorText("TEST SETS");
                std::string_view category;
                for (const auto i : mod::test_rules::ScenarioOrder) {
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
                RenderTest(*state, runner, test, selectedSet);
                if (wide)
                    ImGuiMCP::EndChild();
                ImGuiMCP::EndTabItem();
            }
            if (ImGuiMCP::BeginTabItem("Configuration")) {
                RenderConfiguration(runner, test);
                ImGuiMCP::EndTabItem();
            }
            if (ImGuiMCP::BeginTabItem("Diagnostics")) {
                RenderSpellDiagnostic(*state, runner, test);
                ImGuiMCP::EndTabItem();
            }
            ImGuiMCP::EndTabBar();
        }
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
