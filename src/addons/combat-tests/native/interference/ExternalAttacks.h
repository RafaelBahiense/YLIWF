#pragma once
#include "CombatTestHost.h"
#include <cstdint>
#include <string_view>
#include <string>

namespace RE {
    class Actor;
}

namespace mod::combat_tests::interference::external_attacks {
    using Reserve = decltype(yliwf::testing::Host::setAttackCommitment);

    struct Snapshot {
        std::uint32_t proposals = 0, accepted = 0, rejected = 0, cleared = 0, superseded = 0, endedActive = 0,
                      active = 0;
        std::uint32_t permaProposals = 0, permaAccepted = 0, permaRejected = 0;
        std::uint32_t animationClears = 0, recoveryClears = 0, rejectionClears = 0;
        std::uint32_t handoffs = 0, finishing = 0;
    };

    void Begin(std::uint32_t actor, std::uint32_t token, Reserve callback);
    void End();
    Snapshot Read();
    std::string Report();
    bool Watching(std::uint32_t actor);
    std::uint32_t Before(std::uint32_t actor, std::string_view module, std::string_view event);
    void After(std::uint32_t sequence, bool accepted);
    void Event(std::uint32_t actor, std::string_view event);
    void Sample(RE::Actor* actor);
}
