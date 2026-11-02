#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace mod::test_rules {
    enum class Encounter : std::uint32_t {
        SustainedMelee,
        PartyPressure,
        GuardRelease,
        NormalBash,
        PowerBash,
        RightAttack
    };

    struct Scenario {
        Encounter id;
        std::uint32_t role;
        const char* category;
        const char* name;
        const char* description;
        const char* requirement;
    };

    inline constexpr std::array Scenarios{
        Scenario{Encounter::SustainedMelee, 5, "Shield defender", "Sustained melee",
                 "Manage stamina, defend against sustained sword attacks and exploit counterattack openings.",
                 "Needs a one-handed weapon and shield."},
        Scenario{Encounter::PartyPressure, 1, "Healer", "Party pressure",
                 "Heal the injured player while a sword opponent pressures the healer. Five injury waves over 60 "
                 "active seconds.",
                 "Needs a known ally-healing spell and enough magicka to begin."},
        Scenario{
            Encounter::GuardRelease, 0, "Action primitives", "Guard and release",
            "Request a guard, hold for one second, then release and verify cleanup. Defender tactics are bypassed.",
            "Needs a one-handed weapon and shield."},
        Scenario{Encounter::NormalBash, 0, "Action primitives", "Normal bash",
                 "Acquire an owned guard, request one normal bash, then verify start and completion.",
                 "Needs a one-handed weapon and shield."},
        Scenario{Encounter::PowerBash, 0, "Action primitives", "Power bash",
                 "Acquire an owned guard, request one power bash, then verify start and completion.",
                 "Needs a one-handed weapon, shield and the power-bash perk."},
        Scenario{Encounter::RightAttack, 0, "Action primitives", "Right-hand normal attack",
                 "Request one normal right-hand attack and verify its resolved action, start and cleanup.",
                 "Needs a one-handed melee weapon and shield; start without blocking."}};

    constexpr bool DirectEncounter(std::uint32_t id) {
        return id >= 2 && id <= 5;
    }

    constexpr std::uint32_t EquipmentRole(std::uint32_t id) {
        return DirectEncounter(id) ? 5 : (id < Scenarios.size() ? Scenarios[id].role : 0);
    }

    constexpr bool ValidEncounter(std::uint32_t id) {
        return id < Scenarios.size();
    }

    constexpr std::uint32_t EncounterRole(std::uint32_t id) {
        return ValidEncounter(id) ? Scenarios[id].role : 0;
    }

    constexpr bool ValidConfiguration(std::uint32_t encounter, std::uint32_t mode) {
        return ValidEncounter(encounter) &&
               (DirectEncounter(encounter) ? mode == 3 : mode <= 3 && (encounter == 0 || mode == 0));
    }

    inline float InjuryTarget(float maximumHealth) {
        return std::isfinite(maximumHealth) && maximumHealth > 1 ? std::max(1.0f, maximumHealth * 0.45f) : 0;
    }
}
