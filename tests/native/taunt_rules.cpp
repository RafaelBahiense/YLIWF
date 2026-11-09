#include <doctest/doctest.h>
#include "combat/actions/TauntRules.h"
#include <limits>

using namespace mod::combat_actions::taunt_rules;

TEST_CASE("A challenge requires a visible hostile ally threat and affordable healthy defender") {
    Opportunity value{true, true, true, true, false, 400, 200, 390, 1};
    CHECK(Eligible(value));
    value.recovering = true;
    CHECK_FALSE(Eligible(value));
    value.recovering = false;
    value.threatensAlly = false;
    CHECK_FALSE(Eligible(value));
    value.threatensAlly = true;
    value.hostile = false;
    CHECK_FALSE(Eligible(value));
    value.hostile = true;
    value.stamina = 97;
    CHECK_FALSE(Eligible(value));
    value.stamina = 200;
    value.healthFraction = .39f;
    CHECK_FALSE(Eligible(value));
    value.healthFraction = 1;
    value.distance = 513;
    CHECK_FALSE(Eligible(value));
    value.distance = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(Eligible(value));
}

TEST_CASE("A challenge is acknowledged by applied effect and real target switch then releases naturally") {
    State state;
    REQUIRE(state.Request(10));
    CHECK_FALSE(state.Request(11));
    state.Observe(.1f, true, false, true);
    CHECK(state.observed == 0);
    state.Observe(.1f, true, true, false);
    CHECK(state.observed == 0);
    state.Observe(.1f, true, true, true);
    CHECK(state.observed == 1);
    CHECK(state.phase == Phase::Holding);
    for (int i = 0; i < 16; ++i)
        state.Observe(.25f, true, true, true);
    CHECK(state.phase == Phase::Idle);
    CHECK(state.released == 1);
    CHECK(state.enemy == 0);
    CHECK_FALSE(state.Request(10));
    for (int i = 0; i < 48; ++i)
        state.Observe(.25f, false, false, false);
    CHECK(state.Request(11));
}

TEST_CASE("Unhonored and invalid challenges release influence without retrying or manufacturing success") {
    State state;
    REQUIRE(state.Request(10));
    for (int i = 0; i < 8; ++i)
        state.Observe(.25f, true, true, false);
    CHECK(state.unobserved == 1);
    CHECK(state.observed == 0);
    CHECK(state.enemy == 0);
    CHECK(state.requests == 1);
    State cancelled;
    REQUIRE(cancelled.Request(11));
    cancelled.Observe(.1f, false, true, true);
    CHECK(cancelled.cancelled == 1);
    CHECK(cancelled.observed == 0);
    CHECK(cancelled.phase == Phase::Idle);
}

TEST_CASE("Target preference is scoped to one enemy and fails open when stale") {
    CHECK(RejectTarget(10, 20, 10, 30, true));
    CHECK_FALSE(RejectTarget(10, 30, 10, 30, true));
    CHECK_FALSE(RejectTarget(11, 20, 10, 30, true));
    CHECK_FALSE(RejectTarget(10, 20, 10, 30, false));
    CHECK_FALSE(RejectTarget(10, 0, 10, 30, true));
    CHECK_FALSE(RejectTarget(10, 20, 0, 30, true));
}
