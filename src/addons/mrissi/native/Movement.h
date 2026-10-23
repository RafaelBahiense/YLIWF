#pragma once

#include "FollowerAdapter.h"
#include "MovementRules.h"
#include <cstdint>

namespace RE {
    class Actor;
    class TESPackage;
}

namespace mod::mrissi_movement {
    void Configure(RE::TESPackage* onFoot, RE::TESPackage* wait, RE::TESPackage* idle);
    bool Ready();
    bool Owns(const RE::TESPackage* package);
    bool Permitted(RE::Actor* actor);
    mrissi_movement_rules::Snapshot Read(RE::Actor* actor);
    bool SetDistance(yliwf::sdk::FollowDistance distance);
    bool SetFormation(RE::Actor* actor, std::uint32_t package);
    bool FormationActive(RE::Actor* actor, std::uint32_t package);
    bool SetStandDown(RE::Actor* actor, std::uint32_t package);
    bool StandDownActive(RE::Actor* actor, std::uint32_t package);
    void Refresh(RE::Actor* actor, const mrissi_movement_rules::Snapshot& state);
    void Release(RE::Actor* actor);
    void Reset(RE::Actor* actor);
}
