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

namespace {
    using namespace yliwf::testing;

    constexpr std::array modes{"Styles only", "Recovery gate", "Guard requests", "Combined"};

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

        const auto& scenario = mod::test_rules::Scenarios[encounter];
        ImGuiMCP::SeparatorText(scenario.name);
        ImGuiMCP::TextWrapped("%s", scenario.description);
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
        const bool eligible = chosen != followers.end() && chosen->actor && !chosen->dead && chosen->loaded &&
                              !chosen->waiting && (chosen->readyRoles & (1u << scenario.role));
        if (selected && !eligible)
            ImGuiMCP::TextWrapped("Follower must be loaded, following and have combat permission. %s",
                                  scenario.requirement);
        else if (!selected)
            ImGuiMCP::TextWrapped("Select the follower to evaluate.");

        Gap();
        if (encounter == 0 && ImGuiMCP::CollapsingHeader("Comparison settings")) {
            BeginDisabled(test.active != 0);
            if (ImGuiMCP::BeginCombo("Control mode", modes[mode])) {
                for (int i = 0; i < static_cast<int>(modes.size()); ++i)
                    if (ImGuiMCP::Selectable(modes[i], mode == i))
                        mode = i;
                ImGuiMCP::EndCombo();
            }
            EndDisabled(test.active != 0);
            ImGuiMCP::TextWrapped("Native action controls require Skyrim 1.6.1170. Styles only is the baseline.");
        }
        if (encounter == 1)
            ImGuiMCP::TextWrapped(
                "Player health is lowered to 45% at twelve-second intervals and restored after the run. Keep other "
                "followers outside and do not heal yourself.");
        Gap();
        ImGuiMCP::SeparatorText("RUN");
        BeginDisabled(!eligible || !test.canStart);
        if (ImGuiMCP::Button("Start test"))
            runner.startEncounter(selected, encounter, encounter == 0 ? static_cast<std::uint32_t>(mode) : 0);
        EndDisabled(!eligible || !test.canStart);
        if (test.active) {
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Abort and restore"))
                runner.abort();
            ImGuiMCP::Text("%.1f / %.0f active seconds | Opponent attacks %u", test.elapsed, test.duration,
                           test.attacks);
            if (test.encounter == 1)
                ImGuiMCP::Text("Healing effects: ally %u | self %u | Injury waves %u", test.healingEffects,
                               test.selfHealingEffects, test.injuryWaves);
            else
                ImGuiMCP::Text("Block starts %u | Bash starts %u", test.blocks, test.bashes);
        }
        if (!test.canStart && !test.active && test.startBlockedReason[0])
            ImGuiMCP::TextWrapped("%s", test.startBlockedReason);
        if (test.status[0])
            ImGuiMCP::TextWrapped("%s", test.status);
        if (test.canRecheckRecovery && ImGuiMCP::Button("Recheck guard recovery"))
            runner.recheckRecovery();
        ImGuiMCP::TextWrapped("Close paused menus after Start or Abort. Observe without attacking or casting.");

        if (test.report[0]) {
            Gap();
            ImGuiMCP::SeparatorText("LAST RESULT");
            if (mod::test_rules::ValidEncounter(test.encounter))
                ImGuiMCP::TextUnformatted(mod::test_rules::Scenarios[test.encounter].name);
            BeginDisabled(!test.canStart || !test.subject);
            if (ImGuiMCP::Button("Repeat last test"))
                runner.startEncounter(test.subject, test.encounter, test.mode);
            EndDisabled(!test.canStart || !test.subject);
            if (ImGuiMCP::CollapsingHeader("Full report"))
                ImGuiMCP::TextWrapped("%s", test.report);
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
