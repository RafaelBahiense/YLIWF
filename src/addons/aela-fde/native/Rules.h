#pragma once
#include "FollowerPolicy.h"

namespace mod::aela_rules {
    struct RestrictionsState {
        bool compatible = true;
        bool unavailable = false;
        bool hostile = false;
        bool eligible = true;
        bool transforming = false;
        bool beastForm = false;
        bool inScene = false;
        bool questPackage = false;
    };

    constexpr std::uint32_t Restrictions(const RestrictionsState& state) {
        using namespace yliwf::policy;
        if (!state.compatible || state.unavailable || state.hostile || state.inScene || state.questPackage)
            return All;
        if (state.transforming || state.beastForm)
            return All & ~Bit(Action::Dismiss);
        // Losing eligibility does not authorize silently repairing FDE's story state.
        return state.eligible ? 0u : Bit(Action::Recruit);
    }
}
