#include "ui/UI.h"
#include "ui/HotkeysPage.h"
#include "ui/PreferencesFooter.h"
#include "input/Hotkeys.h"
#include "combat/CombatOrders.h"

#include "settings/Settings.h"
#include "diagnostics/Debug.h"
#include "diagnostics/DebugRules.h"
#include "followers/FollowDistance.h"
#include "formations/Formation.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <unordered_set>

static void BeginDisabled(bool disabled) {
    if (disabled) {
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_Alpha, ImGuiMCP::GetStyle()->Alpha * 0.35f);
        ImGuiMCP::PushItemFlag(ImGuiMCP::ImGuiItemFlags_Disabled, true);
    }
}

static void EndDisabled(bool disabled) {
    if (disabled) {
        ImGuiMCP::PopItemFlag();
        ImGuiMCP::PopStyleVar();
    }
}

static void HelpMarker(const char* desc) {
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.55f, 0.55f, 0.55f, 1.0f});
    ImGuiMCP::TextUnformatted("(?)");
    ImGuiMCP::PopStyleColor();
    if (ImGuiMCP::IsItemHovered()) {
        ImGuiMCP::BeginTooltip();
        ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 28.0f);
        ImGuiMCP::TextUnformatted(desc);
        ImGuiMCP::PopTextWrapPos();
        ImGuiMCP::EndTooltip();
    }
}

namespace {
    constexpr ImGuiMCP::ImVec4 Accent{0.85f, 0.72f, 0.40f, 1.0f};
    constexpr ImGuiMCP::ImVec4 Muted{0.65f, 0.65f, 0.65f, 1.0f};

    float ContentWidth() {
        ImGuiMCP::ImVec2 available;
        ImGuiMCP::GetContentRegionAvail(&available);
        return available.x;
    }

    void SectionGap() {
        ImGuiMCP::Dummy({0, ImGuiMCP::GetFontSize() * 0.5f});
    }

    enum class Page { Followers, Settings, Hotkeys, Debug };
    Page lastPage = Page::Settings;
    int lastPageFrame = -1;
    std::atomic_bool snapshotPageVisible{false};
    std::atomic<SKSEMenuFramework::Model::WindowInterface*> mainWindow{nullptr};
    using GetMainWindow = SKSEMenuFramework::Model::WindowInterface* (*)();
    GetMainWindow getMainWindow = nullptr;

    // The framework calls HUD observers before section pages every frame. This
    // observer only tracks visibility; it never reads Skyrim or queues a refresh.
    void __stdcall ObserveVisibility() {
        if (lastPage != Page::Hotkeys || !mod::ui::IsMenuOpen() || lastPageFrame != ImGuiMCP::GetFrameCount() - 1)
            mod::hotkeys::CancelCapture();
        if (!mod::ui::IsRefreshVisible() || lastPageFrame != ImGuiMCP::GetFrameCount() - 1) {
            snapshotPageVisible = false;
            mod::debug::SetRefreshVisible(false);
        }
    }

    struct Confirmation {
        mod::debug::Action action = mod::debug::Action::Dismiss;
        std::uint32_t actor = 0;
        std::int32_t alias = -1;
        std::uint64_t generation = 0;
        std::string name;
        bool party = false;
        bool active = false, open = false;
    } confirmation;

    mod::debug::Snapshot PageSnapshot(Page page) {
        if (getMainWindow && !mainWindow.load())
            mainWindow = getMainWindow();
        if (page != lastPage || lastPageFrame != ImGuiMCP::GetFrameCount() - 1)
            mod::debug::NotifyStateChanged();
        lastPage = page;
        lastPageFrame = ImGuiMCP::GetFrameCount();
        snapshotPageVisible = true;
        mod::debug::SetRefreshVisible(true);
        mod::debug::RefreshVisiblePage();
        return mod::debug::GetSnapshot();
    }

    bool CanRequest(const mod::debug::Snapshot& state, mod::debug::Action action) {
        return state.ready && !state.busy &&
               mod::debug_rules::CanCommand(state.structureReady, state.scriptReady, state.bindingsReady,
                                            static_cast<std::int32_t>(action), mod::debug::OptionsEnabled.load());
    }

    void Confirm(mod::debug::Action action, const mod::debug::Follower& row, const mod::debug::Snapshot& state,
                 std::int32_t alias) {
        confirmation = {action, row.formID, alias, state.generation, row.name.empty() ? "Unnamed follower" : row.name,
                        false,  true,       true};
    }

    void ConfirmParty(mod::debug::Action action, const mod::debug::Snapshot& state) {
        confirmation = {action, 0, -1, state.generation, "all registered followers", true, true, true};
    }

