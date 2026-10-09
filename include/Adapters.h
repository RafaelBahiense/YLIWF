#pragma once

#include "sdk/FollowerAdapter.h"
#include <functional>
#include <string>
#include <vector>

namespace mod::adapters {
    using Completion = std::function<void(bool, std::string)>;

    struct Follower {
        yliwf::sdk::ActorID actor = 0;
        yliwf::sdk::AdapterID adapter = 0;
        std::string name;
        yliwf::sdk::FollowerState state;
        bool supportsDistance = false;
    };

    std::vector<Follower> Followers(bool annotatePending = true);  // Main thread; quest-backed, never saved as aliases.
    bool Owns(yliwf::sdk::ActorID actor);
    bool HasFollowers();
    void ApplyFollowDistance(bool replaceAll = false);
    bool SetFollowDistance(yliwf::sdk::ActorID actor, std::int32_t preset);
    bool Request(yliwf::sdk::ActorID actor, yliwf::sdk::Command command, Completion completion);

    struct PartySubmission {
        std::size_t targets = 0;
        std::string failure;
    };

    PartySubmission RequestParty(yliwf::sdk::Command command, Completion completion);
    void Tick(float elapsed);
    bool HasWork();
    void Invalidate();
    void GameLoaded();
}
