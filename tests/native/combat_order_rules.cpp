#include <doctest/doctest.h>
#include "combat/CombatOrderRules.h"

TEST_CASE("Stand-down applies only to living followers without scene or foreign-package ownership") {
    using mod::combat_order_rules::Eligible;
    CHECK(Eligible(true, false, false, false));
    CHECK_FALSE(Eligible(false, false, false, false));
    CHECK_FALSE(Eligible(true, true, false, false));
    CHECK_FALSE(Eligible(true, false, true, false));
    CHECK_FALSE(Eligible(true, false, false, true));
}

TEST_CASE("Passive following keeps a safe destination until threats leave or the player moves away") {
    using namespace mod::combat_order_rules;
    const std::array threats{Point{0, 0, 0}};
    CHECK(KeepDestination({0, 640, 0}, {0, 0, 0}, threats, FollowRadius(0)));
    CHECK_FALSE(KeepDestination({0, 100, 0}, {0, 0, 0}, threats, FollowRadius(0)));
    CHECK_FALSE(KeepDestination({0, 640, 0}, {2000, 0, 0}, threats, FollowRadius(0)));
    CHECK_FALSE(KeepDestination({0, 640, 0}, {0, 0, 0}, {}, FollowRadius(0)));
    CHECK(FollowRadius(0) < FollowRadius(1));
    CHECK(FollowRadius(1) < FollowRadius(2));
}

TEST_CASE("Avoidance rejects crowded goals and routes whose first step approaches a pursuer") {
    using namespace mod::combat_order_rules;
    const std::array threats{Point{0, 0, 0}};
    CHECK(AvoidanceScore({100, 0, 0}, {0, 0, 0}, {640, 0, 0}, threats, 192).has_value());
    CHECK_FALSE(AvoidanceScore({100, 0, 0}, {0, 0, 0}, {-640, 0, 0}, threats, 192));
    CHECK_FALSE(AvoidanceScore({100, 0, 0}, {0, 0, 0}, {200, 0, 0}, threats, 192));
    CHECK_FALSE(AvoidanceScore({100, 0, 0}, {0, 0, 0}, {2000, 0, 0}, threats, 192));
}
