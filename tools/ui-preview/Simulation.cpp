#include "Simulation.h"
#include "Session.h"
#include "settings/PreferenceState.h"
#include "diagnostics/Debug.h"
#include "combat/CombatOrders.h"
#include "followers/FollowDistance.h"
#include "formations/Formation.h"
#include "input/Hotkeys.h"
#include "settings/Settings.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <optional>

namespace {
    mod::debug::Snapshot state;
    std::optional<mod::hotkey_rules::Action> capture;
    std::string captureStatus;
    std::function<void()> pending;
    std::chrono::steady_clock::time_point completeAt;

    void Each(const std::function<void(mod::debug::Follower&)>& change) {
        for (auto* rows : {&state.followers, &state.adaptedFollowers})
            for (auto& row : *rows)
                if (row.formID)
                    change(row);
    }

    void Actor(std::uint32_t id, const std::function<void(mod::debug::Follower&)>& change) {
        Each([&](auto& row) {
            if (row.formID == id)
                change(row);
        });
    }

    void Queue(std::string message, std::function<void()> change) {
        if (state.busy || !state.ready)
            return;
        state.busy = true;
        state.commandState = mod::debug::CommandState::Pending;
        state.commandStatus = std::move(message);
        pending = std::move(change);
        completeAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
    }

    void Apply(mod::debug::Follower& row, mod::debug::Action action) {
        using mod::debug::Action;
        switch (action) {
            case Action::Wait:
                row.waiting = 1;
                row.adapterStatus = "Waiting";
                break;
            case Action::Follow:
                row.waiting = 0;
                row.adapterStatus = "Following";
                break;
            case Action::Dismiss:
            case Action::ClearSlot:
                row.formID = 0;
                break;
            case Action::Repair:
                row.issues.clear();
                row.teammate = row.currentFaction = true;
                break;
            case Action::RestoreRecruitment:
                row.potentialFaction = true;
                break;
            default:
                break;
        }
    }

    mod::debug::Follower SampleFollower(std::uint32_t id, std::uint32_t alias, std::string name) {
        mod::debug::Follower row;
        row.formID = id;
        row.baseID = id + 1;
        row.aliasID = alias;
        row.aliasName = alias == 0 ? "Primary" : "Extra follower";
        row.name = std::move(name);
        row.loaded = row.teammate = row.currentFaction = row.potentialFaction = true;
        row.recruitmentCompatible = true;
        row.essential = row.crossfire = true;
        row.location = "Whiterun";
        row.distanceMeters = 4.7;
        row.combatRole.available = true;
        row.combatRole.behavior = "Ordinary combat behavior";
        for (auto& option : row.combatRole.options)
            option.eligible = option.ready = true;
        return row;
    }
}

void preview::Reset(Scenario scenario) {
    const auto generation = state.generation + 1;
    state = {};
    state.generation = generation;
    pending = {};
    capture.reset();
    mod::combat_orders::Enabled = false;
    mod::combat_orders::Ready = true;
    state.ready = state.structureReady = state.bindingsReady = state.scriptReady = true;
    state.cap = state.configuredCap = 8;
    state.slotCapacity = 8;
    state.boundExtraAliases = 7;
    state.recruitGate = 1;
    state.status = "Preview: simulated party";
    state.winningPlugin = "You Lead, I Will Follow.esp";
    if (scenario == Scenario::NoGame) {
        state.ready = state.structureReady = state.bindingsReady = state.scriptReady = false;
        state.status = "No game loaded.";
        return;
    }
    if (scenario == Scenario::Empty)
        return;
    state.followers = {SampleFollower(0x100, 0, "Lydia"), SampleFollower(0x200, 2, "Fura Bloodmouth")};
    state.followers[1].waiting = 1;
    state.followers[1].location = "Angeline's Aromatics, Solitude";
    state.followers[1].distanceMeters.reset();
    state.followers[1].distanceUnavailable = "Actor is not loaded";
    state.adaptedFollowers = {SampleFollower(0x300, 99, "Inigo"), SampleFollower(0x400, 99, "Serana")};
    for (auto& row : state.adaptedFollowers) {
        row.adapterID = row.formID;
        row.adapterName = row.name;
        row.adapterCommands = 7;
        row.adapterStatus = "Following";
        row.combatRole.available = false;
        row.combatRole.unavailable = "Combat roles are controlled by this follower.";
    }
    yliwf::sdk::FollowerAction reanimation;
    reanimation.id = 1;
    reanimation.kind = yliwf::sdk::ActionKind::Toggle;
    reanimation.enabled = reanimation.value = 1;
    std::snprintf(reanimation.label, sizeof(reanimation.label), "Allow raising the dead");
    std::snprintf(reanimation.description, sizeof(reanimation.description),
                  "Allow combat AI to use reanimation spells.");
    state.adaptedFollowers[1].adapterActions = {reanimation};
    state.liveCount = 2;
    state.modCount = 2;
    state.vanillaCount = 1;
    if (scenario == Scenario::QuestLocked) {
        auto& row = state.adaptedFollowers[0];
        row.adapterCommands = 0;
        row.adapterReason = "Inigo's quest currently requires his behavior to stay unchanged.";
        state.followers[1].questCommands = 0;
        state.followers[1].questRestriction = "Fura is pursuing her personal quest.";
    } else if (scenario == Scenario::Busy) {
        state.busy = true;
        state.commandState = mod::debug::CommandState::Pending;
        state.commandStatus = "Waiting for the follower controller...";
    } else if (scenario == Scenario::Error) {
        state.commandState = mod::debug::CommandState::Failed;
        state.commandStatus = "The follower could not complete the requested command.";
        state.followers[1].teammate = false;
        state.followers[1].issues = "missing teammate flag";
    }
}

