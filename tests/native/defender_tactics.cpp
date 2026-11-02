#include <doctest/doctest.h>
#include "combat/DefenderTactics.h"
#include "combat/DefenderMovementRules.h"
#include "combat/DefenderProgress.h"
#include "combat/DefenderCastingRules.h"
#include <array>
#include <limits>

using namespace mod::defender_tactics;

TEST_CASE("Hostile spell startup and ready states establish preparation without assuming projectile flight") {
    using namespace mod::defender_casting_rules;
    for (std::uint32_t state = 1; state <= 5; ++state) {
        CHECK(Observe(true, state) == Activity::Preparing);
        CHECK(Observe(false, state) == Activity::Idle);
    }
    CHECK(Observe(true, 6) == Activity::Casting);
    for (const auto state : {0u, 7u, 8u, 9u, 999u})
        CHECK(Observe(true, state) == Activity::Idle);
    std::array threats{Threat{1, Kind::Projectile, 900, 0, 288, true, false, false, true, false, true, true}};
    Snapshot state{true, false, false, true, 150, 300, threats};
    CHECK(Choose(state).motion == Motion::Lateral);
    CHECK_FALSE(Choose(state).guard);
    threats[0].windup = false;
    CHECK(Choose(state).motion == Motion::Close);
}

TEST_CASE("Movement bounds reject invalid radii and destinations outside their anchor") {
    using namespace mod::defender_movement_rules;
    CHECK(WithinBounds({0, 0}, {2000, 2000}, 4096));
    CHECK_FALSE(WithinBounds({0, 0}, {4097, 0}, 4096));
    CHECK_FALSE(WithinBounds({0, 0}, {0, 0}, 0));
    CHECK_FALSE(WithinBounds({0, 0}, {0, 0}, 4097));
    CHECK_FALSE(WithinBounds({0, 0}, {std::numeric_limits<float>::quiet_NaN(), 0}, 4096));
}

TEST_CASE("Defender braces for normal melee and interrupts an affordable heavy windup") {
    std::array threats{Threat{1, Kind::Melee, 100, 0, 288, true, false, true, true, false, true, true}};
    Snapshot state{true, false, false, true, 150, 300, threats};
    CHECK(Choose(state).intent == Intent::Guard);
    threats[0].kind = Kind::HeavyMelee;
    CHECK(Choose(state).intent == Intent::Interrupt);
    state.stamina = 20;
    CHECK(Choose(state).intent == Intent::Reposition);
    CHECK(Choose(state).guard);
    state.movement = false;
    CHECK(Choose(state).motion == Motion::None);
    CHECK(Choose(state).guard);
}

TEST_CASE("Melee pursuit uses progress-tracked movement and releases it within engagement range") {
    std::array threats{Threat{1, Kind::Melee, 500, 0, 288, true, false, false, false, false, true, true}};
    Snapshot state{true, false, false, true, 150, 300, threats};
    CHECK(Choose(state).motion == Motion::Close);
    state.movement = false;
    CHECK(Choose(state).motion == Motion::None);
    state.movement = true;
    threats[0].distance = 192;
    CHECK(Choose(state).motion == Motion::None);
    CHECK(Choose(state).offense);
}

TEST_CASE("Archer approach never invents a safe projectile gap from attack state") {
    std::array threats{Threat{1, Kind::Bow, 1000, 0, 288, true, false, false, false, false, true, true}};
    Snapshot state{true, false, false, true, 150, 300, threats};
    auto decision = Choose(state);
    CHECK(decision.guard);
    CHECK(decision.motion == Motion::Close);
    CHECK_FALSE(decision.offense);
}

TEST_CASE("Unprotected caster defense repositions instead of assuming magic resistance helps a shield") {
    for (const auto kind : {Kind::Projectile, Kind::Concentration, Kind::Area}) {
        std::array threats{Threat{1, kind, 900, 0, 288, true, false, true, false, false, true, true}};
        Snapshot state{true, false, false, true, 150, 300, threats};
        CHECK(Choose(state).motion == Motion::Lateral);
        CHECK_FALSE(Choose(state).guard);
        state.magicGuard = true;
        CHECK(Choose(state).guard);
        CHECK(Choose(state).motion == Motion::Close);
    }
}

