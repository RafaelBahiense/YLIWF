#include <doctest/doctest.h>
#include "combat/CombatOrderRules.h"
#include "combat/CombatPreventionRules.h"
#include "combat/CombatUpdateGate.h"

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

TEST_CASE("Stand-down claims only combat prevention that it introduced") {
    using mod::combat_prevention_rules::Decide;
    const auto acquired = Decide(true, false, false);
    CHECK(acquired.claim);
    CHECK(acquired.prevent == true);
    CHECK_FALSE(acquired.release);

    const auto foreign = Decide(true, false, true);
    CHECK_FALSE(foreign.claim);
    CHECK_FALSE(foreign.prevent.has_value());
    CHECK_FALSE(foreign.release);
    CHECK_FALSE(Decide(false, false, true).prevent.has_value());
}

TEST_CASE("Saved combat-prevention ownership is cleared when normal behavior resumes") {
    using mod::combat_prevention_rules::Decide;
    const auto restored = Decide(false, true, true);
    CHECK(restored.prevent == false);
    CHECK(restored.release);
    CHECK_FALSE(restored.claim);

    const auto recreatedProcess = Decide(true, true, false);
    CHECK(recreatedProcess.prevent == true);
    CHECK_FALSE(recreatedProcess.claim);
    CHECK_FALSE(recreatedProcess.release);

    CHECK(Decide(false, true, false).release);
    CHECK_FALSE(Decide(false, false, false).prevent.has_value());
}

TEST_CASE("Combat updates pass through except while a follower has a stand-down lease") {
    mod::combat_update::Gate gate;
    constexpr std::uint32_t follower = 1, other = 2;

    CHECK_FALSE(gate.Suppress(follower, false, false));
    gate.Acquire(follower);
    CHECK(gate.Suppress(follower, false, false));
    CHECK_FALSE(gate.Suppress(other, false, false));

    SUBCASE("Scenes and foreign interrupt packages retain combat ownership") {
        CHECK_FALSE(gate.Suppress(follower, true, false));
        CHECK_FALSE(gate.Suppress(follower, false, true));
        CHECK(gate.Suppress(follower, false, false));
    }

    SUBCASE("Dismissal or normal behavior releases the lease") {
        gate.Release(follower);
        CHECK_FALSE(gate.Suppress(follower, false, false));
    }

    SUBCASE("Load invalidation discards every lease") {
        gate.Acquire(other);
        gate.Clear();
        CHECK_FALSE(gate.Suppress(follower, false, false));
        CHECK_FALSE(gate.Suppress(other, false, false));
    }
}

TEST_CASE("Combat-update diagnostics survive lease refresh and drain without double counting") {
    mod::combat_update::Gate gate;
    gate.Acquire(1);
    CHECK(gate.Suppress(1, false, false));
    gate.Acquire(1);
    CHECK(gate.Suppress(1, false, false));
    CHECK_FALSE(gate.Suppress(1, true, false));
    CHECK(gate.TakeSuppressed(1) == 2);
    CHECK(gate.TakeSuppressed(1) == 0);

    gate.Release(1);
    gate.Acquire(1);
    CHECK(gate.TakeSuppressed(1) == 0);
    CHECK(gate.TakeSuppressed(2) == 0);
}