    void RenderConfirmation(const mod::debug::Snapshot& state) {
        constexpr const char* title = "Confirm follower action";
        if (confirmation.open) {
            // Open outside the row's PushID/child scope so the modal uses the same ID.
            ImGuiMCP::OpenPopup(title);
            confirmation.open = false;
        }
        if (!confirmation.active)
            return;
        if (ImGuiMCP::BeginPopupModal(title, &confirmation.active, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool current = state.ready && confirmation.generation == state.generation &&
                                 (mod::debug_rules::IsPartyAction(static_cast<std::int32_t>(confirmation.action)) ||
                                  mod::debug::OptionsEnabled.load());
            if (!current) {
                confirmation.active = false;
                ImGuiMCP::CloseCurrentPopup();
            } else {
                const auto label = confirmation.party ? mod::debug::PartyActionName(confirmation.action)
                                                      : mod::debug::ActionName(confirmation.action);
                ImGuiMCP::TextWrapped("%s: %s?", label, confirmation.name.c_str());
                if (confirmation.action == mod::debug::Action::RestoreRecruitment)
                    ImGuiMCP::TextWrapped(
                        "Adds potential-follower membership and reports the current relationship rank. "
                        "If needed, suggests a console command to reach Friend. No party slot is assigned.");
                if (!confirmation.party && mod::debug::OptionsEnabled.load())
                    ImGuiMCP::Text("Reference %08X", confirmation.actor);
                const bool allowed = CanRequest(state, confirmation.action);
                BeginDisabled(!allowed);
                if (ImGuiMCP::Button(label)) {
                    if (confirmation.party)
                        mod::debug::RequestPartyAction(confirmation.action, confirmation.generation);
                    else
                        mod::debug::RequestAction(confirmation.action, confirmation.actor, confirmation.alias,
                                                  confirmation.generation);
                    confirmation.active = false;
                    ImGuiMCP::CloseCurrentPopup();
                }
                EndDisabled(!allowed);
                ImGuiMCP::SameLine();
                if (ImGuiMCP::Button("Cancel")) {
                    confirmation.active = false;
                    ImGuiMCP::CloseCurrentPopup();
                }
            }
            ImGuiMCP::EndPopup();
        }
    }

    void ControlTooltip(const char* text) {
        if (!ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled))
            return;
        ImGuiMCP::BeginTooltip();
        ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 18);
        ImGuiMCP::TextWrapped("%s", text);
        ImGuiMCP::PopTextWrapPos();
        ImGuiMCP::EndTooltip();
    }

    void DiagnosticIDs(const char* id, const std::array<std::pair<const char*, std::uint32_t>, 4>& fields) {
        if (!ImGuiMCP::BeginTable(id, 2, ImGuiMCP::ImGuiTableFlags_SizingFixedFit))
            return;
        for (const auto& [name, value] : fields) {
            ImGuiMCP::TableNextRow();
            ImGuiMCP::TableSetColumnIndex(0);
            ImGuiMCP::TextColored(Muted, "%s", name);
            ImGuiMCP::TableSetColumnIndex(1);
            ImGuiMCP::Text("%08X", value);
        }
        ImGuiMCP::EndTable();
    }

    void ActorDiagnostics(const mod::debug::Follower& row) {
        const bool wide = ContentWidth() > ImGuiMCP::GetFontSize() * 32;
        if (ImGuiMCP::BeginTable("Actor diagnostics", wide ? 2 : 1)) {
            if (wide) {
                ImGuiMCP::TableSetupColumn("References", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed,
                                           ImGuiMCP::GetFontSize() * 11);
                ImGuiMCP::TableSetupColumn("State", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch);
            }
            ImGuiMCP::TableNextColumn();
            ImGuiMCP::TextColored(Muted, "REFERENCE DATA");
            DiagnosticIDs(
                "References",
                {{{"Reference", row.formID}, {"Base", row.baseID}, {"Cell", row.cellID}, {"Package", row.packageID}}});
            ImGuiMCP::TableNextColumn();
            if (!wide)
                ImGuiMCP::Spacing();
            ImGuiMCP::TextColored(Muted, "ACTOR FLAGS");
            const bool pairs = ContentWidth() > ImGuiMCP::GetFontSize() * 20;
            if (ImGuiMCP::BeginTable("Flags", pairs ? 4 : 2, ImGuiMCP::ImGuiTableFlags_SizingFixedFit)) {
                const std::array<std::pair<const char*, bool>, 7> flags{{{"Teammate", row.teammate},
                                                                         {"Current faction", row.currentFaction},
                                                                         {"Potential follower", row.potentialFaction},
                                                                         {"3D loaded", row.loaded},
                                                                         {"Essential", row.essential},
                                                                         {"Protected", row.protectedActor},
                                                                         {"Crossfire", row.crossfire}}};
                for (const auto& [name, enabled] : flags) {
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextColored(Muted, "%s", name);
                    ImGuiMCP::TableNextColumn();
                    ImGuiMCP::TextColored(enabled ? ImGuiMCP::ImVec4{0.57f, 0.78f, 0.62f, 1} : Muted, "%s",
                                          enabled ? "Yes" : "No");
                }
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::TextColored(Muted, "Waiting value");
                ImGuiMCP::TableNextColumn();
                ImGuiMCP::Text("%.2f", row.waiting);
                ImGuiMCP::EndTable();
            }
            ImGuiMCP::EndTable();
        }
        if (!row.issues.empty()) {
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Accent);
            ImGuiMCP::TextWrapped("Issues: %s", row.issues.c_str());
            ImGuiMCP::PopStyleColor();
        }
    }

    void CommandFeedback(const mod::debug::Snapshot& state) {
        if (!state.ready) {
            ImGuiMCP::TextWrapped("Load a game to view your followers.");
        } else if (state.commandState != mod::debug::CommandState::Idle) {
            const auto color = state.commandState == mod::debug::CommandState::Failed
                                   ? ImGuiMCP::ImVec4{0.95f, 0.45f, 0.45f, 1.0f}
                               : state.busy ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f}
                                            : ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f};
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
            ImGuiMCP::TextWrapped("%s", state.commandStatus.c_str());
            ImGuiMCP::PopStyleColor();
        }
        if (state.ready && mod::debug::OptionsEnabled.load() && !state.executionStatus.empty())
            ImGuiMCP::TextWrapped("%s", state.executionStatus.c_str());
    }

    const char* FollowerStatus(const mod::debug::Follower& row) {
        if (row.dead)
            return "Dead";
        if (!row.questRestriction.empty())
            return "Quest activity";
        if (!row.adapterName.empty() && !row.adapterCommands && !row.adapterReason.empty())
            return "Unavailable";
        if (!row.adapterName.empty())
            return row.adapterStatus.c_str();
        if (!row.teammate || !row.currentFaction || (row.waiting != 0 && row.waiting != 1))
            return "Needs attention";
        return row.waiting == 1 ? "Waiting" : "Following";
    }

    ImGuiMCP::ImVec4 FollowerColor(const mod::debug::Follower& row) {
        if (row.dead || (!row.issues.empty() && row.adapterName.empty()))
            return {0.95f, 0.52f, 0.48f, 1};
        if (!row.questRestriction.empty() || (!row.adapterReason.empty() && row.adapterCommands != 7))
            return Accent;
        if (row.waiting > 0 || row.adapterStatus == "Waiting")
            return {0.57f, 0.73f, 0.88f, 1};
        return {0.57f, 0.78f, 0.62f, 1};
    }

    void RenderFollowerCommands(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        const bool canDirect = CanRequest(state, mod::debug::Action::Follow) && !row.dead;
        const auto allowed = [&](std::uint32_t command) {
            return (row.questCommands & (1u << command)) &&
                   (row.adapterName.empty() || (row.adapterCommands & (1u << command)));
        };
        const bool following = row.waiting == 0 && (row.adapterName.empty() || row.adapterStatus == "Following");
        const auto action = following ? mod::debug::Action::Wait : mod::debug::Action::Follow;
        const bool canAct = canDirect && allowed(following ? 1 : 0);
        const bool inlineActions = ContentWidth() > ImGuiMCP::GetFontSize() * 11;
        const bool inlinePromotion = ContentWidth() > ImGuiMCP::GetFontSize() * 21;
        BeginDisabled(!canAct);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Button, {Accent.x, Accent.y, Accent.z, 0.18f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Border, {Accent.x, Accent.y, Accent.z, 0.65f});
        if (ImGuiMCP::Button(mod::debug::ActionName(action)))
            mod::debug::RequestAction(action, row.formID, row.aliasID, state.generation);
        ImGuiMCP::PopStyleColor(2);
        EndDisabled(!canAct);
        if (inlineActions)
            ImGuiMCP::SameLine(0, ImGuiMCP::GetFontSize() * 0.6f);
        const bool canDismiss =
            CanRequest(state, mod::debug::Action::Dismiss) && allowed(2) && (row.adapterName.empty() || !row.dead);
        BeginDisabled(!canDismiss);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Muted);
        if (ImGuiMCP::Button("Dismiss..."))
            Confirm(mod::debug::Action::Dismiss, row, state, row.aliasID);
        ImGuiMCP::PopStyleColor();
        EndDisabled(!canDismiss);
        if (row.adapterName.empty() && row.aliasID >= 2 && row.aliasID <= 8) {
            if (inlinePromotion)
                ImGuiMCP::SameLine(0, ImGuiMCP::GetFontSize() * 0.6f);
            const bool canPromote = CanRequest(state, mod::debug::Action::PromotePrimary) && !row.dead;
            BeginDisabled(!canPromote);
            if (ImGuiMCP::Button(mod::debug::ActionName(mod::debug::Action::PromotePrimary)))
                mod::debug::RequestAction(mod::debug::Action::PromotePrimary, row.formID, row.aliasID,
                                          state.generation);
            const bool hovered = ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
            EndDisabled(!canPromote);
            if (hovered) {
                ImGuiMCP::BeginTooltip();
                ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 18);
                ImGuiMCP::TextWrapped(
                    "Move this follower to the vanilla primary slot. The current primary takes this follower's "
                    "slot.\nNeither follower is dismissed; following/waiting state and waiting deadlines are "
                    "preserved.");
                ImGuiMCP::PopTextWrapPos();
                ImGuiMCP::EndTooltip();
            }
        }
    }

    void RenderPartyCommands(const mod::debug::Snapshot& state) {
        const bool inlineActions = ContentWidth() > ImGuiMCP::GetFontSize() * 14;
        const bool living =
            std::ranges::any_of(state.followers, [](const auto& row) { return row.formID && !row.dead; });
        const bool occupied = std::ranges::any_of(state.followers, [](const auto& row) { return row.formID != 0; });
        const auto external = [&](std::uint32_t command) {
            return std::ranges::any_of(state.adaptedFollowers, [command](const auto& row) {
                return !row.dead && (row.adapterCommands & (1u << command));
            });
        };
        const bool canFollow = (living || external(0)) && CanRequest(state, mod::debug::Action::Follow);
        const bool canWait = (living || external(1)) && CanRequest(state, mod::debug::Action::Wait);
        const bool canDismiss = (occupied || external(2)) && CanRequest(state, mod::debug::Action::Dismiss);
        BeginDisabled(!canFollow);
        if (ImGuiMCP::Button(mod::debug::PartyActionName(mod::debug::Action::Follow)))
            mod::debug::RequestPartyAction(mod::debug::Action::Follow, state.generation);
        EndDisabled(!canFollow);
        ImGuiMCP::SameLine();
        BeginDisabled(!canWait);
        if (ImGuiMCP::Button(mod::debug::PartyActionName(mod::debug::Action::Wait)))
            mod::debug::RequestPartyAction(mod::debug::Action::Wait, state.generation);
        EndDisabled(!canWait);
        if (inlineActions)
            ImGuiMCP::SameLine();
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 4.5f);
        if (ImGuiMCP::BeginCombo("##party-actions", "More")) {
            BeginDisabled(!canDismiss);
            if (ImGuiMCP::Selectable("Dismiss everyone...", false))
                ConfirmParty(mod::debug::Action::Dismiss, state);
            EndDisabled(!canDismiss);
            ImGuiMCP::EndCombo();
        }
    }

    void RenderFollowerPosition(const mod::debug::Follower& row) {
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Muted);
        ImGuiMCP::TextWrapped("%s", row.location.c_str());
        if (row.distanceMeters)
            ImGuiMCP::Text("Distance: approximately %.1f m", *row.distanceMeters);
        else
            ImGuiMCP::TextUnformatted("Distance: unavailable");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker(row.distanceMeters
                       ? "Approximate straight-line distance to the player; does not measure a travel route."
                   : row.distanceUnavailable.empty() ? "Position unavailable."
                                                     : row.distanceUnavailable.c_str());
    }

    void RenderFollowerDistance(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        if (!row.distanceAvailable || row.aliasID == 1)
            return;
        const bool disabled = !CanRequest(state, mod::debug::Action::Follow) || row.dead ||
                              (!row.adapterName.empty() && !(row.adapterCommands & 1u));
        SectionGap();
        ImGuiMCP::TextColored(Muted, "FOLLOWING DISTANCE");
        if (row.individualDistance)
            ImGuiMCP::TextColored(Muted, "Individual preference");
        constexpr std::array labels{"Close##distance", "Normal##distance", "Far##distance"};
        BeginDisabled(disabled);
        const bool inlineChoices = ContentWidth() > ImGuiMCP::GetFontSize() * 13;
        for (int preset = 0; preset < static_cast<int>(labels.size()); ++preset) {
            if (preset && inlineChoices)
                ImGuiMCP::SameLine();
            if (ImGuiMCP::RadioButton(labels[preset], row.followDistance == preset))
                mod::follow_distance::Request(row.formID, static_cast<std::int32_t>(row.aliasID), preset,
                                              state.generation);
        }
        EndDisabled(disabled);
    }

    void RenderCombatRole(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        if (row.aliasID == 1 || (!row.adapterName.empty() && !row.combatRole.available))
            return;
        SectionGap();
        ImGuiMCP::TextColored(Muted, "COMBAT ROLE");
        ImGuiMCP::SameLine();
        HelpMarker(
            "Healer assists injured followers. Shield defender favors shield blocks and bashes. "
            "Archer, Mage, Frontline and Support currently store preferences without changing behavior.");
        const auto& roles = row.combatRole;
        if (!roles.available) {
            ImGuiMCP::TextWrapped("%s", roles.unavailable.c_str());
            return;
        }
        using mod::combat_roles::Role;
        const auto& selected = roles.options[static_cast<std::size_t>(roles.assigned)];
        const bool blocked = !CanRequest(state, mod::debug::Action::Follow) || row.dead;
        ImGuiMCP::SetNextItemWidth(std::min(ContentWidth(), ImGuiMCP::GetFontSize() * 14));
        if (ImGuiMCP::BeginCombo("##combat-role", mod::combat_roles::Name(roles.assigned).data())) {
            for (std::size_t index = 0; index < mod::combat_roles::RoleCount; ++index) {
                const auto role = static_cast<Role>(index);
                const auto& option = roles.options[index];
                const bool disabled = blocked || !option.eligible;
                BeginDisabled(disabled);
                if (ImGuiMCP::Selectable(mod::combat_roles::Name(role).data(), roles.assigned == role))
                    mod::combat_roles::Request(row.formID, row.aliasID, role, state.generation);
                const bool hovered = ImGuiMCP::IsItemHovered(ImGuiMCP::ImGuiHoveredFlags_AllowWhenDisabled);
                EndDisabled(disabled);
                if (hovered && !option.reason.empty()) {
                    ImGuiMCP::BeginTooltip();
                    ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 18);
                    ImGuiMCP::TextWrapped("%s", option.reason.c_str());
                    ImGuiMCP::PopTextWrapPos();
                    ImGuiMCP::EndTooltip();
                }
            }
            ImGuiMCP::EndCombo();
        }
        if (!selected.eligible || !selected.ready) {
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Accent);
            ImGuiMCP::TextWrapped("%s. Ordinary combat behavior continues.", selected.reason.c_str());
            ImGuiMCP::PopStyleColor();
        } else if (!roles.behavior.empty()) {
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Muted);
            ImGuiMCP::TextWrapped("%s", roles.behavior.c_str());
            ImGuiMCP::PopStyleColor();
        }
    }

    void RenderFollowerRepairs(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        if (!ImGuiMCP::CollapsingHeader("Diagnostics and repairs"))
            return;
        ImGuiMCP::Spacing();
        ImGuiMCP::TextColored(Muted, "%s | Alias %u", row.aliasName.c_str(), row.aliasID);
        ImGuiMCP::Spacing();
        ActorDiagnostics(row);
        if (row.aliasID != 1) {
            SectionGap();
            ImGuiMCP::TextColored(Muted, "PACKAGE CONTROL");
            bool enabled = row.packageControl.enabled;
            const bool disabled = !enabled && (!CanRequest(state, mod::debug::Action::Repair) || row.dead);
            BeginDisabled(disabled);
            if (ImGuiMCP::Checkbox("Force YLIWF packages (debug)", &enabled))
                mod::package_control::Request(row.formID, row.aliasID, enabled, state.generation);
            EndDisabled(disabled);
            ImGuiMCP::SameLine();
            HelpMarker(
                "Prioritizes this follower's YLIWF packages over other controllers and remembers displaced "
                "temporary packages for restoration. Scenes and quest restrictions retain control. "
                "Resets on dismissal, save load or disabling debug options.");
            if (enabled || row.packageControl.policyRequested) {
                if (row.packageControl.policyRequested)
                    ImGuiMCP::TextWrapped("A follower policy is requesting package control.");
                ImGuiMCP::TextWrapped("%s", row.packageControl.status.c_str());
                DiagnosticIDs("Package control", {{{"Requested", row.packageControl.requestedPackage},
                                                   {"Running", row.packageID},
                                                   {"Previous", row.packageControl.previousPackage},
                                                   {"Saved interrupt", row.packageControl.savedInterrupt}}});
            }
        }
        SectionGap();
        ImGuiMCP::TextColored(Muted, "REPAIR ACTIONS");
        const bool inlineActions = ContentWidth() > ImGuiMCP::GetFontSize() * 27;
        const bool canRepair = CanRequest(state, mod::debug::Action::Repair) && !row.dead;
        BeginDisabled(!canRepair);
        if (ImGuiMCP::Button("Repair flags / duplicates"))
            mod::debug::RequestAction(mod::debug::Action::Repair, row.formID, row.aliasID, state.generation);
        EndDisabled(!canRepair);
        ControlTooltip("Reconcile follower flags and duplicate slot registrations.");
        if (row.adapterName.empty()) {
            if (inlineActions)
                ImGuiMCP::SameLine();
            BeginDisabled(!canRepair);
            if (ImGuiMCP::Button("Clear movement offset"))
                mod::formation::RequestOffsetCleanup(row.formID, state.generation);
            EndDisabled(!canRepair);
            ControlTooltip(
                "Clears a saved KeepOffsetFromActor constraint for this follower.\n"
                "Use if turning in place still makes them circle behind you with formation disabled.\n"
                "This also clears an offset assigned by another mod; that mod may apply it again.");
        }
        if (inlineActions)
            ImGuiMCP::SameLine();
        const bool canClear = CanRequest(state, mod::debug::Action::ClearSlot);
        BeginDisabled(!canClear);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Muted);
        if (ImGuiMCP::Button("Clear this slot..."))
            Confirm(mod::debug::Action::ClearSlot, row, state, row.aliasID);
        ImGuiMCP::PopStyleColor();
        EndDisabled(!canClear);
    }

    void RenderAdapterActions(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        if (row.adapterActions.empty())
            return;
        SectionGap();
        ImGuiMCP::SeparatorText("ADAPTER OPTIONS");
        for (const auto& action : row.adapterActions) {
            ImGuiMCP::PushID(static_cast<int>(action.id));
            const bool disabled = !state.ready || state.busy || !action.enabled;
            BeginDisabled(disabled);
            bool invoke = false;
            bool value = action.value != 0;
            if (action.kind == yliwf::sdk::ActionKind::Toggle)
                invoke = ImGuiMCP::Checkbox(action.label, &value);
            else
                invoke = ImGuiMCP::Button(action.label);
            if (invoke)
                mod::debug::RequestAdapterAction(
                    row.formID, row.adapterID, action.id,
                    action.kind == yliwf::sdk::ActionKind::Toggle ? static_cast<std::uint32_t>(value) : 0,
                    state.generation);
            EndDisabled(disabled);
            if (action.description[0])
                ControlTooltip(action.description);
            if (!action.enabled && action.reason[0])
                ImGuiMCP::TextWrapped("%s", action.reason);
            ImGuiMCP::PopID();
        }
    }

    void RenderFollowerRow(const mod::debug::Follower& row, const mod::debug::Snapshot& state, bool debug) {
        ImGuiMCP::SetWindowFontScale(1.2f);
        ImGuiMCP::TextWrapped("%s", row.name.empty() ? "Unnamed follower" : row.name.c_str());
        ImGuiMCP::SetWindowFontScale(1.0f);
        const char* kind = !row.adapterName.empty() ? row.adapterName.c_str()
                           : row.aliasID == 0       ? "Primary follower"
                           : row.aliasID == 1       ? "Animal companion"
                                                    : "Follower";
        ImGuiMCP::TextColored(Muted, "%s", kind);
        ImGuiMCP::SameLine();
        ImGuiMCP::TextColored(FollowerColor(row), "| %s", FollowerStatus(row));
        ImGuiMCP::Spacing();
        RenderFollowerPosition(row);
        SectionGap();
        if (!row.questRestriction.empty() || !row.adapterReason.empty()) {
            ImGuiMCP::TextColored(Accent, "%s",
                                  row.questRestriction.empty() ? "CONTROLLER NOTICE" : "QUEST RESTRICTION");
            ImGuiMCP::TextWrapped(
                "%s", row.questRestriction.empty() ? row.adapterReason.c_str() : row.questRestriction.c_str());
            SectionGap();
        }
        RenderFollowerCommands(row, state);
        RenderFollowerDistance(row, state);
        RenderCombatRole(row, state);
        RenderAdapterActions(row, state);
        if (debug) {
            SectionGap();
            if (row.adapterName.empty())
                RenderFollowerRepairs(row, state);
            else if (ImGuiMCP::CollapsingHeader("Diagnostics"))
                ActorDiagnostics(row);
        }
    }

    struct FollowerSelection {
        std::uint32_t actor = 0, alias = 0;
        bool adapted = false;

        bool Matches(const mod::debug::Follower& row) const {
            return actor == row.formID && alias == row.aliasID && adapted == !row.adapterName.empty();
        }

        void Select(const mod::debug::Follower& row) {
            actor = row.formID;
            alias = row.aliasID;
            adapted = !row.adapterName.empty();
        }
    };

    std::string FitRosterText(std::string text, float width) {
        ImGuiMCP::ImVec2 size;
        ImGuiMCP::CalcTextSize(&size, text.c_str(), nullptr, false, -1);
        if (size.x <= width)
            return text;
        while (!text.empty()) {
            auto end = text.size();
            do {
                --end;
            } while (end && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80);
            text.resize(end);
            const auto shortened = text + "...";
            ImGuiMCP::CalcTextSize(&size, shortened.c_str(), nullptr, false, -1);
            if (size.x <= width)
                return shortened;
        }
        return "...";
    }

    void RenderRosterEntry(const mod::debug::Follower& row, FollowerSelection& selection) {
        const float em = ImGuiMCP::GetFontSize();
        const float padding = em * 0.35f;
        const float width = ContentWidth();
        const float height = em * 2.65f;
        ImGuiMCP::ImVec2 start, after;
        ImGuiMCP::GetCursorPos(&start);
        const bool selected = selection.Matches(row);
        ImGuiMCP::PushID(static_cast<int>(row.formID));
        ImGuiMCP::PushID(static_cast<int>(row.aliasID));
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Header, {Accent.x, Accent.y, Accent.z, 0.18f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderHovered, {1, 1, 1, 0.09f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_HeaderActive, {Accent.x, Accent.y, Accent.z, 0.28f});
        if (ImGuiMCP::Selectable("##follower", selected, 0, {width, height}))
            selection.Select(row);
        const bool hovered = ImGuiMCP::IsItemHovered();
        ImGuiMCP::PopStyleColor(3);
        ImGuiMCP::GetCursorPos(&after);
        ImGuiMCP::SetCursorPos({start.x + padding, start.y + padding});
        const auto name = FitRosterText(row.name.empty() ? "Unnamed follower" : row.name, width - padding * 2);
        ImGuiMCP::TextColored(selected ? Accent : ImGuiMCP::ImVec4{0.93f, 0.93f, 0.93f, 1}, "%s", name.c_str());
        std::string summary = FollowerStatus(row);
        if (row.distanceMeters)
            summary += fmt::format("  |  {:.0f} m", *row.distanceMeters);
        else if (!row.location.empty())
            summary += "  |  " + row.location;
        summary = FitRosterText(std::move(summary), width - padding * 2);
        ImGuiMCP::SetCursorPosX(start.x + padding);
        ImGuiMCP::TextColored(FollowerColor(row), "%s", summary.c_str());
        ImGuiMCP::SetCursorPos(after);
        if (hovered) {
            ImGuiMCP::BeginTooltip();
            ImGuiMCP::PushTextWrapPos(em * 18);
            ImGuiMCP::TextUnformatted(row.name.c_str());
            ImGuiMCP::TextColored(FollowerColor(row), "%s", FollowerStatus(row));
            ImGuiMCP::TextWrapped("%s", row.location.c_str());
            ImGuiMCP::PopTextWrapPos();
            ImGuiMCP::EndTooltip();
        }
        ImGuiMCP::PopID();
        ImGuiMCP::PopID();
        ImGuiMCP::Dummy({0, em * 0.15f});
    }

    void RenderFollowerPanels(const mod::debug::Snapshot& state, bool debug) {
        static FollowerSelection selection;
        static std::uint64_t generation = 0;
        static bool showEmpty = false;
        if (generation != state.generation) {
            selection = {};
            generation = state.generation;
        }
        std::vector<const mod::debug::Follower*> roster;
        std::unordered_set<std::uint32_t> seen;
        for (const auto* rows : {&state.followers, &state.adaptedFollowers})
            for (const auto& row : *rows)
                if (row.formID && (seen.insert(row.formID).second || debug))
                    roster.push_back(&row);
        if (roster.empty()) {
            SectionGap();
            ImGuiMCP::TextUnformatted("Your party is empty.");
            ImGuiMCP::TextColored(Muted, "Recruited followers will appear here.");
            if (debug && ImGuiMCP::CollapsingHeader("Empty follower slots"))
                for (const auto& row : state.followers)
                    if (!row.formID)
                        ImGuiMCP::TextColored(Muted, "%s: Empty", row.aliasName.c_str());
            return;
        }
        const auto findSelected = [&]() -> const mod::debug::Follower* {
            const auto found = std::ranges::find_if(roster, [&](const auto* row) { return selection.Matches(*row); });
            return found == roster.end() ? nullptr : *found;
        };
        if (!findSelected())
            selection.Select(*roster.front());
        const float em = ImGuiMCP::GetFontSize();
        ImGuiMCP::ImVec2 available;
        ImGuiMCP::GetContentRegionAvail(&available);
        const bool sideBySide = available.x >= em * 25;
        const float rosterWidth = sideBySide ? std::clamp(available.x * 0.32f, em * 7.5f, em * 10) : 0;
        const float rosterHeight = sideBySide ? 0 : std::clamp(available.y * 0.38f, em * 3.5f, em * 8.4f);
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_WindowPadding, {em * 0.5f, em * 0.5f});
        ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ChildBorderSize, 1);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_ChildBg, {1, 1, 1, 0.025f});
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Border, {1, 1, 1, 0.14f});
        if (ImGuiMCP::BeginChild("Follower roster", {rosterWidth, rosterHeight}, ImGuiMCP::ImGuiChildFlags_Border)) {
            ImGuiMCP::TextColored(Muted, "YOUR FOLLOWERS");
            ImGuiMCP::Spacing();
            for (const auto* row : roster)
                RenderRosterEntry(*row, selection);
            if (debug) {
                ImGuiMCP::Checkbox("Show empty slots", &showEmpty);
                if (showEmpty)
                    for (const auto& row : state.followers)
                        if (!row.formID)
                            ImGuiMCP::TextColored(Muted, "%s: Empty", row.aliasName.c_str());
            }
        }
        ImGuiMCP::EndChild();
        if (sideBySide)
            ImGuiMCP::SameLine(0, em * 0.5f);
        else
            ImGuiMCP::Dummy({0, em * 0.35f});
        const float detailsHeight = sideBySide ? 0 : std::max(em * 9, available.y - rosterHeight - em * 0.5f);
        if (ImGuiMCP::BeginChild("Follower details", {0, detailsHeight}, ImGuiMCP::ImGuiChildFlags_Border)) {
            if (const auto* row = findSelected()) {
                ImGuiMCP::PushID(static_cast<int>(row->formID));
                ImGuiMCP::PushID(static_cast<int>(row->aliasID));
                RenderFollowerRow(*row, state, debug);
                ImGuiMCP::PopID();
                ImGuiMCP::PopID();
            }
        }
        ImGuiMCP::EndChild();
        ImGuiMCP::PopStyleColor(2);
        ImGuiMCP::PopStyleVar(2);
    }

    void RenderInstallationDiagnostics(const mod::debug::Snapshot& state) {
        if (ImGuiMCP::CollapsingHeader("Installation")) {
            ImGuiMCP::TextWrapped("DialogueFollower record: %s", state.winningPlugin.c_str());
            ImGuiMCP::Text("Configured cap: %d", state.configuredCap);
            ImGuiMCP::Text("Validated slots: %d / 8", state.slotCapacity);
            ImGuiMCP::Text("Resolved extra aliases: %d / 7", state.boundExtraAliases);
        }
    }

    void RenderPartyDiagnostics(const mod::debug::Snapshot& state) {
        if (!ImGuiMCP::CollapsingHeader("Party health"))
            return;
        ImGuiMCP::Text("Followers: %d / %d", state.liveCount, state.cap);
        if (ImGuiMCP::TreeNode("Raw counts and gates")) {
            ImGuiMCP::Text("Party count: %.0f", state.modCount);
            ImGuiMCP::Text("Vanilla gate: %.0f | Recruit gate: %.0f", state.vanillaCount, state.recruitGate);
            ImGuiMCP::TextWrapped("The vanilla dialogue gate may temporarily be zero during recruitment.");
            ImGuiMCP::TreePop();
        }
        const bool canSync = CanRequest(state, mod::debug::Action::Sync);
        BeginDisabled(!canSync);
        if (ImGuiMCP::Button("Reconcile counts / dead slots"))
            mod::debug::RequestAction(mod::debug::Action::Sync, 0, -1, state.generation);
        EndDisabled(!canSync);
        ControlTooltip("Recalculate party counts and clear dead slots. Individual actor repairs are on Followers.");
    }

    bool ParseReferenceFormID(std::string_view input, std::uint32_t& formID) {
        if (input.starts_with("0x") || input.starts_with("0X"))
            input.remove_prefix(2);
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), formID, 16);
        return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size() && formID;
    }

    void RenderActorInspector(const mod::debug::Snapshot& state) {
        if (!ImGuiMCP::CollapsingHeader("Actor inspector"))
            return;
        static char referenceID[16]{};
        static std::string inputError;
        ImGuiMCP::TextUnformatted("Reference FormID (hex)");
        ImGuiMCP::SetNextItemWidth(ImGuiMCP::GetFontSize() * 8);
        ImGuiMCP::InputText("##Reference FormID", referenceID, sizeof(referenceID));
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Inspect")) {
            std::uint32_t id = 0;
            if (ParseReferenceFormID(referenceID, id)) {
                inputError.clear();
                mod::debug::RequestRefresh(id);
            } else {
                inputError = "Enter an actor reference FormID, for example 000A2C94.";
            }
        }
        if (!inputError.empty())
            ImGuiMCP::TextWrapped("%s", inputError.c_str());
        if (!state.inspected.formID)
            return;
        ImGuiMCP::TextUnformatted(state.inspected.name.c_str());
        ActorDiagnostics(state.inspected);
        const bool canRestore =
            CanRequest(state, mod::debug::Action::RestoreRecruitment) && state.inspected.recruitmentCompatible;
        BeginDisabled(!canRestore);
        if (ImGuiMCP::Button("Restore follower dialogue..."))
            Confirm(mod::debug::Action::RestoreRecruitment, state.inspected, state, -1);
        EndDisabled(!canRestore);
        HelpMarker(
            "For NPCs with authored follower membership or a standard follower voice. Recruitment restrictions, "
            "scenes and external follower controllers must permit the actor.");
        ImGuiMCP::TextWrapped(
            "Test recruitment requires a living potential follower and a free slot. Release is only for actors without "
            "a party registration.");
        const bool canRecruit = CanRequest(state, mod::debug::Action::Adopt) && !state.inspected.dead;
        BeginDisabled(!canRecruit);
        if (ImGuiMCP::Button("Recruit into party (debug)"))
            Confirm(mod::debug::Action::Adopt, state.inspected, state, -1);
        EndDisabled(!canRecruit);
        ImGuiMCP::SameLine();
        const bool canRelease = CanRequest(state, mod::debug::Action::Release);
        BeginDisabled(!canRelease);
        if (ImGuiMCP::Button("Release orphan service flags"))
            Confirm(mod::debug::Action::Release, state.inspected, state, -1);
        EndDisabled(!canRelease);
    }
}

