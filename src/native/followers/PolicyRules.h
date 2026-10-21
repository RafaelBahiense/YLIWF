#pragma once

#include "FollowerPolicy.h"
#include <cstring>

namespace mod::policy_rules {
    struct Aggregate {
        yliwf::policy::State state;
        bool invalid = false, ownerConflict = false, packageConflict = false;

        Aggregate() {
            state.blocked = 0;
        }

        void Add(const yliwf::policy::State& value) {
            using namespace yliwf::policy;
            if (!Valid(value)) {
                invalid = true;
                return;
            }
            if (value.reason[0] && (!state.reason[0] || (!state.blocked && value.blocked))) {
                std::memcpy(state.reason, value.reason, sizeof(state.reason));
                state.reason[sizeof(state.reason) - 1] = 0;
            }
            state.blocked |= value.blocked;
            if (value.ownerQuest) {
                ownerConflict |= state.ownerQuest && state.ownerQuest != value.ownerQuest;
                state.ownerQuest = value.ownerQuest;
            }
            if (value.packagePreference == PackagePreference::Default)
                return;
            if (state.packagePreference == PackagePreference::Default ||
                value.packagePriority > state.packagePriority) {
                state.packagePreference = value.packagePreference;
                state.package = value.package;
                state.packagePriority = value.packagePriority;
                packageConflict = false;
            } else if (value.packagePriority == state.packagePriority &&
                       (value.packagePreference != state.packagePreference || value.package != state.package)) {
                packageConflict = true;
            }
        }

        yliwf::policy::State Result() const {
            using namespace yliwf::policy;
            if (invalid || ownerConflict) {
                State result;
                std::strcpy(result.reason, "Follower policy providers conflict on ownership or returned invalid data.");
                return result;
            }
            auto result = state;
            if (packageConflict) {
                result.packagePreference = PackagePreference::Default;
                result.package = 0;
                result.packagePriority = 0;
                std::strcpy(result.reason, "Follower package requests conflict at the highest priority.");
            }
            return result;
        }
    };
}
