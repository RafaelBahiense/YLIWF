#pragma once

#include "FollowerAdapter.h"
#include <cmath>
#include <cstdio>
#include <optional>

namespace mod::inigo_rules {
    struct Snapshot {
        bool initialized = false;
        bool dead = false;
        bool teammate = false;
        float waiting = -1.0f;
        bool questLocked = false;
        bool scene = false;
        bool ordinaryPackage = false;
        bool incapacitated = false;
    };

    inline yliwf::sdk::FollowerState Describe(const Snapshot& value) {
        using namespace yliwf::sdk;
        FollowerState result;
        const auto explain = [&](const char* reason) {
            std::snprintf(result.reason, sizeof(result.reason), "%s", reason);
            return result;
        };
        if (value.dead || !value.initialized) {
            result.state = State::Inactive;
            return explain(value.dead ? "Inigo is dead." : "Recruit Inigo through his own dialogue first.");
        }
        // Inigo deliberately keeps teammate status after Dismiss().
        if (value.waiting == -1.0f) {
            result.state = State::Inactive;
            return result;
        }
        if (!value.teammate || !std::isfinite(value.waiting) ||
            (value.waiting != 0.0f && value.waiting != 1.0f && value.waiting != 2.0f))
            return explain("Inigo's teammate and waiting state are incompatible; no automatic repair is applied.");

        result.state = value.waiting == 0.0f ? State::Following : State::Waiting;
        if (value.questLocked)
            return explain("Inigo's quest currently requires his behavior to stay unchanged.");
        if (value.scene)
            return explain("Inigo is participating in a scene.");
        if (value.incapacitated)
            return explain("Inigo is disabled, unconscious or bleeding out.");
        if (!value.ordinaryPackage)
            return explain("Inigo is using a quest, restricted or unrecognized package.");
        result.commands = AllCommands;
        return result;
    }

    inline std::optional<std::int8_t> DistanceRank(yliwf::sdk::FollowDistance distance) {
        // Preserve Inigo's native preference for movement we do not replace,
        // including horse behavior. On-foot spacing uses the host's three packages.
        using yliwf::sdk::FollowDistance;
        switch (distance) {
            case FollowDistance::Close:
            case FollowDistance::Normal:
                return 0;
            case FollowDistance::Far:
                return 1;
        }
        return {};
    }
}
