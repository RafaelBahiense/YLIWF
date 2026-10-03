#pragma once

#include <fmt/format.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RE {
    class Actor;
    class TESQuest;
    class TESFaction;
    class SpellItem;
    class TESGlobal;
    namespace BSScript { class IVirtualMachine; }
}

namespace mod::debug {
    // UI action numbers map to native operations in ControllerExecutor.cpp.
    enum class Action : std::int32_t { Sync = 0, Repair = 1, Follow = 2, Wait = 3, Dismiss = 4, ClearSlot = 5, Adopt = 6, Release = 7, PromotePrimary = 8 };
    enum class CommandState { Idle, Pending, Succeeded, Failed };

    constexpr const char* ActionName(Action action) {
        switch (action) {
        case Action::Sync: return "Reconcile party";
        case Action::Repair: return "Repair follower";
        case Action::Follow: return "Follow";
        case Action::Wait: return "Wait";
        case Action::Dismiss: return "Dismiss";
        case Action::ClearSlot: return "Clear slot";
        case Action::Adopt: return "Recruit into party";
        case Action::Release: return "Release orphan flags";
        case Action::PromotePrimary: return "Make primary";
        }
        return "Follower command";
    }
    constexpr const char* PartyActionName(Action action) {
        switch (action) {
        case Action::Follow: return "Follow All";
        case Action::Wait: return "Wait All";
        case Action::Dismiss: return "Dismiss All";
        default: return "Party command";
        }
    }

    struct Follower {
        std::uint32_t aliasID = 0, formID = 0, baseID = 0, cellID = 0, packageID = 0;
        std::string aliasName, name, issues;
        std::string location = "Unknown", distanceUnavailable;
        std::optional<double> distanceMeters;
        bool dead = false, teammate = false, currentFaction = false, potentialFaction = false;
        bool loaded = false, essential = false, protectedActor = false, crossfire = false;
        float waiting = 0;
    };

    struct Snapshot {
        bool ready = false, busy = false;
        CommandState commandState = CommandState::Idle;
        std::uint64_t generation = 0;
        std::int32_t cap = 0, liveCount = 0;
        float vanillaCount = 0, modCount = 0, recruitGate = 0;
        std::string status = "No game loaded.", issues;
        std::string commandStatus, executionStatus;
        std::string winningPlugin, installationIssues;
        std::int32_t configuredCap = 0, slotCapacity = 0, boundExtraAliases = 0;
        bool structureReady = false, bindingsReady = false, scriptReady = false;
        std::vector<Follower> followers;
        Follower inspected;
    };

    struct Context {
        RE::TESQuest* quest = nullptr;
        RE::TESFaction* currentFaction = nullptr;
        RE::TESFaction* potentialFaction = nullptr;
        RE::SpellItem* protectionSpell = nullptr;
        RE::TESGlobal* vanillaCount = nullptr;
        RE::TESGlobal* modCount = nullptr;
        RE::TESGlobal* recruitGate = nullptr;
        std::int32_t (*getCap)() = nullptr;
        void (*syncNative)() = nullptr;
        std::vector<std::pair<std::uint32_t, std::uint8_t>> (*originalFlags)() = nullptr;
        std::int32_t (*getConfiguredCap)() = nullptr;
        bool (*isRefreshVisible)() = nullptr;
    };

    inline std::atomic_bool Logging{false};
    inline std::atomic_bool OptionsEnabled{false};
    void InitializeLog();
    void Configure(Context context);
    void SetLogging(bool enabled);
    void Trace(std::string_view event, RE::Actor* actor = nullptr, std::string_view detail = {});

    // Invoke immediately, never queue or retain the callback. Use this for getters
    // and other diagnostic work that must be skipped while logging is disabled.
    template <class Detail>
    void TraceLazy(std::string_view event, RE::Actor* actor, Detail&& makeDetail) {
        if (!Logging.load()) return;
        Trace(event, actor, std::forward<Detail>(makeDetail)());
    }

    // Argument expressions are evaluated by the caller; only formatting is deferred.
    template <class... Args> requires (sizeof...(Args) > 0)
    void Trace(std::string_view event, RE::Actor* actor, fmt::format_string<Args...> pattern, Args&&... args) {
        TraceLazy(event, actor, [&] { return fmt::format(pattern, std::forward<Args>(args)...); });
    }

    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm);
    void Invalidate();
    void GameLoaded();
    std::uint64_t Generation();
    bool IsCurrentGame(std::uint64_t generation);
    bool IsCommandCurrent(std::int32_t ticket);
    void CompleteCommand(std::int32_t ticket, bool success, std::string detail);
    void UpdateCommandProgress(std::int32_t ticket, std::string detail);
    Snapshot GetSnapshot();
    void SetRefreshVisible(bool visible);
    void NotifyStateChanged();
    void RefreshVisiblePage();
    void RequestRefresh(std::uint32_t inspectID = 0);
    void RequestDump();
    void RequestAction(Action action, std::uint32_t actorID, std::int32_t aliasID, std::uint64_t generation);
    void RequestPartyAction(Action action, std::uint64_t generation);
}
