#include <doctest/doctest.h>
#include "combat/actions/ExternalAttackReservation.h"

TEST_CASE("External attack ownership checks actor token and monotonically increasing request IDs") {
    mod::combat_actions::ExternalAttackReservation lease;
    CHECK_FALSE(lease.Set(1, 2, 1, true));
    lease.Begin(1, 2);
    CHECK_FALSE(lease.Set(9, 2, 1, true));
    CHECK_FALSE(lease.Set(1, 9, 1, true));
    CHECK_FALSE(lease.Set(1, 2, 0, true));
    CHECK(lease.Set(1, 2, 1, true));
    CHECK(lease.Active());
    CHECK(lease.Set(1, 2, 2, true));
    CHECK_FALSE(lease.Set(1, 2, 1, false));
    CHECK_FALSE(lease.Set(1, 2, 1, true));
    CHECK(lease.Active());
    CHECK(lease.Set(1, 2, 2, false));
    CHECK_FALSE(lease.Active());
    CHECK_FALSE(lease.Set(1, 2, 2, true));
}

TEST_CASE("Closing and resetting an external attack lease rejects late cross-run updates") {
    mod::combat_actions::ExternalAttackReservation lease;
    lease.Begin(1, 2);
    CHECK(lease.Set(1, 2, 1, true));
    lease.Close();
    CHECK_FALSE(lease.Set(1, 2, 1, false));
    CHECK_FALSE(lease.Set(1, 2, 2, true));
    CHECK(lease.Active());
    lease.Clear();
    CHECK_FALSE(lease.Active());
    lease.Begin(1, 3);
    CHECK(lease.Set(1, 3, 2, true));
    CHECK_FALSE(lease.Set(1, 2, 1, false));
    CHECK(lease.Active());
}

TEST_CASE("Final attack evidence begins a protected handoff without releasing the reservation") {
    mod::combat_actions::ExternalAttackReservation lease;
    lease.Begin(1, 2);
    CHECK(lease.Set(1, 2, 1, true));
    CHECK_FALSE(lease.BlocksNewAttacks());
    CHECK_FALSE(lease.Finish(9, 2, 1));
    CHECK_FALSE(lease.Finish(1, 9, 1));
    CHECK_FALSE(lease.Finish(1, 2, 0));
    CHECK_FALSE(lease.Finish(1, 2, 2));
    CHECK(lease.Finish(1, 2, 1));
    CHECK(lease.Active());
    CHECK(lease.BlocksNewAttacks());
    CHECK(lease.Finish(1, 2, 1));
    CHECK(lease.Set(1, 2, 1, false));
    CHECK_FALSE(lease.Active());
    CHECK_FALSE(lease.BlocksNewAttacks());
}

TEST_CASE("A new external owner and scope closure end the previous handoff restriction") {
    mod::combat_actions::ExternalAttackReservation lease;
    lease.Begin(1, 2);
    REQUIRE(lease.Set(1, 2, 1, true));
    REQUIRE(lease.Finish(1, 2, 1));
    CHECK(lease.Set(1, 2, 2, true));
    CHECK_FALSE(lease.BlocksNewAttacks());
    CHECK_FALSE(lease.Finish(1, 2, 1));
    CHECK(lease.Finish(1, 2, 2));
    CHECK(lease.BlocksNewAttacks());
    lease.Close();
    CHECK(lease.Active());
    CHECK_FALSE(lease.BlocksNewAttacks());
    CHECK_FALSE(lease.Finish(1, 2, 2));
    lease.Clear();
    CHECK_FALSE(lease.Active());
    CHECK_FALSE(lease.BlocksNewAttacks());
}
