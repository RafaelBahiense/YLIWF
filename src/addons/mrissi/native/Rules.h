#pragma once

#include "FollowerAdapter.h"
#include <cmath>
#include <cstdio>

namespace mod::mrissi_rules {
    struct Snapshot {
        bool initialized = false;
        bool dead = false;
        bool recruited = false;
        bool teammate = false;
        bool dismissed = false;
        float waiting = 0;
        bool questLocked = false;
        bool broken = false;
        bool restrictedLocation = false;
        bool scene = false;
        bool ordinaryPackage = false;
        bool incapacitated = false;
        bool mounting = false;
        bool pending = false;
    };

    inline yliwf::sdk::FollowerState Describe(const Snapshot& value) {
        using namespace yliwf::sdk;
        FollowerState result;
        const auto explain = [&](const char* reason) {
            std::snprintf(result.reason, sizeof(result.reason), "%s", reason);
            return result;
        };
        if (value.dead) {
            result.state = State::Inactive;
            return explain("M'rissi is dead.");
        }
        if (!value.initialized)
            return explain("M'rissi's follower controller is unavailable or incompatible.");
        if (!value.recruited) {
            if (value.teammate && !value.dismissed)
                return explain("M'rissi has teammate status without her follower alias; no repair is applied.");
            result.state = State::Inactive;
            return explain("Recruit M'rissi through her own dialogue.");
        }
        if (!value.teammate || value.dismissed || !std::isfinite(value.waiting) ||
            (value.waiting != 0 && value.waiting != 1))
            return explain("M'rissi's follower alias, teammate and waiting state disagree; no repair is applied.");
        result.state = value.waiting == 0 ? State::Following : State::Waiting;
        if (value.questLocked)
            return explain("M'rissi's quest currently restricts follower commands.");
        if (value.broken)
            return explain("M'rissi is using her special broken-state controller.");
        if (value.scene)
            return explain("M'rissi is participating in a scene.");
        if (value.incapacitated)
            return explain("M'rissi is disabled, unconscious or bleeding out.");
        if (value.mounting)
            return explain("M'rissi's mount controller is handling movement.");
        if (value.pending)
            return explain("Waiting for M'rissi's follower controller to finish.");
        if (!value.ordinaryPackage)
            return explain("M'rissi is using a quest, restricted or unrecognized package.");

        result.commands = Capability(Command::Follow);
        if (!value.restrictedLocation) {
            result.commands |= Capability(Command::Wait);
            // Her dismissal dialogue is authored for ordinary following only.
            if (result.state == State::Following)
                result.commands |= Capability(Command::Dismiss);
        } else {
            return explain("M'rissi cannot wait or be dismissed in this location.");
        }
        return result;
    }
}
