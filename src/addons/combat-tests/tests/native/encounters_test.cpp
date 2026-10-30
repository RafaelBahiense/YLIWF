#include <doctest/doctest.h>
#include "Rules.h"
#include <limits>
#include <array>

TEST_CASE("Role encounters accept only their applicable control settings") {
    using namespace mod::test_rules;
    CHECK(EncounterRole(0) == ShieldRole);
    CHECK(EncounterRole(1) == 1);
    CHECK(ValidConfiguration(0, 3));
    CHECK(ValidConfiguration(1, 0));
    CHECK_FALSE(ValidConfiguration(1, 1));
    CHECK_FALSE(ValidConfiguration(2, 0));
    CHECK_FALSE(ValidConfiguration(0, 4));
    CHECK(RestoreRole(true, 1, 1));
    CHECK_FALSE(RestoreRole(true, 5, 1));
}

TEST_CASE("Healer injury waves have a positive floor and reject invalid health") {
    using mod::test_rules::InjuryTarget;
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
    REQUIRE(DecodePlan(3, bytes));
    CHECK(DecodePlan(3, bytes)->playerHealth == 50);
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
