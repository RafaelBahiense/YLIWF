#pragma once

#include "FollowerPolicy.h"

namespace mod::package_control_rules {
    enum class Decision { Release, Suspend, Override };

    struct State {
        bool requested = false, registered = false, alive = false, loaded = false;
        bool scene = false, questAllows = false;
        bool commandPending = false;
        bool policyRequested = false, disabled = false;
    };

    constexpr Decision Decide(const State& state) {
        if (!state.requested || !state.registered || !state.alive)
            return Decision::Release;
        if (state.disabled || state.scene || !state.questAllows || state.commandPending ||
            (!state.loaded && !state.policyRequested))
            return Decision::Suspend;
        return Decision::Override;
    }

    constexpr bool RestoreInterrupt(bool available, bool occupied, bool scene, bool conditions) {
        return available && !occupied && !scene && conditions;
    }

    template <class Packages, class Eligible>
    typename Packages::value_type Select(const Packages& packages, Eligible eligible) {
        for (auto package : packages)
            if (eligible(package))
                return package;
        return {};
    }

    template <class Packages, class Resolve, class Eligible>
    typename Packages::value_type SelectPolicy(const yliwf::policy::State& policy, const Packages& packages,
                                               Resolve resolve, Eligible eligible) {
        if (yliwf::policy::HasPackageRequest(policy) &&
            policy.packagePreference == yliwf::policy::PackagePreference::Requested) {
            auto package = resolve(policy.package);
            return package && eligible(package) ? package : typename Packages::value_type{};
        }
        return Select(packages, eligible);
    }
}
