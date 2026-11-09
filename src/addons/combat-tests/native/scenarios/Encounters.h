#pragma once
#include "Scenario.h"
#include "DefenderSuite.h"
#include "HealerSuite.h"
#include "PrimitiveSuite.h"
#include "../testing/CaseCatalog.h"
#include <array>
#include <cstdint>

namespace mod::test_rules {
    enum class Encounter : std::uint32_t {
        SustainedMelee,
        PartyPressure,
        GuardRelease,
        NormalBash,
        PowerBash,
        RightAttack,
        LeveledMage
    };

    using Scenario = mod::combat_tests::scenarios::Entry;
    using Execution = mod::combat_tests::scenarios::Execution;
    inline constexpr std::array Scenarios{
        mod::combat_tests::suite::Navigation,           mod::combat_tests::healer::Navigation,
        mod::combat_tests::primitive::Navigation[0],    mod::combat_tests::primitive::Navigation[1],
        mod::combat_tests::primitive::Navigation[2],    mod::combat_tests::primitive::Navigation[3],
        mod::combat_tests::suite::LeveledMageNavigation};

    inline constexpr std::array ScenarioOrder{0u, 1u, 2u, 3u, 4u, 5u};
    inline const std::array Catalogs{mod::combat_tests::testing::CaseCatalog{mod::combat_tests::suite::Definition},
                                     mod::combat_tests::testing::CaseCatalog{mod::combat_tests::healer::Definition},
                                     mod::combat_tests::testing::CaseCatalog{mod::combat_tests::primitive::Definition}};

    inline const mod::combat_tests::testing::CaseCatalog& EncounterCatalog(std::uint32_t id) {
        const auto* definition = id < Scenarios.size() ? Scenarios[id].suite : Scenarios[0].suite;
        for (const auto& catalog : Catalogs)
            if (&catalog.Definition() == definition)
                return catalog;
        return Catalogs[0];
    }

    constexpr bool ValidEncounter(std::uint32_t id) {
        return id < Scenarios.size();
    }

    constexpr bool IsExecution(std::uint32_t id, Execution execution) {
        return ValidEncounter(id) && Scenarios[id].execution == execution;
    }

    constexpr bool NaturalEncounter(std::uint32_t id) {
        return IsExecution(id, Execution::Natural);
    }

    constexpr bool DirectEncounter(std::uint32_t id) {
        return IsExecution(id, Execution::Primitive);
    }

    constexpr std::uint32_t EquipmentRole(std::uint32_t id) {
        return ValidEncounter(id) ? Scenarios[id].equipmentRole : 0;
    }

    constexpr std::uint32_t EncounterRole(std::uint32_t id) {
        return ValidEncounter(id) ? Scenarios[id].role : 0;
    }

    constexpr std::uint32_t Control(std::uint32_t id) {
        return ValidEncounter(id) ? Scenarios[id].control : 0;
    }

    constexpr bool ValidSavedConfiguration(std::uint32_t encounter, std::uint32_t mode) {
        return ValidEncounter(encounter) &&
               (DirectEncounter(encounter) ? mode == 3
                                           : mode <= 3 && (IsExecution(encounter, Execution::Defender) ||
                                                           NaturalEncounter(encounter) || mode == 0));
    }
}