void preview::Tick() {
    if (pending && std::chrono::steady_clock::now() >= completeAt) {
        auto change = std::move(pending);
        pending = {};
        change();
        state.busy = false;
        state.commandState = mod::debug::CommandState::Succeeded;
        state.commandStatus = "Preview action completed.";
        std::erase_if(state.adaptedFollowers, [](const auto& row) { return !row.formID; });
        state.liveCount = static_cast<std::int32_t>(
            std::ranges::count_if(state.followers, [](const auto& row) { return row.formID != 0; }));
        state.modCount = static_cast<float>(state.liveCount);
    }
}

void preview::SaveSession(Session& session) {
    session.party = state;
    if (pending) {
        session.party.busy = false;
        session.party.commandState = mod::debug::CommandState::Idle;
        session.party.commandStatus = "Pending preview command cancelled by UI reload.";
    }
    using namespace mod::settings;
    session.maximum = MaxExtraFollowers;
    session.gate = FollowerPerkOption;
    session.speech = SpeechLevelsPerSlot;
    session.distance = FollowDistance;
    session.essential = FollowerEssential;
    session.friendlyFire = FriendlyFire;
    session.crossfire = FollowerCrossfire;
    session.sandbox = FollowerSandbox;
    session.homes = FollowerHomes;
    session.formation = Formation;
    session.hotkeys = Hotkeys;
    session.perks = PerkSpecs;
    session.perkCount = PerkSpecCount;
    session.perkText = PerkListBuffer;
    session.saveStatus = SaveStatus;
    session.debug = mod::debug::OptionsEnabled;
    session.logging = mod::debug::Logging;
    session.standDown = mod::combat_orders::Enabled;
    session.valid = true;
}

void preview::RestoreSession(const Session& session) {
    if (!session.valid)
        return;
    state = session.party;
    using namespace mod::settings;
    MaxExtraFollowers = session.maximum;
    FollowerPerkOption = session.gate;
    SpeechLevelsPerSlot = session.speech;
    FollowDistance = session.distance;
    FollowerEssential = session.essential;
    FriendlyFire = session.friendlyFire;
    FollowerCrossfire = session.crossfire;
    FollowerSandbox = session.sandbox;
    FollowerHomes = session.homes;
    Formation = session.formation;
    Hotkeys = session.hotkeys;
    PerkSpecs = session.perks;
    PerkSpecCount = session.perkCount;
    std::snprintf(PerkListBuffer, sizeof(PerkListBuffer), "%s", session.perkText.c_str());
    SaveStatus = session.saveStatus;
    mod::debug::OptionsEnabled = session.debug;
    mod::debug::Logging = session.logging;
    mod::combat_orders::Set(session.standDown);
    preference_state::RememberSaved();
}

void preview::CaptureKey(unsigned int key) {
    if (!capture)
        return;
    if (key == VK_ESCAPE) {
        capture.reset();
        captureStatus.clear();
        return;
    }
    if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU)
        return;
    auto binding = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
    if (GetKeyState(VK_SHIFT) < 0)
        binding |= mod::hotkey_rules::Shift;
    if (GetKeyState(VK_CONTROL) < 0)
        binding |= mod::hotkey_rules::Control;
    if (GetKeyState(VK_MENU) < 0)
        binding |= mod::hotkey_rules::Alt;
    if (const auto conflict =
            mod::hotkey_rules::Conflict(mod::settings::Hotkeys, binding, static_cast<std::size_t>(*capture))) {
        captureStatus = "Already assigned to " + std::string(mod::hotkey_rules::Definitions[*conflict].name);
    } else if (mod::hotkey_rules::Valid(binding)) {
        mod::settings::Hotkeys[static_cast<std::size_t>(*capture)] = binding;
        capture.reset();
        captureStatus.clear();
    }
}

namespace mod::debug {
    Snapshot GetSnapshot() {
        return state;
    }

    bool IsCurrentGame(std::uint64_t generation) {
        return state.ready && generation == state.generation;
    }

    void SetRefreshVisible(bool) {}

    void NotifyStateChanged() {}

    void RefreshVisiblePage() {}

    void RequestRefresh(std::uint32_t id) {
        if (id)
            state.inspected = SampleFollower(id, 99, "Inspected actor");
    }

    void RequestDump() {
        state.executionStatus = "Preview: diagnostic output is simulated.";
    }

    void SetLogging(bool enabled) {
        Logging = enabled;
    }

