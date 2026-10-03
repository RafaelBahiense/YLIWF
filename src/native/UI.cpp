#include "UI.h"

#include "Settings.h"
#include "Debug.h"
#include "DebugRules.h"
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

static bool StyledRadio(const char* label, const char* desc, bool selected) {
    ImGuiMCP::ImVec4 labelCol = selected ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.90f, 0.90f, 0.90f, 1.0f};
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, labelCol);
    bool hit = ImGuiMCP::RadioButton(label, selected);
    ImGuiMCP::PopStyleColor();
    ImGuiMCP::SameLine();
    ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.50f, 0.50f, 0.50f, 1.0f});
    ImGuiMCP::TextUnformatted(desc);
    ImGuiMCP::PopStyleColor();
    return hit;
}

namespace {
    enum class Page { Followers, Settings, Debug };
    Page lastPage = Page::Settings;
    int lastPageFrame = -1;
    std::atomic_bool snapshotPageVisible{false};
    std::atomic<SKSEMenuFramework::Model::WindowInterface*> mainWindow{nullptr};
    using GetMainWindow = SKSEMenuFramework::Model::WindowInterface* (*)();
    GetMainWindow getMainWindow = nullptr;

    // The framework calls HUD observers before section pages every frame. This
    // observer only tracks visibility; it never reads Skyrim or queues a refresh.
    void __stdcall ObserveVisibility() {
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
        if (getMainWindow && !mainWindow.load()) mainWindow = getMainWindow();
        if (page != lastPage || lastPageFrame != ImGuiMCP::GetFrameCount() - 1) mod::debug::NotifyStateChanged();
        lastPage = page;
        lastPageFrame = ImGuiMCP::GetFrameCount();
        snapshotPageVisible = true;
        mod::debug::SetRefreshVisible(true);
        mod::debug::RefreshVisiblePage();
        return mod::debug::GetSnapshot();
    }

    bool CanRequest(const mod::debug::Snapshot& state, mod::debug::Action action) {
        return state.ready && !state.busy && mod::debug_rules::CanCommand(state.structureReady, state.scriptReady,
            state.bindingsReady, static_cast<std::int32_t>(action), mod::debug::OptionsEnabled.load());
    }

