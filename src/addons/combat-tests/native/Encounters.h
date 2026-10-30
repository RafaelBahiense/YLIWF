#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace mod::test_rules {
    enum class Encounter : std::uint32_t { SustainedMelee, PartyPressure };

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
                 "Needs a known ally-healing spell and enough magicka to begin."}};

    constexpr bool ValidEncounter(std::uint32_t id) {
        return id < Scenarios.size();
    }

    constexpr std::uint32_t EncounterRole(std::uint32_t id) {
        return ValidEncounter(id) ? Scenarios[id].role : 0;
    }

    constexpr bool ValidConfiguration(std::uint32_t encounter, std::uint32_t mode) {
        return ValidEncounter(encounter) && mode <= 3 && (encounter == 0 || mode == 0);
    }

    inline float InjuryTarget(float maximumHealth) {
        return std::isfinite(maximumHealth) && maximumHealth > 1 ? std::max(1.0f, maximumHealth * 0.45f) : 0;
    }
}