void mod::ui::Register() {
    if (!SKSEMenuFramework::IsInstalled())
        return;
    // Optional newer export: use the main menu's atomic IsOpen when available,
    // including non-pausing menus. Older framework versions expose blocking state.
    getMainWindow = SKSEMenuFramework::Model::Internal::GetFunction<GetMainWindow>("GetMainWindow");
    SKSEMenuFramework::AddHudElement(ObserveVisibility);
    SKSEMenuFramework::SetSection(mod::info::DisplayName);
    SKSEMenuFramework::AddSectionItem("Followers", mod::ui::RenderFollowers);
    SKSEMenuFramework::AddSectionItem("Settings", mod::ui::RenderSettings);
    SKSEMenuFramework::AddSectionItem("Hotkeys", mod::ui::RenderHotkeys);
    SKSEMenuFramework::AddSectionItem("Debug", mod::ui::RenderDebug);
}

bool mod::ui::IsRefreshVisible() {
    if (!snapshotPageVisible.load())
        return false;
    if (auto* window = mainWindow.load())
        return window->IsOpen.load();
    return SKSEMenuFramework::IsAnyBlockingWindowOpened();
}

bool mod::ui::IsMenuOpen() {
    if (!SKSEMenuFramework::IsInstalled())
        return false;
    if (!mainWindow.load() && getMainWindow)
        mainWindow = getMainWindow();
    if (const auto* window = mainWindow.load())
        return window->IsOpen.load();
    return SKSEMenuFramework::IsAnyBlockingWindowOpened();
}

