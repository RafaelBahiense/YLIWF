#pragma once

#include <cstdint>
#include <cstddef>

// Restrictions and transient package requests for core-managed followers.
// These providers never claim follower ownership or consume external slots.
namespace yliwf::policy {
    inline constexpr std::uint32_t Version = 1;
    inline constexpr char QueryExport[] = "YLIWF_GetFollowerPolicyAPI";
    enum class Action : std::uint32_t { Recruit, Follow, Wait, Dismiss, Movement, Combat, Home };

    constexpr std::uint32_t Bit(Action action) {
        return 1u << static_cast<std::uint32_t>(action);
    }

    inline constexpr std::uint32_t All = (1u << 7) - 1;
    enum class PackagePreference : std::uint32_t { Default, Follower, Requested };

    struct State {
        std::uint32_t size = sizeof(State);
        std::uint32_t blocked = All;
        std::uint32_t ownerQuest = 0;
        char reason[192]{};
        PackagePreference packagePreference = PackagePreference::Default;
        std::uint32_t package = 0;
        std::uint32_t packagePriority = 0;
    };

    struct Provider {
        std::uint32_t size = sizeof(Provider), version = Version;
        const char* id = nullptr;
        void* context = nullptr;
        // Main-thread, read-only and nonblocking. Return 1 for related actors,
        // including inactive ones, or 0 for unrelated actors. No exceptions.
        std::uint32_t (*inspect)(void*, std::uint32_t actor, State*) = nullptr;
    };

    struct API {
        std::uint32_t size = sizeof(API), version = Version;
        // Register once during SKSE PostLoad. Providers live until process exit.
        std::uint32_t (*registerProvider)(const Provider*) = nullptr;
    };

    using QueryAPI = const API* (*)(std::uint32_t);
    static_assert(sizeof(State) == 216 && sizeof(Provider) == 32 && sizeof(API) == 16);

    inline bool Valid(const State& state) {
        return state.size >= sizeof(State) && !(state.blocked & ~All) &&
               (state.packagePreference == PackagePreference::Default ||
                state.packagePreference == PackagePreference::Follower ||
                state.packagePreference == PackagePreference::Requested) &&
               (state.packagePreference == PackagePreference::Requested ? state.package != 0 : state.package == 0);
    }

    inline bool CanRelease(const State& state, std::uint32_t owner = 0) {
        return Valid(state) && state.ownerQuest && (!owner || state.ownerQuest == owner);
    }

    inline bool HasPackageRequest(const State& state) {
        return Valid(state) && state.packagePreference != PackagePreference::Default &&
               !(state.blocked & Bit(Action::Movement));
    }

}