    void RequestAction(Action action, std::uint32_t id, std::int32_t, std::uint64_t generation) {
        if (!IsCurrentGame(generation))
            return;
        Queue(ActionName(action), [=] { Actor(id, [=](auto& row) { Apply(row, action); }); });
    }

    void RequestPartyAction(Action action, std::uint64_t generation) {
        if (!IsCurrentGame(generation))
            return;
        Queue(PartyActionName(action), [=] {
            Each([=](auto& row) {
                const auto bit = 1u << (static_cast<unsigned int>(action) - 2);
                if ((row.questCommands & bit) && (row.adapterName.empty() || (row.adapterCommands & bit)))
                    Apply(row, action);
            });
        });
    }

    void RequestAdapterAction(std::uint32_t id, yliwf::sdk::AdapterID adapter, std::uint32_t action,
                              std::uint32_t value, std::uint64_t generation) {
        if (!IsCurrentGame(generation))
            return;
        Queue("Adapter action", [=] {
            Actor(id, [=](auto& row) {
                if (row.adapterID == adapter)
                    for (auto& option : row.adapterActions)
                        if (option.id == action && option.enabled && value <= 1)
                            option.value = value;
            });
        });
    }
}

namespace mod::settings {
    void Load(bool) {
        MaxExtraFollowers = 7;
        FollowerPerkOption = 0;
        SpeechLevelsPerSlot = 10;
        FollowDistance = 1;
        Formation = formation_geometry::Style::Off;
        Hotkeys = {};
        PerkSpecs = {};
        PerkSpecCount = 0;
        PerkListBuffer[0] = '\0';
        FollowerEssential = FriendlyFire = FollowerCrossfire = FollowerSandbox = FollowerHomes = true;
        ApplyGateCallback = FriendlyFireCallback = SandboxCallback = CrossfireCallback = HomesCallback =
            EssentialCallback = +[] {};
        FollowDistanceCallback = +[] {
            Each([](auto& row) {
                row.followDistance = FollowDistance;
                row.individualDistance = false;
            });
        };
        Loaded = true;
        SaveStatus.clear();
        preference_state::RememberSaved();
    }

    bool Save() {
        SaveStatus = "Preview settings kept in memory; no INI was written.";
        preference_state::RememberSaved();
        return true;
    }

    bool SaveDefaults() {
        return Save();
    }

    void LoadGameplayDefaults() {
        const auto saved = preference_state::Saved;
        const auto hotkeys = Hotkeys;
        const auto enabled = mod::debug::OptionsEnabled.load();
        const auto logging = mod::debug::Logging.load();
        Load(true);
        Hotkeys = hotkeys;
        mod::debug::OptionsEnabled = enabled;
        mod::debug::Logging = logging;
        preference_state::Saved = saved;
    }

    void ParsePerkListIntoSpecs(const std::string&) {
        SaveStatus = "Perk resolution requires Skyrim; this preview only edits the text.";
    }
}

namespace mod::combat_orders {
    void Set(bool enabled) {
        Enabled = enabled;
        Affected = enabled ? static_cast<std::size_t>(state.liveCount) + state.adaptedFollowers.size() : 0;
    }
}

namespace mod::follow_distance {
    void Request(std::uint32_t id, std::int32_t, std::int32_t preset, std::uint64_t) {
        Actor(id, [=](auto& row) {
            row.followDistance = preset;
            row.individualDistance = true;
        });
    }
}

namespace mod::combat_roles {
    void Request(std::uint32_t id, std::uint32_t, Role role, std::uint64_t) {
        Actor(id, [=](auto& row) { row.combatRole.assigned = role; });
    }
}

namespace mod::package_control {
    void Request(std::uint32_t id, std::uint32_t, bool enabled, std::uint64_t) {
        Actor(id, [=](auto& row) {
            row.packageControl.enabled = row.packageControl.active = enabled;
            row.packageControl.status = "Preview package control";
        });
    }
}

namespace mod::formation {
    std::string Status() {
        return "Preview: formation movement is simulated.";
    }

    void RequestOffsetCleanup(std::uint32_t, std::uint64_t) {}
}

namespace mod::hotkeys {
    void BeginCapture(hotkey_rules::Action action) {
        capture = action;
        captureStatus = "Waiting for a key combination...";
    }

    void CancelCapture() {
        capture.reset();
        captureStatus.clear();
    }

    bool IsCapturing(hotkey_rules::Action action) {
        return capture == action;
    }

    void Clear(hotkey_rules::Action action) {
        settings::Hotkeys[static_cast<std::size_t>(action)] = 0;
    }

    std::string Status() {
        return capture ? captureStatus : "";
    }

    std::string Describe(std::uint32_t binding) {
        if (!binding)
            return "Unbound";
        char name[128]{};
        GetKeyNameTextA(static_cast<LONG>((binding & 0xFF) << 16), name, sizeof(name));
        return std::string(binding & hotkey_rules::Control ? "Ctrl+" : "") +
               (binding & hotkey_rules::Shift ? "Shift+" : "") + (binding & hotkey_rules::Alt ? "Alt+" : "") + name;
    }
}
