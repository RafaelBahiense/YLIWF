#include <ostream>
#include <doctest/doctest.h>
#include "fixtures/NaturalOpponentRules.h"
#include "runtime/Rules.h"

TEST_CASE("Leveled opponent resolution requires one concrete NPC") {
    using mod::combat_tests::natural::SingleActor;
    CHECK(SingleActor(1, 1, true));
    CHECK_FALSE(SingleActor(0, 0, false));
    CHECK_FALSE(SingleActor(2, 1, true));
    CHECK_FALSE(SingleActor(1, 2, true));
    CHECK_FALSE(SingleActor(1, 1, false));
}

TEST_CASE("Natural encounter results distinguish performance from invalid execution") {
    using namespace mod::combat_tests::natural;
    CHECK(Resolve(false, true, true, true) == Outcome::NotMeasured);
    CHECK(Resolve(true, true, false, false) == Outcome::Defeat);
    CHECK(Resolve(true, true, true, false) == Outcome::Defeat);
    CHECK(Resolve(true, false, true, true) == Outcome::Victory);
    CHECK(Resolve(true, false, false, true) == Outcome::TimeLimit);
    CHECK(Resolve(true, false, false, false) == Outcome::NotMeasured);
    CHECK(Name(Outcome::Defeat) != Name(Outcome::Invalid));
}

TEST_CASE("Natural opponent ownership requires the live placement receipt") {
    using mod::combat_tests::natural::Owned;
    CHECK(Owned(true, true, true, true));
    CHECK_FALSE(Owned(false, true, true, true));
    CHECK_FALSE(Owned(true, false, true, true));
    CHECK_FALSE(Owned(true, true, false, true));
    CHECK_FALSE(Owned(true, true, true, false));
}

TEST_CASE("Natural mage restoration permits guard ownership without changing the saved layout") {
    using namespace mod::test_rules;
    Plan plan{.subject = 1, .origin = 2, .health = 100, .generation = 3, .mode = 3, .guardPhase = 1, .encounter = 6};
    CHECK(Valid(plan));
    plan.mode = 0;
    CHECK_FALSE(Valid(plan));
    plan.guardPhase = 0;
    CHECK(Valid(plan));
}
