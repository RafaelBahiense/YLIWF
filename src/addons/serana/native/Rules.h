#pragma once
#include "sdk/FollowerAdapter.h"
#include <cstdio>
#include <cstring>
#include <array>
#include <optional>

namespace mod::serana_rules {
    struct ControllerState {
        bool following = false, waiting = false, dismissed = false;
        bool locked = false, willingToWait = false, canDismiss = false;
        bool canFollow = true;
    };

    inline bool CanRecruitThroughDialogue(const ControllerState& value, bool dialogueBlocked) {
        return value.dismissed && !value.following && !value.waiting && value.canFollow && !value.locked &&
               !dialogueBlocked;
    }

    inline std::optional<std::array<bool, 3>> DistanceFlags(yliwf::sdk::FollowDistance distance) {
        using yliwf::sdk::FollowDistance;
        if (static_cast<unsigned>(distance) > 2)
            return {};
        return std::array{distance == FollowDistance::Far, distance == FollowDistance::Normal,
                          distance == FollowDistance::Close};
    }

    inline std::optional<std::array<bool, 3>> DistanceFlags(const ControllerState& value,
                                                            yliwf::sdk::FollowDistance distance) {
        if (!value.following || value.dismissed || value.locked || !value.canFollow)
            return {};
        return DistanceFlags(distance);
    }

    inline yliwf::sdk::FollowerState Describe(const ControllerState& value) {
        using namespace yliwf::sdk;
        FollowerState result;
        result.state = !value.following || value.dismissed ? State::Inactive
                       : value.waiting                     ? State::Waiting
                                                           : State::Following;
        if (result.state == State::Inactive)
            return result;
        if (value.locked) {
            std::snprintf(result.reason, sizeof(result.reason), "%s",
                          "Dawnguard currently requires Serana's behavior to stay unchanged. Follow, Wait and Dismiss "
                          "are unavailable.");
            return result;
        }
        if (value.canFollow)
            result.commands = Capability(Command::Follow);
        if (value.willingToWait)
            result.commands |= Capability(Command::Wait);
        if (value.canDismiss)
            result.commands |= Capability(Command::Dismiss);
        const auto restrict = [&](const char* message) {
            const auto used = std::strlen(result.reason);
            std::snprintf(result.reason + used, sizeof(result.reason) - used, "%s%s", used ? " " : "", message);
        };
        if (!value.canFollow)
            restrict("Follow is unavailable: her controller blocks resuming travel.");
        if (!value.willingToWait)
            restrict("Wait is unavailable: she is unwilling to wait.");
        if (!value.canDismiss)
            restrict("Dismiss is unavailable: her quest requires her to stay.");
        return result;
    }
}
