#pragma once
#include "FollowDistanceRules.h"
#include <cstdint>

namespace RE {
    class Actor;
    class TESFaction;
}

namespace mod::follow_distance {
    void Configure(RE::TESFaction* faction);
    bool Available();
    Choice Read(RE::Actor* actor);
    bool Remember(RE::Actor* actor, std::int32_t preset, bool individual);
    void ApplyActor(RE::Actor* actor);  // Native roster: initialize missing choice, then reevaluate AI.
    void ApplyAll();                    // Replace all current roster choices with the selected party preset.
    void Loaded();                      // Preserve saved actor choices; initialize previously unconfigured followers.
    void Request(std::uint32_t actor, std::int32_t alias, std::int32_t preset, std::uint64_t generation);
}
