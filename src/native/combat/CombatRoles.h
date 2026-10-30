#pragma once

#include "combat/CombatRoleRules.h"
#include <string>
#include <vector>

namespace RE {
    class Actor;
    class TESFaction;
    class SpellItem;
}

namespace mod::combat_roles {
    struct Option {
        bool eligible = false, ready = false;
        std::string reason;
    };

    struct Snapshot {
        Role assigned = Role::Automatic;
        bool available = false;
        std::string unavailable;
        std::string behavior;
        std::array<Option, RoleCount> options;
    };

    struct HealingSpell {
        RE::SpellItem* spell = nullptr;
        float cost = 0;
        bool self = false;
    };

    void Configure(RE::TESFaction* faction);
    Role Read(RE::Actor* actor);
    std::vector<HealingSpell> HealingSpells(RE::Actor* actor);
    Snapshot Inspect(RE::Actor* actor);
    // Main-thread assignment shared by queued UI commands and development tools.
    bool Apply(std::uint32_t actorID, std::uint32_t aliasID, Role role, std::uint64_t generation);
    void Request(std::uint32_t actorID, std::uint32_t aliasID, Role role, std::uint64_t generation);
}
