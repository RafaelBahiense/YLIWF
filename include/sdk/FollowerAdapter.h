#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Public x64 ABI. No STL types, engine pointers, allocation ownership, or
// exceptions cross the DLL boundary. Providers remain loaded for the session.
namespace yliwf::sdk {
    inline constexpr std::uint32_t InterfaceVersion = 1;
    inline constexpr char QueryExport[] = "YLIWF_GetFollowerAdapterAPI";
    using ActorID = std::uint32_t;
    using AdapterID = std::uint32_t;
    using RequestID = std::uint64_t;
    enum class Command : std::uint32_t { Follow, Wait, Dismiss };
    enum class State : std::uint32_t { Inactive, Following, Waiting, Unavailable };
    enum class StartResult : std::uint32_t { Rejected, Pending, Completed };
    enum class FollowDistance : std::uint32_t { Close, Normal, Far };

    constexpr std::uint32_t Capability(Command command) {
        return 1u << static_cast<std::uint32_t>(command);
    }

    inline constexpr std::uint32_t AllCommands = 7;

    struct FollowerState {
        std::uint32_t size = sizeof(FollowerState);
        State state = State::Unavailable;
        std::uint32_t commands = 0;
        char reason[192]{};
    };

    inline bool Supports(const FollowerState& state, Command command) {
        return static_cast<std::uint32_t>(command) <= 2 && state.size >= sizeof(FollowerState) &&
               (state.state == State::Following || state.state == State::Waiting) && !(state.commands & ~AllCommands) &&
               (state.commands & Capability(command));
    }

    inline bool Reached(const FollowerState& state, Command command) {
        switch (command) {
            case Command::Follow:
                return state.state == State::Following;
            case Command::Wait:
                return state.state == State::Waiting;
            case Command::Dismiss:
                return state.state == State::Inactive;
        }
        return false;
    }

    struct Adapter {
        std::uint32_t size = sizeof(Adapter);
        std::uint32_t version = InterfaceVersion;
        const char* id = nullptr;  // Unique stable identity, e.g. "org.example.serana".
        const char* name = nullptr;
        void* context = nullptr;
        // Main game thread only. Return the number written, never more than capacity.
        std::uint32_t (*enumerate)(void*, ActorID*, std::uint32_t capacity) = nullptr;
        // Return 1 when this controller owns the actor, including inactive actors.
        std::uint32_t (*inspect)(void*, ActorID, FollowerState*) = nullptr;
        // Pending requires API.complete after the operation has actually finished.
        StartResult (*start)(void*, ActorID, Command, RequestID, char* reason, std::uint32_t capacity) = nullptr;
        // Optional v1 extension. Main thread, synchronous; change only distance,
        // never recruit, resume waiting, dismiss or bypass quest restrictions.
        std::uint32_t (*setFollowDistance)(void*, ActorID, FollowDistance) = nullptr;
        // Optional v1 extension. Read-only, main thread: may the actor be
        // recruited through its owner's dialogue, ignoring only the vanilla
        // one-follower limit? Return 1 only when the owner's restrictions allow it.
        std::uint32_t (*canRecruitThroughDialogue)(void*, ActorID) = nullptr;
        // Optional v1 extension. Read-only, main thread: may the host apply its
        // combat-protection ability? Does not grant essential status or ownership.
        std::uint32_t (*canReceiveCombatProtection)(void*, ActorID) = nullptr;
    };

    inline constexpr std::uint32_t BaseAdapterSize = offsetof(Adapter, setFollowDistance);

    inline bool HasFollowDistance(const Adapter& adapter) {
        return adapter.size >= offsetof(Adapter, setFollowDistance) + sizeof(adapter.setFollowDistance) &&
               adapter.setFollowDistance;
    }

    inline bool HasRecruitmentEligibility(const Adapter& adapter) {
        return adapter.size >=
                   offsetof(Adapter, canRecruitThroughDialogue) + sizeof(adapter.canRecruitThroughDialogue) &&
               adapter.canRecruitThroughDialogue;
    }

    inline bool HasCombatProtection(const Adapter& adapter) {
        return adapter.size >=
                   offsetof(Adapter, canReceiveCombatProtection) + sizeof(adapter.canReceiveCombatProtection) &&
               adapter.canReceiveCombatProtection;
    }

    struct API {
        std::uint32_t size = sizeof(API);
        std::uint32_t version = InterfaceVersion;
        // Register at SKSE PostLoad; return 0 on an invalid/duplicate registration.
        AdapterID (*registerAdapter)(const Adapter*) = nullptr;
        // Thread-safe. Tokens are transient and never reused across save loads.
        void (*complete)(AdapterID, RequestID, std::uint32_t success, const char* reason) = nullptr;
        // Optional v1 extension. Thread-safe: notify after the owner's state may
        // have changed externally. Check the table size before reading this field.
        void (*stateChanged)(AdapterID) = nullptr;
    };

    inline constexpr std::uint32_t BaseAPISize = offsetof(API, stateChanged);

    inline bool CanNotifyState(const API* api) {
        return api && api->size >= sizeof(API) && api->version == InterfaceVersion && api->stateChanged;
    }

    using QueryAPI = const API* (*)(std::uint32_t version);
    static_assert(std::is_standard_layout_v<Adapter> && std::is_standard_layout_v<API>);
    static_assert(sizeof(void*) == 8 && sizeof(FollowerState) == 204 && BaseAdapterSize == 56 &&
                      offsetof(Adapter, canRecruitThroughDialogue) == 64 &&
                      offsetof(Adapter, canReceiveCombatProtection) == 72 && sizeof(Adapter) == 80 &&
                      BaseAPISize == 24 && sizeof(API) == 32,
                  "Follower adapter API v1 requires the standard x64 structure layout");
}