TEST_CASE("Recovery and counters yield to overlapping and rear threats") {
    std::array threats{Threat{1, Kind::Melee, 100, 0, 288, true, false, false, false, true, true, true},
                       Threat{2, Kind::Melee, 180, 120, 288, true, false, false, false, false, true, true}};
    Snapshot state{true, false, false, true, 150, 300, threats};
    CHECK(Choose(state).intent == Intent::Counter);
    state.recovering = true;
    CHECK(Choose(state).intent == Intent::Recover);
    CHECK(Choose(state).motion == Motion::Retreat);
    threats[1].attacking = true;
    CHECK(Choose(state).intent == Intent::Reposition);
    CHECK_FALSE(Choose(state).offense);
}

TEST_CASE("Defender pressures ally threats, respects pursuit bounds and yields to restrictions") {
    std::array threats{Threat{1, Kind::Melee, 500, 0, 288, false, true, true, false, false, true, true}};
    Snapshot state{true, false, false, true, 150, 300, threats};
    CHECK(Choose(state).motion == Motion::Intercept);
    CHECK(Choose(state).target == 1);
    state.eligible = false;
    CHECK(Choose(state).intent == Intent::Yield);
    state.eligible = true;
    threats[0].distance = 2500;
    CHECK(Choose(state).target == 0);
    threats[0].distance = std::numeric_limits<float>::quiet_NaN();
    CHECK(Choose(state).target == 0);
}

TEST_CASE("Retreat must increase separation without passing through another attacker") {
    using namespace mod::defender_movement_rules;
    std::array threats{Point{100, 0}, Point{-300, 0}};
    CHECK_FALSE(ClearStep({0, 0}, {-256, 0}, threats, true));
    CHECK(ClearStep({0, 0}, {0, 256}, threats, true));
    CHECK_FALSE(ClearStep({0, 0}, {256, 0}, threats, false));
    CHECK_FALSE(ClearStep({0, 0}, {10, 0}, threats, false));
    CHECK_FALSE(ClearStep({0, 0}, {600, 0}, threats, false));
}

TEST_CASE("Changing accepted destinations cannot hide an actor that never advances") {
    using namespace mod::defender_movement_rules;
    Progress tracker;
    tracker.Sample({0, 0}, .1f, false);
    for (int i = 0; i < 15; ++i) {
        tracker.Command({256.0f + i, 0}, false);
        tracker.Sample({0, 0}, .1f, true);
    }
    CHECK(tracker.Stalls() == 1);
    CHECK(tracker.Detouring());
    CHECK(tracker.Blocked({256, 0}));
    CHECK_FALSE(tracker.Blocked({0, 256}));
    for (int i = 0; i < 80; ++i)
        tracker.Sample({0, 0}, .1f, false);
    CHECK_FALSE(tracker.Blocked({256, 0}));
    CHECK_FALSE(tracker.Detouring());
}

TEST_CASE("Approach waypoints cannot finish outside engagement distance because of package arrival tolerance") {
    using namespace mod::defender_movement_rules;
    for (const float distance : {193.0f, 201.0f, 224.0f, 300.0f}) {
        const auto step = ApproachStep(distance);
        CHECK(step > 80);
        CHECK(distance - step + 64 < 192);
    }
    CHECK(ApproachStep(1500) == 384);
    CHECK(ApproachStep(192) == 0);
}

TEST_CASE("Progress observes actor displacement, pauses inactive time and bounds repeated obstruction") {
    using namespace mod::defender_movement_rules;
    Progress tracker;
    tracker.Sample({0, 0}, .1f, false);
    tracker.Command({256, 0}, false);
    for (int i = 0; i < 30; ++i)
        tracker.Sample({0, 0}, .1f, false);
    CHECK(tracker.Stalls() == 0);
    for (int i = 0; i < 40; ++i)
        tracker.Sample({0, 0}, .1f, true);
    CHECK(tracker.Stalls() == 3);
    CHECK(tracker.Yielding());
    tracker.Sample({40, 0}, .1f, true);
    CHECK(tracker.Measured() == 40);
    CHECK(tracker.Waiting() == 0);
}
