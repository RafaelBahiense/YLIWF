#pragma once
#include "FollowerPolicy.h"

namespace mod::fura_rules {
    struct RestrictionsState {
        bool unavailable = false, questLocked = false, atSawmill = false;
        bool hostile = false, inScene = false, specialPackage = false, introduced = false;
    };

    inline std::uint32_t Restrictions(const RestrictionsState& value) {
        using namespace yliwf::policy;
        if (value.unavailable || value.questLocked || value.atSawmill || value.hostile || value.inScene ||
            value.specialPackage)
            return All;
        return value.introduced ? 0u : Bit(Action::Recruit);
    }
}