void __stdcall mod::ui::RenderHotkeys() {
    snapshotPageVisible = false;
    mod::debug::SetRefreshVisible(false);
    lastPage = Page::Hotkeys;
    lastPageFrame = ImGuiMCP::GetFrameCount();
    hotkeys_page::Render();
}

void __stdcall mod::ui::RenderFollowers() {
    const auto state = PageSnapshot(Page::Followers);
    const bool debug = mod::debug::OptionsEnabled.load();
    ImGuiMCP::TextColored(Muted, "PARTY");
    ImGuiMCP::SameLine();
    ImGuiMCP::Text("%zu followers", static_cast<std::size_t>(state.liveCount) + state.adaptedFollowers.size());
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Refresh"))
        mod::debug::RequestRefresh();
    ImGuiMCP::Spacing();
    CommandFeedback(state);
    if (!state.ready) {
        RenderConfirmation(state);
        return;
    }
    RenderPartyCommands(state);
    ImGuiMCP::Spacing();
    bool standDown = mod::combat_orders::Enabled.load();
    const bool canChangeCombatOrder = mod::combat_orders::Ready.load() &&
                                      (standDown || (state.structureReady && state.scriptReady && state.bindingsReady));
    BeginDisabled(!canChangeCombatOrder);
    if (ImGuiMCP::Checkbox("Stand down", &standDown))
        mod::combat_orders::Set(standDown);
    EndDisabled(!canChangeCombatOrder);
    ImGuiMCP::SameLine();
    HelpMarker(
        "Everyone follows or waits without fighting and avoids nearby enemies. Healers can heal followers, "
        "but not the player. Quest restrictions retain priority. Resets on save load.");
    if (standDown) {
        ImGuiMCP::TextColored(Muted, "%zu responding | %zu unavailable", mod::combat_orders::Affected.load(),
                              mod::combat_orders::Unavailable.load());
        if (debug)
            ImGuiMCP::Text("Suppressed combat updates: %llu | Combat re-entry failures: %llu",
                           static_cast<unsigned long long>(mod::combat_orders::SuppressedUpdates.load()),
                           static_cast<unsigned long long>(mod::combat_orders::Reentries.load()));
    }
    if (!state.structureReady || !state.scriptReady || !state.bindingsReady)
        ImGuiMCP::TextWrapped(
            "Follower controls are unavailable. Enable debug options on the Debug page to check the installation.");
    SectionGap();
    RenderFollowerPanels(state, debug);
    RenderConfirmation(state);
}

