#include <doctest/doctest.h>
#include "runtime/Rules.h"
#include <limits>
#include <array>
#include <string_view>
#include <ostream>

TEST_CASE("Scenario navigation groups roles and exposes leveled mages through defender cases") {
    using namespace mod::test_rules;
    std::array<bool, Scenarios.size()> listed{};
    std::array<std::string_view, Scenarios.size()> groups{};
    std::size_t count = 0;
    std::string_view previous;
    for (const auto id : ScenarioOrder) {
        REQUIRE(id < Scenarios.size());
        CHECK_FALSE(listed[id]);
        listed[id] = true;
        const std::string_view category = Scenarios[id].category;
        if (category != previous) {
            for (std::size_t i = 0; i < count; ++i)
                CHECK(groups[i] != category);
            groups[count++] = category;
            previous = category;
        }
    }
    for (std::size_t id = 0; id < listed.size(); ++id)
        CHECK(listed[id] == (id != static_cast<std::size_t>(Encounter::LeveledMage)));
}

TEST_CASE("New defender scenarios select the full control path") {
    using namespace mod::test_rules;
    for (auto encounter : {0u, 2u, 3u, 4u, 5u, 6u})
        CHECK(Control(encounter) == 3);
    CHECK(Control(1) == 0);
}

TEST_CASE("Saved encounter controls remain readable for restoration") {
    using namespace mod::test_rules;
    CHECK(EncounterRole(0) == ShieldRole);
    CHECK(EncounterRole(1) == 1);
    CHECK(ValidSavedConfiguration(0, 3));
    CHECK(ValidSavedConfiguration(1, 0));
    CHECK_FALSE(ValidSavedConfiguration(1, 1));
    CHECK_FALSE(ValidSavedConfiguration(2, 0));
    for (auto id : {2u, 3u, 4u, 5u}) {
        CHECK(ValidSavedConfiguration(id, 3));
        CHECK_FALSE(ValidSavedConfiguration(id, 2));
        CHECK(EncounterRole(id) == 0);
        CHECK(EquipmentRole(id) == ShieldRole);
        CHECK(RestoreRole(true, 0, id));
        CHECK_FALSE(RestoreRole(true, ShieldRole, id));
    }
    for (auto mode : {0u, 1u, 2u, 3u})
        CHECK(ValidSavedConfiguration(6, mode));
    CHECK(EquipmentRole(6) == ShieldRole);
    CHECK(EncounterRole(6) == ShieldRole);
    CHECK_FALSE(ValidSavedConfiguration(7, 3));
    CHECK_FALSE(ValidSavedConfiguration(0, 4));
    CHECK(RestoreRole(true, 1, 1));
    CHECK_FALSE(RestoreRole(true, 5, 1));
}

TEST_CASE("Healer injury waves have a positive floor and reject invalid health") {
    using mod::combat_tests::healer::InjuryTarget;
    CHECK(InjuryTarget(100) == doctest::Approx(45));
    CHECK(InjuryTarget(2) == 1);
    CHECK(InjuryTarget(0) == 0);
    CHECK(InjuryTarget(-1) == 0);
    CHECK(InjuryTarget(std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK(InjuryTarget(std::numeric_limits<float>::infinity()) == 0);
}

TEST_CASE("Healer restore plans retain player health and exclude shield guard ownership") {
    using namespace mod::test_rules;
    Plan plan{.subject = 1, .health = 100, .generation = 2, .encounter = 1, .playerHealth = 50, .playerChanged = 1};
    CHECK(Valid(plan));
    std::array<std::uint8_t, sizeof(Plan)> bytes;
    std::memcpy(bytes.data(), &plan, sizeof(plan));
    REQUIRE(DecodePlan(5, bytes));
    CHECK(DecodePlan(5, bytes)->playerHealth == 50);
    REQUIRE(DecodePlan(3, std::span(bytes).first(64)));
    CHECK(DecodePlan(3, std::span(bytes).first(64))->playerHealth == 50);
    CHECK_FALSE(DecodePlan(2, bytes));
    plan.playerHealth = 0;
    CHECK_FALSE(Valid(plan));
    plan.playerHealth = 50;
    plan.guardPhase = 1;
    CHECK_FALSE(Valid(plan));
    plan.guardPhase = 0;
    plan.encounter = 0;
    CHECK_FALSE(Valid(plan));
}