    void Confirm(mod::debug::Action action, const mod::debug::Follower& row, const mod::debug::Snapshot& state, std::int32_t alias) {
        confirmation = {action, row.formID, alias, state.generation, row.name.empty() ? "Unnamed follower" : row.name, false, true, true};
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
        if (!confirmation.active) return;
        if (ImGuiMCP::BeginPopupModal(title, &confirmation.active, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool current = state.ready && confirmation.generation == state.generation &&
                (mod::debug_rules::IsPartyAction(static_cast<std::int32_t>(confirmation.action)) || mod::debug::OptionsEnabled.load());
            if (!current) {
                confirmation.active = false;
                ImGuiMCP::CloseCurrentPopup();
            } else {
                const auto label = confirmation.party ? mod::debug::PartyActionName(confirmation.action) : mod::debug::ActionName(confirmation.action);
                ImGuiMCP::TextWrapped("%s: %s?", label, confirmation.name.c_str());
                if (!confirmation.party && mod::debug::OptionsEnabled.load()) ImGuiMCP::Text("Reference %08X", confirmation.actor);
                const bool allowed = CanRequest(state, confirmation.action);
                BeginDisabled(!allowed);
                if (ImGuiMCP::Button(label)) {
                    if (confirmation.party) mod::debug::RequestPartyAction(confirmation.action, confirmation.generation);
                    else mod::debug::RequestAction(confirmation.action, confirmation.actor, confirmation.alias, confirmation.generation);
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

    void ActorDiagnostics(const mod::debug::Follower& row) {
        ImGuiMCP::TextWrapped("Reference %08X | Base %08X | Cell %08X | Package %08X", row.formID, row.baseID, row.cellID, row.packageID);
        ImGuiMCP::TextWrapped("Waiting %.2f | Teammate %s | Current faction %s | Potential follower %s | 3D %s",
            row.waiting, row.teammate ? "yes" : "no", row.currentFaction ? "yes" : "no",
            row.potentialFaction ? "yes" : "no", row.loaded ? "loaded" : "unloaded");
        ImGuiMCP::TextWrapped("Essential %s | Protected %s | Crossfire %s",
            row.essential ? "yes" : "no", row.protectedActor ? "yes" : "no", row.crossfire ? "yes" : "no");
        if (!row.issues.empty()) ImGuiMCP::TextWrapped("Issues: %s", row.issues.c_str());
    }

    void CommandFeedback(const mod::debug::Snapshot& state) {
        if (!state.ready) {
            ImGuiMCP::TextWrapped("Load a game to view your followers.");
        } else if (state.commandState != mod::debug::CommandState::Idle) {
            const auto color = state.commandState == mod::debug::CommandState::Failed ? ImGuiMCP::ImVec4{0.95f, 0.45f, 0.45f, 1.0f} :
                state.busy ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f};
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
            ImGuiMCP::TextWrapped("%s", state.commandStatus.c_str());
            ImGuiMCP::PopStyleColor();
        }
        if (state.ready && mod::debug::OptionsEnabled.load() && !state.executionStatus.empty()) ImGuiMCP::TextWrapped("%s", state.executionStatus.c_str());
    }
    const char* FollowerStatus(const mod::debug::Follower& row) {
        if (row.dead) return "Dead";
        if (!row.teammate || !row.currentFaction || (row.waiting != 0 && row.waiting != 1)) return "Needs attention";
        return row.waiting == 1 ? "Waiting" : "Following";
    }

    void RenderFollowerCommands(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        const bool canDirect = CanRequest(state, mod::debug::Action::Follow) && !row.dead;
        BeginDisabled(!canDirect);
        if (ImGuiMCP::Button("Follow")) mod::debug::RequestAction(mod::debug::Action::Follow, row.formID, row.aliasID, state.generation);
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Wait")) mod::debug::RequestAction(mod::debug::Action::Wait, row.formID, row.aliasID, state.generation);
        EndDisabled(!canDirect);
        ImGuiMCP::SameLine();
        const bool canDismiss = CanRequest(state, mod::debug::Action::Dismiss);
        BeginDisabled(!canDismiss);
        if (ImGuiMCP::Button("Dismiss")) Confirm(mod::debug::Action::Dismiss, row, state, row.aliasID);
        EndDisabled(!canDismiss);
        if (row.aliasID >= 2 && row.aliasID <= 8) {
            ImGuiMCP::SameLine();
            const bool canPromote = CanRequest(state, mod::debug::Action::PromotePrimary) && !row.dead;
            BeginDisabled(!canPromote);
            if (ImGuiMCP::Button("Make primary"))
                mod::debug::RequestAction(mod::debug::Action::PromotePrimary, row.formID, row.aliasID, state.generation);
            EndDisabled(!canPromote);
            HelpMarker("Move this follower to the vanilla primary slot. The current primary takes this follower's slot.\nNeither follower is dismissed; following/waiting state and waiting deadlines are preserved.");
        }
    }
    void RenderPartyCommands(const mod::debug::Snapshot& state) {
        const bool living = std::ranges::any_of(state.followers, [](const auto& row) { return row.formID && !row.dead; });
        const bool occupied = std::ranges::any_of(state.followers, [](const auto& row) { return row.formID != 0; });
        const bool canFollow = living && CanRequest(state, mod::debug::Action::Follow);
        const bool canDismiss = occupied && CanRequest(state, mod::debug::Action::Dismiss);
        BeginDisabled(!canFollow);
        if (ImGuiMCP::Button("Follow All")) mod::debug::RequestPartyAction(mod::debug::Action::Follow, state.generation);
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Wait All")) mod::debug::RequestPartyAction(mod::debug::Action::Wait, state.generation);
        EndDisabled(!canFollow);
        ImGuiMCP::SameLine();
        BeginDisabled(!canDismiss);
        if (ImGuiMCP::Button("Dismiss All")) ConfirmParty(mod::debug::Action::Dismiss, state);
        EndDisabled(!canDismiss);
    }

    void RenderFollowerPosition(const mod::debug::Follower& row) {
        ImGuiMCP::TextWrapped("Location: %s", row.location.c_str());
        if (row.distanceMeters) ImGuiMCP::Text("Distance: approximately %.1f m", *row.distanceMeters);
        else ImGuiMCP::TextUnformatted("Distance: unavailable");
        ImGuiMCP::SameLine();
        HelpMarker(row.distanceMeters ? "Approximate straight-line distance to the player; does not measure a travel route." :
            row.distanceUnavailable.empty() ? "Position unavailable." : row.distanceUnavailable.c_str());
    }

    void RenderFollowerRepairs(const mod::debug::Follower& row, const mod::debug::Snapshot& state) {
        if (!ImGuiMCP::CollapsingHeader("Diagnostics and repairs")) return;
        ImGuiMCP::Text("Slot %s (alias %u)", row.aliasName.c_str(), row.aliasID);
        ActorDiagnostics(row);
        const bool canRepair = CanRequest(state, mod::debug::Action::Repair) && !row.dead;
        BeginDisabled(!canRepair);
        if (ImGuiMCP::Button("Repair flags / duplicates")) mod::debug::RequestAction(mod::debug::Action::Repair, row.formID, row.aliasID, state.generation);
        EndDisabled(!canRepair);
        ImGuiMCP::SameLine();
        const bool canClear = CanRequest(state, mod::debug::Action::ClearSlot);
        BeginDisabled(!canClear);
        if (ImGuiMCP::Button("Clear this slot")) Confirm(mod::debug::Action::ClearSlot, row, state, row.aliasID);
        EndDisabled(!canClear);
    }

    void RenderFollowerRow(const mod::debug::Follower& row, const mod::debug::Snapshot& state, bool debug) {
        if (!row.formID) {
            ImGuiMCP::Text("%s: Empty", row.aliasName.c_str());
            return;
        }
        ImGuiMCP::TextUnformatted(row.name.empty() ? "Unnamed follower" : row.name.c_str());
        ImGuiMCP::SameLine();
        ImGuiMCP::Text("- %s%s", FollowerStatus(row), row.aliasID == 0 ? " (Primary)" : "");
        RenderFollowerPosition(row);
        RenderFollowerCommands(row, state);
        if (debug) RenderFollowerRepairs(row, state);
    }

    void RenderInstallationDiagnostics(const mod::debug::Snapshot& state) {
        if (ImGuiMCP::CollapsingHeader("Installation")) {
            ImGuiMCP::TextWrapped("DialogueFollower winning record: %s | Configured cap: %d | Validated slots: %d / 8 | Resolved extra aliases: %d / 7",
                state.winningPlugin.c_str(), state.configuredCap, state.slotCapacity, state.boundExtraAliases);
            if (!state.installationIssues.empty()) ImGuiMCP::TextWrapped("%s", state.installationIssues.c_str());
        }
        if (!state.installationIssues.empty()) ImGuiMCP::TextWrapped("Installation needs attention. Expand Installation for details.");
    }

    void RenderPartyDiagnostics(const mod::debug::Snapshot& state) {
        if (!ImGuiMCP::CollapsingHeader("Party diagnostics and reconciliation")) return;
        ImGuiMCP::TextWrapped("Live followers: %d / %d | Party count: %.0f | Vanilla gate: %.0f | Recruit gate: %.0f",
            state.liveCount, state.cap, state.modCount, state.vanillaCount, state.recruitGate);
        if (!state.issues.empty()) ImGuiMCP::TextWrapped("%s", state.issues.c_str());
        ImGuiMCP::TextWrapped("A temporary zero in the vanilla dialogue gate while recruiting is expected. Individual diagnostics and repairs are on Followers.");
        const bool canSync = CanRequest(state, mod::debug::Action::Sync);
        BeginDisabled(!canSync);
        if (ImGuiMCP::Button("Reconcile counts / dead slots")) mod::debug::RequestAction(mod::debug::Action::Sync, 0, -1, state.generation);
        EndDisabled(!canSync);
    }

    bool ParseReferenceFormID(std::string_view input, std::uint32_t& formID) {
        if (input.starts_with("0x") || input.starts_with("0X")) input.remove_prefix(2);
        const auto parsed = std::from_chars(input.data(), input.data() + input.size(), formID, 16);
        return parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size() && formID;
    }

    void RenderActorInspector(const mod::debug::Snapshot& state) {
        if (!ImGuiMCP::CollapsingHeader("Actor inspection and test recruitment")) return;
        static char referenceID[16]{};
        static std::string inputError;
        ImGuiMCP::InputText("Reference FormID (hex)", referenceID, sizeof(referenceID));
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
        if (!inputError.empty()) ImGuiMCP::TextWrapped("%s", inputError.c_str());
        if (!state.inspected.formID) return;
        ImGuiMCP::TextUnformatted(state.inspected.name.c_str());
        ActorDiagnostics(state.inspected);
        ImGuiMCP::TextWrapped("Test recruitment requires a living potential follower and a free slot. Release is only for actors without a party registration.");
        const bool canRecruit = CanRequest(state, mod::debug::Action::Adopt) && !state.inspected.dead;
        BeginDisabled(!canRecruit);
        if (ImGuiMCP::Button("Recruit into party (debug)")) Confirm(mod::debug::Action::Adopt, state.inspected, state, -1);
        EndDisabled(!canRecruit);
        ImGuiMCP::SameLine();
        const bool canRelease = CanRequest(state, mod::debug::Action::Release);
        BeginDisabled(!canRelease);
        if (ImGuiMCP::Button("Release orphan service flags")) Confirm(mod::debug::Action::Release, state.inspected, state, -1);
        EndDisabled(!canRelease);
    }
}

void mod::ui::Register() {
    if (!SKSEMenuFramework::IsInstalled()) return;
    // Optional newer export: use the main menu's atomic IsOpen when available,
    // including non-pausing menus. Older framework versions expose blocking state.
    getMainWindow = SKSEMenuFramework::Model::Internal::GetFunction<GetMainWindow>("GetMainWindow");
    SKSEMenuFramework::AddHudElement(ObserveVisibility);
    SKSEMenuFramework::SetSection(mod::info::DisplayName);
    SKSEMenuFramework::AddSectionItem("Followers", mod::ui::RenderFollowers);
    SKSEMenuFramework::AddSectionItem("Settings", mod::ui::RenderSettings);
    SKSEMenuFramework::AddSectionItem("Debug", mod::ui::RenderDebug);
}

bool mod::ui::IsRefreshVisible() {
    if (!snapshotPageVisible.load()) return false;
    if (auto* window = mainWindow.load()) return window->IsOpen.load();
    return SKSEMenuFramework::IsAnyBlockingWindowOpened();
}

void __stdcall mod::ui::RenderFollowers() {
    const auto state = PageSnapshot(Page::Followers);
    const bool debug = mod::debug::OptionsEnabled.load();
    ImGuiMCP::Text("Followers: %d / %d", state.liveCount, state.cap);
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Refresh")) mod::debug::RequestRefresh();
    CommandFeedback(state);
    if (!state.ready) { RenderConfirmation(state); return; }
    RenderPartyCommands(state);
    if (!state.structureReady || !state.scriptReady || !state.bindingsReady)
        ImGuiMCP::TextWrapped("Follower controls are unavailable. Enable debug options on the Debug page to check the installation.");
    static bool showEmpty = false;
    if (debug) ImGuiMCP::Checkbox("Show empty slots", &showEmpty);
    std::unordered_set<std::uint32_t> seen;
    bool displayed = false;
    if (ImGuiMCP::BeginChild("Follower roster", ImGuiMCP::ImVec2{0, 0}, ImGuiMCP::ImGuiChildFlags_Border,
        ImGuiMCP::ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        for (const auto& row : state.followers) {
            if (!row.formID && !(debug && showEmpty)) continue;
            if (row.formID && !seen.insert(row.formID).second && !debug) continue;
            displayed = true;
            ImGuiMCP::PushID(static_cast<int>(row.aliasID));
            RenderFollowerRow(row, state, debug);
            ImGuiMCP::Separator();
            ImGuiMCP::PopID();
        }
        if (!displayed) ImGuiMCP::TextUnformatted("No followers in your party.");
    }
    ImGuiMCP::EndChild();
    RenderConfirmation(state);
}

void __stdcall mod::ui::RenderDebug() {
    bool enabled = mod::debug::OptionsEnabled.load();
    if (ImGuiMCP::Checkbox("Enable debug options", &enabled)) mod::debug::OptionsEnabled = enabled;
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Save debug settings")) mod::settings::Save();
    {
        std::scoped_lock lock(mod::settings::Mutex);
        if (!mod::settings::SaveStatus.empty()) ImGuiMCP::TextWrapped("%s", mod::settings::SaveStatus.c_str());
    }
    ImGuiMCP::TextWrapped("Debug options show follower diagnostics, state repairs and test recruitment. Flow logging is a separate setting.");
    bool logging = mod::debug::Logging.load();
    if (ImGuiMCP::Checkbox("Log mod flow", &logging)) mod::debug::SetLogging(logging);
    const auto state = PageSnapshot(Page::Debug);
    if (!enabled) { RenderConfirmation(state); return; }
    if (ImGuiMCP::Button("Refresh state")) mod::debug::RequestRefresh();
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Dump full context")) mod::debug::RequestDump();
    ImGuiMCP::TextWrapped("%s", state.status.c_str());
    if (!state.ready) { RenderConfirmation(state); return; }
    RenderInstallationDiagnostics(state);
    RenderPartyDiagnostics(state);
    RenderActorInspector(state);
    RenderConfirmation(state);
}

void __stdcall mod::ui::RenderSettings() {
    snapshotPageVisible = false;
    mod::debug::SetRefreshVisible(false);
    lastPage = Page::Settings;
    std::scoped_lock lock(mod::settings::Mutex);
    bool changed = false;

    ImGuiMCP::SetWindowFontScale(0.93f);
    ImGuiMCP::SeparatorText("FOLLOWER LIMIT MODE");

    if (StyledRadio("Max Followers", "", mod::settings::FollowerPerkOption == 0)) {
        mod::settings::FollowerPerkOption = 0;
        changed = true;
    }
    {
        bool dis = (mod::settings::FollowerPerkOption != 0);
        BeginDisabled(dis);
        ImGuiMCP::Indent(22.0f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.55f, 0.55f, 0.55f, 1.0f});
        ImGuiMCP::TextUnformatted("Max Followers");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine(160.0f);
        ImGuiMCP::SetNextItemWidth(250.0f);
        int displayMax = mod::settings::MaxExtraFollowers + 1;
        if (ImGuiMCP::SliderInt("##iMaxFollowers", &displayMax, 1, 8)) {
            mod::settings::MaxExtraFollowers = displayMax - 1;
            changed = true;
        }
        ImGuiMCP::SameLine();
        HelpMarker("Total follower slots.\n1 = vanilla (one follower).\nMaximum is 8.");
        ImGuiMCP::Unindent(22.0f);
        EndDisabled(dis);
    }

    ImGuiMCP::Spacing();

    if (StyledRadio("Perk Gated", "", mod::settings::FollowerPerkOption == 1)) {
        mod::settings::FollowerPerkOption = 1;
        changed = true;
    }
    {
        bool dis = (mod::settings::FollowerPerkOption != 1);
        BeginDisabled(dis);
        ImGuiMCP::Indent(22.0f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.55f, 0.55f, 0.55f, 1.0f});
        ImGuiMCP::TextUnformatted("Perk List");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker("Comma-separated list, up to 8 entries.\nFormat: PluginName.esp|FormID\nExample: Skyrim.esm|00058F75\nEach perk the player owns grants one extra follower slot.");
        ImGuiMCP::SetNextItemWidth(-1.0f);
        if (ImGuiMCP::InputText("##PerkList", mod::settings::PerkListBuffer, sizeof(mod::settings::PerkListBuffer))) {
            changed = true;
            mod::settings::ParsePerkListIntoSpecs(mod::settings::PerkListBuffer);
        }
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, mod::settings::PerkSpecCount > 0 ? ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f} : ImGuiMCP::ImVec4{0.50f, 0.50f, 0.50f, 1.0f});
        ImGuiMCP::Text("  %zu / %zu Perks Parsed", mod::settings::PerkSpecCount, mod::settings::kMaxPerkSpecs);
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::Unindent(22.0f);
        EndDisabled(dis);
    }

    ImGuiMCP::Spacing();

    if (StyledRadio("Speech Scaled", "- Scales With Speechcraft Skill", mod::settings::FollowerPerkOption == 2)) {
        mod::settings::FollowerPerkOption = 2;
        changed = true;
    }
    {
        bool dis = (mod::settings::FollowerPerkOption != 2);
        BeginDisabled(dis);
        ImGuiMCP::Indent(22.0f);
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.55f, 0.55f, 0.55f, 1.0f});
        ImGuiMCP::TextUnformatted("Levels Per Slot");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine(160.0f);
        ImGuiMCP::SetNextItemWidth(250.0f);
        if (ImGuiMCP::SliderInt("##iSpeechLevelsPerSlot", &mod::settings::SpeechLevelsPerSlot, 1, 30)) changed = true;
        ImGuiMCP::SameLine();
        HelpMarker("Speech levels needed for each extra follower slot.\nDefault is 10, meaning Speech 0-9 = 1 follower.");
        ImGuiMCP::Unindent(22.0f);
        EndDisabled(dis);
    }

    ImGuiMCP::Spacing();

    ImGuiMCP::SeparatorText("FEATURES");

    {
        bool ess = mod::settings::FollowerEssential;
        if (ImGuiMCP::Checkbox("##EssCheck", &ess)) {
            mod::settings::FollowerEssential = ess;
            changed = true;
            mod::settings::EssentialCallback();
        }
        ImGuiMCP::SameLine();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ess ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.90f, 0.90f, 0.90f, 1.0f});
        ImGuiMCP::TextUnformatted("Make Followers Essential");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker("Flags every actor in CurrentFollowerFaction as Essential,\nso they cannot be killed while following you.\nTheir original Essential or Protected state is fully\nrestored when they leave the party.");
        if (ess) {
            ImGuiMCP::Indent(22.0f);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f});
            ImGuiMCP::TextUnformatted("Active - Current Followers Cannot Die");
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::Unindent(22.0f);
        }
    }

    ImGuiMCP::Spacing();

    {
        bool ff = mod::settings::FriendlyFire;
        if (ImGuiMCP::Checkbox("##FFCheck", &ff)) {
            mod::settings::FriendlyFire = ff;
            changed = true;
            mod::settings::FriendlyFireCallback();
        }
        ImGuiMCP::SameLine();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ff ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.90f, 0.90f, 0.90f, 1.0f});
        ImGuiMCP::TextUnformatted("Friendly Fire Protection");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker("Your attacks, shouts, and destruction spells do no\ndamage to your followers while in combat.");
        if (ff) {
            ImGuiMCP::Indent(22.0f);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f});
            ImGuiMCP::TextUnformatted("Active - Followers Are Safe From Friendly Fire While in Combat");
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::Unindent(22.0f);
        }
    }

    ImGuiMCP::Spacing();

    {
        bool cf = mod::settings::FollowerCrossfire;
        if (ImGuiMCP::Checkbox("##CrossfireCheck", &cf)) {
            mod::settings::FollowerCrossfire = cf;
            changed = true;
            mod::settings::CrossfireCallback();
        }
        ImGuiMCP::SameLine();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, cf ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.90f, 0.90f, 0.90f, 1.0f});
        ImGuiMCP::TextUnformatted("Follower Crossfire Protection");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker("Followers cannot damage each other.\nTheir attacks, shouts and spells do no damage to anyone else in your party.\nDoes not change how much damage they deal to enemies.");
        if (cf) {
            ImGuiMCP::Indent(22.0f);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f});
            ImGuiMCP::TextUnformatted("Active - Followers Cannot Hurt Each Other");
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::Unindent(22.0f);
        }
    }

    ImGuiMCP::Spacing();

    {
        bool sb = mod::settings::FollowerSandbox;
        if (ImGuiMCP::Checkbox("##SandboxCheck", &sb)) {
            mod::settings::FollowerSandbox = sb;
            changed = true;
            mod::settings::SandboxCallback();
        }
        ImGuiMCP::SameLine();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, sb ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.90f, 0.90f, 0.90f, 1.0f});
        ImGuiMCP::TextUnformatted("Follower Sandbox");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker("Allows followers to sandbox (wander, sit, idle) in Dwellings and Habitation.\nExample: Towns, Homes and any other places marked as Dwellings and Habitation.");
        if (sb) {
            ImGuiMCP::Indent(22.0f);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f});
            ImGuiMCP::TextUnformatted("Active - Followers Will Sandbox When Idle");
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::Unindent(22.0f);
        }
    }

    ImGuiMCP::Spacing();

    {
        bool hm = mod::settings::FollowerHomes;
        if (ImGuiMCP::Checkbox("##HomesCheck", &hm)) {
            mod::settings::FollowerHomes = hm;
            changed = true;
            mod::settings::HomesCallback();
        }
        ImGuiMCP::SameLine();
        ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, hm ? ImGuiMCP::ImVec4{0.85f, 0.72f, 0.40f, 1.0f} : ImGuiMCP::ImVec4{0.90f, 0.90f, 0.90f, 1.0f});
        ImGuiMCP::TextUnformatted("Follower Homes");
        ImGuiMCP::PopStyleColor();
        ImGuiMCP::SameLine();
        HelpMarker("Adds the \"I want you to live here.\" and \"Forget the home I gave you.\" dialogue to vanilla followers.\nDismissed followers go back to the home you gave them.\nTurn this off to use another follower home mod. The dialogue disappears and assigned homes are ignored until you turn it back on.");
        if (hm) {
            ImGuiMCP::Indent(22.0f);
            ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{0.45f, 0.75f, 0.45f, 1.0f});
            ImGuiMCP::TextUnformatted("Active - Followers Can Be Given A Home");
            ImGuiMCP::PopStyleColor();
            ImGuiMCP::Unindent(22.0f);
        }
    }

    ImGuiMCP::Spacing();

    ImGuiMCP::SeparatorText("INI FILE");

    if (ImGuiMCP::Button("Save Settings")) mod::settings::Save();
    ImGuiMCP::SameLine();
    HelpMarker(fmt::format("Writes the current values to:\n{}\nChanges are already live in-game. This only saves them.", mod::info::IniPath).c_str());
    ImGuiMCP::SameLine(0.0f, 14.0f);
    if (ImGuiMCP::Button("Reload Settings")) {
        mod::settings::Load(true);
        mod::settings::FriendlyFireCallback();
        mod::settings::SandboxCallback();
        mod::settings::HomesCallback();
        mod::settings::EssentialCallback();
        changed = true;
    }
    ImGuiMCP::SameLine();
    HelpMarker("Discard unsaved UI changes and reload values from the INI file.");
    if (!mod::settings::SaveStatus.empty()) ImGuiMCP::TextWrapped("%s", mod::settings::SaveStatus.c_str());
    ImGuiMCP::SetWindowFontScale(1.0f);

    if (changed) mod::settings::ApplyGateCallback();
}