void __stdcall mod::ui::RenderDebug() {
    const auto state = PageSnapshot(Page::Debug);
    ImGuiMCP::ImVec2 available;
    ImGuiMCP::GetContentRegionAvail(&available);
    ImGuiMCP::BeginChild("Debug body", {0, std::max(available.y - ImGuiMCP::GetFontSize() * 4.5f, 1.0f)});
    const bool needsAttention = !state.issues.empty() || !state.installationIssues.empty() ||
                                (state.ready && (!state.structureReady || !state.bindingsReady || !state.scriptReady));
    ImGuiMCP::TextColored(state.ready && !needsAttention ? ImGuiMCP::ImVec4{0.55f, 0.80f, 0.60f, 1} : Accent, "%s",
                          !state.ready     ? "No game loaded"
                          : needsAttention ? "Needs attention"
                                           : "Ready");
    ImGuiMCP::TextWrapped("%s", state.status.c_str());
    if (!state.issues.empty())
        ImGuiMCP::TextWrapped("%s", state.issues.c_str());
    if (!state.installationIssues.empty())
        ImGuiMCP::TextWrapped("%s", state.installationIssues.c_str());
    SectionGap();
    if (ImGuiMCP::Button("Refresh state"))
        mod::debug::RequestRefresh();
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Dump full context"))
        mod::debug::RequestDump();
    ControlTooltip("Write a detailed diagnostic snapshot to the mod log.");
    SectionGap();
    ImGuiMCP::SeparatorText("TOOLS");
    bool enabled = mod::debug::OptionsEnabled.load();
    if (ImGuiMCP::Checkbox("Enable diagnostics and repairs", &enabled))
        mod::debug::OptionsEnabled = enabled;
    ControlTooltip("Also shows individual follower diagnostics on the Followers tab.");
    SectionGap();
    if (enabled && state.ready) {
        RenderPartyDiagnostics(state);
        RenderActorInspector(state);
    }
    RenderInstallationDiagnostics(state);
    SectionGap();
    ImGuiMCP::SeparatorText("LOGGING");
    bool logging = mod::debug::Logging.load();
    if (ImGuiMCP::Checkbox("Log mod flow", &logging))
        mod::debug::SetLogging(logging);
    ImGuiMCP::TextColored(Muted, "Record controller activity for troubleshooting.");
    ImGuiMCP::EndChild();
    PreferencesFooter(false);
    RenderConfirmation(state);
}

void __stdcall mod::ui::RenderSettings() {
    snapshotPageVisible = false;
    mod::debug::SetRefreshVisible(false);
    lastPage = Page::Settings;
    std::scoped_lock lock(mod::settings::Mutex);
    using namespace mod::settings;
    bool changed = false;
    ImGuiMCP::ImVec2 available;
    ImGuiMCP::GetContentRegionAvail(&available);
    ImGuiMCP::BeginChild("Settings body", {0, std::max(available.y - ImGuiMCP::GetFontSize() * 6.0f, 1.0f)});
    ImGuiMCP::SeparatorText("PARTY SIZE");
    constexpr std::array modes{"Fixed limit", "Unlocked by perks", "Scaled by Speech"};
    ImGuiMCP::SetNextItemWidth(std::min(ContentWidth(), ImGuiMCP::GetFontSize() * 18));
    if (ImGuiMCP::BeginCombo("##Party mode", modes[std::clamp(FollowerPerkOption, 0, 2)])) {
        for (int i = 0; i < static_cast<int>(modes.size()); ++i) {
            if (ImGuiMCP::Selectable(modes[i], FollowerPerkOption == i)) {
                FollowerPerkOption = i;
                changed = true;
            }
        }
        ImGuiMCP::EndCombo();
    }
    ImGuiMCP::SetNextItemWidth(std::min(ContentWidth() * 0.65f, ImGuiMCP::GetFontSize() * 18));
    if (FollowerPerkOption == 0) {
        int total = MaxExtraFollowers + 1;
        if (ImGuiMCP::SliderInt("Followers", &total, 1, 8)) {
            MaxExtraFollowers = total - 1;
            changed = true;
        }
    } else if (FollowerPerkOption == 1) {
        ImGuiMCP::TextWrapped("Each owned perk unlocks an extra follower slot.");
        ImGuiMCP::SetNextItemWidth(-1);
        if (ImGuiMCP::InputText("##Perk list", PerkListBuffer, sizeof(PerkListBuffer))) {
            ParsePerkListIntoSpecs(PerkListBuffer);
            changed = true;
        }
        ControlTooltip("Comma-separated PluginName.esp|FormID entries, for example Skyrim.esm|00058F75.");
        ImGuiMCP::TextColored(Muted, "%zu / %zu perks parsed", PerkSpecCount, kMaxPerkSpecs);
    } else {
        changed |= ImGuiMCP::SliderInt("Speech levels per extra slot", &SpeechLevelsPerSlot, 1, 30);
    }
    SectionGap();
    ImGuiMCP::SeparatorText("TRAVEL");
    ImGuiMCP::TextUnformatted("Following distance");
    constexpr std::array distances{"Close", "Normal", "Far"};
    for (int i = 0; i < static_cast<int>(distances.size()); ++i) {
        if (i)
            ImGuiMCP::SameLine();
        if (ImGuiMCP::RadioButton(distances[i], FollowDistance == i)) {
            FollowDistance = i;
            changed = true;
            if (FollowDistanceCallback)
                FollowDistanceCallback();
        }
    }
    if (ImGuiMCP::Button("Apply distance to party") && FollowDistanceCallback)
        FollowDistanceCallback();
    ControlTooltip("Replaces individual distance choices. The game save retains this party default.");
    SectionGap();
    ImGuiMCP::TextUnformatted("Formation (experimental)");
    ImGuiMCP::SetNextItemWidth(std::min(ContentWidth(), ImGuiMCP::GetFontSize() * 18));
    if (ImGuiMCP::BeginCombo("##Formation", mod::formation_geometry::Name(Formation).data())) {
        for (const auto style : {mod::formation_geometry::Style::Off, mod::formation_geometry::Style::Column,
                                 mod::formation_geometry::Style::Line, mod::formation_geometry::Style::Wedge}) {
            if (ImGuiMCP::Selectable(mod::formation_geometry::Name(style).data(), Formation == style)) {
                Formation = style;
                changed = true;
                if (FormationCallback)
                    FormationCallback();
            }
        }
        ImGuiMCP::EndCombo();
    }
    ControlTooltip(
        "Supported followers use distance choices for spacing. Quest behavior keeps priority.\n"
        "Formation pauses in settlements, interiors, combat, dialogue, scenes, waiting and mounted or swimming "
        "movement.");
    ImGuiMCP::TextWrapped("%s", mod::formation::Status().c_str());
    const auto option = [&](const char* label, bool& value, const char* description, auto callback) {
        if (ImGuiMCP::Checkbox(label, &value)) {
            changed = true;
            if (callback)
                callback();
        }
        ImGuiMCP::Indent(ImGuiMCP::GetFontSize());
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, Muted);
        ImGuiMCP::TextWrapped("%s", description);
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::Unindent(ImGuiMCP::GetFontSize());
        SectionGap();
    };
    SectionGap();
    ImGuiMCP::SeparatorText("PROTECTION");
    option("Essential followers", FollowerEssential, "Prevent follower deaths while they are in your party.",
           EssentialCallback);
    option("Friendly fire protection", FriendlyFire, "Prevent your attacks from damaging followers during combat.",
           FriendlyFireCallback);
    option("Follower crossfire protection", FollowerCrossfire, "Prevent followers from damaging other party members.",
           CrossfireCallback);
    ImGuiMCP::SeparatorText("FOLLOWER BEHAVIOR");
    option("Sandbox when idle", FollowerSandbox,
           "Allow wandering, sitting and idle activities in homes and settlements.", SandboxCallback);
    option("Assign follower homes", FollowerHomes,
           "Enable home dialogue and send dismissed followers to their assigned homes.", HomesCallback);
    ImGuiMCP::SeparatorText("NEW GAME DEFAULTS");
    ImGuiMCP::TextWrapped(
        "Existing saves keep their gameplay settings. The INI supplies defaults for new games and saves without stored "
        "settings.");
    if (ImGuiMCP::Button("Save current settings as INI defaults"))
        SaveDefaults();
    ImGuiMCP::EndChild();
    PreferencesFooter();
    if (changed && ApplyGateCallback)
        ApplyGateCallback();
}
