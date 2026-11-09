#include <doctest/doctest.h>
#include "combat/DefenderTactics.h"
#include "combat/DefenderMovementRules.h"
#include "combat/DefenderProgress.h"
#include "combat/DefenderCastingRules.h"
#include "combat/DefenderTuning.h"
#include "combat/ShieldDefenderRules.h"
#include <array>
#include <limits>

using namespace mod::defender_tactics;

TEST_CASE("A teammate defender remains a self target rather than an ally to intercept") {
    CHECK(ClassifyTarget(10, 10, false, true) == PartyTarget::Self);
    CHECK(ClassifyTarget(10, 11, false, true) == PartyTarget::Ally);
    CHECK(ClassifyTarget(10, 20, true, false) == PartyTarget::Ally);
    CHECK(ClassifyTarget(10, 30, false, false) == PartyTarget::Other);
    CHECK(ClassifyTarget(10, 0, false, false) == PartyTarget::Other);
}

TEST_CASE("Approach handoffs leave a distance gap before reacquiring movement") {
    std::array threats{Threat{.actor = 20, .kind = Kind::Melee, .distance = 300, .self = true}};
    Snapshot state{true, false, true, 100, 100, threats};
    CHECK(Choose(state).motion == Motion::Close);
    state.closing = true;
    threats[0].distance = 220;
    CHECK(Choose(state).motion == Motion::Close);
    threats[0].distance = 192;
    CHECK(Choose(state).motion == Motion::None);
    state.closing = false;
    for (const float distance : {193.0f, 220.0f, 256.0f}) {
        threats[0].distance = distance;
        CHECK(Choose(state).motion == Motion::None);
    }
    threats[0].distance = 257;
    CHECK(Choose(state).motion == Motion::Close);
    threats[0].ally = true;
    threats[0].self = false;
    CHECK(Choose(state).motion == Motion::Intercept);
    threats[0].distance = 220;
    CHECK(Choose(state).motion == Motion::None);
    state.closing = true;
    CHECK(Choose(state).motion == Motion::Intercept);
}

TEST_CASE("A test tuning scope is actor-specific, immutable and token-protected") {
    mod::defender_tuning::Scope scope;
    yliwf::testing::DefenderTuning profile;
    profile.revision = 4;
    profile.lateralStep = 400;
    profile.challengeEnabled = 0;
    scope.Begin(10, 20);
    CHECK_FALSE(scope.Configure(10, 21, profile));
    CHECK_FALSE(scope.Configure(11, 20, profile));
    profile.lateralStep = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(scope.Configure(10, 20, profile));
    CHECK(scope.Read(10).lateralStep == 256);
    profile.lateralStep = 400;
    CHECK(scope.Configure(10, 20, profile));
    CHECK(scope.Read(10).revision == 4);
    CHECK(scope.Read(10).challengeEnabled == 0);
    CHECK(scope.Read(11).challengeEnabled == 1);
    CHECK(scope.Read(10).lateralStep == 400);
    CHECK(scope.Read(11).lateralStep == 256);
    profile.lateralStep = 128;
    CHECK_FALSE(scope.Configure(10, 20, profile));
    CHECK(scope.Read(10).lateralStep == 400);
    scope.End(10, 21);
    CHECK(scope.Read(10).lateralStep == 400);
    scope.End(10, 20);
    CHECK(scope.Read(10).lateralStep == 256);
    CHECK(scope.Read(10).challengeEnabled == 1);
    scope.Begin(11, 22);
    CHECK(scope.Configure(11, 22, profile));
}

TEST_CASE("Tuning changes recovery hysteresis and approach length without changing arrival safety") {
    using mod::shield_defender_rules::Recovering;
    CHECK_FALSE(Recovering(false, 35, 100, .25f, .5f));
    CHECK(Recovering(false, 35, 100));
    CHECK_FALSE(Recovering(true, 55, 100, .25f, .5f));
    CHECK(Recovering(true, 55, 100));
    using mod::defender_movement_rules::ApproachStep;
    CHECK(ApproachStep(1000, 128) == 128);
    CHECK(ApproachStep(1000, 512) == 512);
    CHECK(193 - ApproachStep(193, 128) + 64 < 192);
}

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
    Snapshot state{true, false, true, 150, 300, threats};
    CHECK(Choose(state).motion == Motion::AdvanceLateral);
    CHECK_FALSE(Choose(state).guard);
    threats[0].windup = false;
    CHECK(Choose(state).motion == Motion::Close);
}

TEST_CASE("Active casting from either hand outranks idle or preparing magic") {
    using namespace mod::defender_casting_rules;
    CHECK(Prefer(Observe(true, 6), Observe(false, 0)));
    CHECK(Prefer(Observe(true, 3), Observe(false, 0)));
    CHECK(Prefer(Observe(true, 6), Observe(true, 3)));
    CHECK_FALSE(Prefer(Observe(false, 0), Observe(true, 6)));
    CHECK_FALSE(Prefer(Observe(true, 6), Observe(true, 6)));
}

TEST_CASE("Target commitment tolerates small distance changes but yields to danger and ally priority") {
    std::array threats{Threat{1, Kind::Melee, 500, 0, 288, true, false, false, false, false, true, true},
                       Threat{2, Kind::Melee, 450, 0, 288, true, false, false, false, false, true, true}};
    Snapshot state{true, false, true, 150, 300, threats};
    state.previousTarget = 1;
    CHECK(Choose(state).target == 1);
    threats[1].distance = 250;
    CHECK(Choose(state).target == 2);
    threats[1].distance = 450;
    threats[1].self = false;
    threats[1].ally = true;
    CHECK(Choose(state).target == 2);
    threats[1].ally = false;
    threats[1].self = true;
    threats[1].distance = 200;
    threats[1].attacking = true;
    CHECK(Choose(state).target == 2);
    threats[1].distance = 450;
    threats[1].attacking = false;
    threats[0].visible = false;
    CHECK(Choose(state).target == 2);
    threats[0].actor = 0;
    CHECK(Choose(state).target == 2);
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
    Snapshot state{true, false, true, 150, 300, threats};
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
    Snapshot state{true, false, true, 150, 300, threats};
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
    Snapshot state{true, false, true, 150, 300, threats};
    auto decision = Choose(state);
    CHECK(decision.guard);
    CHECK(decision.motion == Motion::Close);
    CHECK_FALSE(decision.offense);
    threats[0].flightKnown = true;
    CHECK(Choose(state).guard);
    state.projectileGap = true;
    CHECK_FALSE(Choose(state).guard);
    threats[0].incomingProjectile = true;
    CHECK(Choose(state).guard);
    threats[0].incomingProjectile = false;
    threats[0].windup = true;
    CHECK(Choose(state).guard);
    threats[0].windup = false;
    threats[0].flightKnown = false;
    CHECK(Choose(state).guard);
}

TEST_CASE("Retreat scoring prefers bringing a flanking attacker into the facing arc") {
    using namespace mod::defender_movement_rules;
    const std::array threats{Point{100, 0}, Point{-100, 0}};
    CHECK(FormationScore({0, 256}, threats[0], threats) > FormationScore({0, 0}, threats[0], threats));
    CHECK(FormationScore({0, 256}, threats[0], threats) > FormationScore({0, 64}, threats[0], threats));
}

TEST_CASE("Unprotected caster defense repositions instead of assuming magic resistance helps a shield") {
    for (const auto kind : {Kind::Projectile, Kind::Concentration, Kind::Area}) {
        std::array threats{Threat{1, kind, 900, 0, 288, true, false, true, false, false, true, true}};
        Snapshot state{true, false, true, 150, 300, threats};
        CHECK(Choose(state).motion == (kind == Kind::Area ? Motion::Lateral : Motion::AdvanceLateral));
        CHECK_FALSE(Choose(state).guard);
    }
}

TEST_CASE("Recovery and counters yield to overlapping and rear threats") {
    std::array threats{Threat{1, Kind::Melee, 100, 0, 288, true, false, false, false, true, true, true},
                       Threat{2, Kind::Melee, 180, 120, 288, true, false, false, false, false, true, true}};
    Snapshot state{true, false, true, 150, 300, threats};
    CHECK(Choose(state).intent == Intent::Counter);
    state.recovering = true;
    CHECK(Choose(state).intent == Intent::Recover);
    CHECK(Choose(state).motion == Motion::Retreat);
    threats[1].attacking = true;
    CHECK(Choose(state).intent == Intent::Reposition);
    CHECK_FALSE(Choose(state).offense);
}

TEST_CASE("Caster interruption needs a close visible windup, facing and stamina reserve") {
    for (const auto kind : {Kind::Projectile, Kind::Concentration, Kind::Area}) {
        std::array threats{Threat{1, kind, 130, 0, 288, true, false, false, true, false, true, true}};
        Snapshot state{true, false, true, 150, 300, threats};
        CHECK(Choose(state).intent == Intent::Interrupt);
        CHECK(Choose(state).guard);
        CHECK(Choose(state).interrupt);
        CHECK(Choose(state).motion == Motion::None);
        state.stamina = 80;
        CHECK_FALSE(Choose(state).interrupt);
        state.stamina = 150;
        state.recovering = true;
        CHECK_FALSE(Choose(state).interrupt);
        state.recovering = false;
        threats[0].distance = 300;
        CHECK_FALSE(Choose(state).interrupt);
        threats[0].distance = 130;
        threats[0].visible = false;
        CHECK_FALSE(Choose(state).interrupt);
        threats[0].visible = true;
        threats[0].heading = 70;
        CHECK_FALSE(Choose(state).interrupt);
        threats[0].heading = 100;
        CHECK_FALSE(Choose(state).interrupt);
        threats[0].heading = 0;
        threats[0].windup = false;
        threats[0].attacking = true;
        CHECK_FALSE(Choose(state).interrupt);
        CHECK(Choose(state).offense == (kind != Kind::Area));
    }
}

TEST_CASE("Diagonal caster approach reduces range while retaining clearance margins") {
    using namespace mod::defender_movement_rules;
    for (const auto distance : {260.0f, 400.0f, 900.0f, 1500.0f}) {
        const auto step = AdvanceLateralStep(distance, 384, 256);
        CHECK(step.x > step.y);
        CHECK(step.y > 0);
        CHECK(std::hypot(distance - step.x, step.y) < distance);
        const std::array threat{Point{distance, 0}};
        CHECK(ClearStep({0, 0}, step, threat, false));
    }
    CHECK(AdvanceLateralStep(192, 384, 256).x == 0);
    CHECK(AdvanceLateralStep(900, -1, 256).x == 0);
    CHECK(AdvanceLateralStep(std::numeric_limits<float>::quiet_NaN(), 384, 256).x == 0);
}

TEST_CASE("Recovery creates breathing room without pursuing ranged targets or retreating indefinitely") {
    Snapshot unreadable{true, true, true, 100, 300, {}};
    CHECK(Choose(unreadable).intent == Intent::Recover);
    CHECK_FALSE(Choose(unreadable).offense);
    for (const auto kind : {Kind::Melee, Kind::Bow, Kind::Projectile}) {
        std::array threats{Threat{.actor = 1, .kind = kind, .distance = 100, .self = true}};
        Snapshot state{true, true, true, 100, 300, threats};
        auto decision = Choose(state);
        CHECK(decision.intent == Intent::Recover);
        CHECK(decision.motion == Motion::Retreat);
        CHECK_FALSE(decision.guard);
        CHECK_FALSE(decision.offense);
        threats[0].distance = 1000;
        decision = Choose(state);
        CHECK(decision.intent == Intent::Recover);
        CHECK(decision.motion == Motion::None);
        CHECK_FALSE(decision.guard);
        threats[0].attacking = true;
        if (kind == Kind::Projectile)
            CHECK(Choose(state).motion == Motion::Lateral);
    }
}

TEST_CASE("Defender pressures ally threats, respects pursuit bounds and yields to restrictions") {
    std::array threats{Threat{1, Kind::Melee, 500, 0, 288, false, true, true, false, false, true, true}};
    Snapshot state{true, false, true, 150, 300, threats};
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

TEST_CASE("Recovery spacing has a release margin and refreshes retain unfinished waypoints") {
    using namespace mod::defender_movement_rules;
    CHECK(KeepWaypoint(true, true, false, {0, 0}, {-256, 0}));
    CHECK(KeepWaypoint(true, true, false, {-100, 0}, {-256, 0}));
    CHECK_FALSE(KeepWaypoint(true, true, false, {-200, 0}, {-256, 0}));
    CHECK_FALSE(KeepWaypoint(true, true, true, {0, 0}, {-256, 0}));
    CHECK_FALSE(KeepWaypoint(true, false, false, {0, 0}, {-256, 0}));
    CHECK_FALSE(KeepWaypoint(false, true, false, {0, 0}, {-256, 0}));
    CHECK_FALSE(KeepWaypoint(true, true, false, {1000, 0}, {-256, 0}));
    std::array threats{Threat{.actor = 1, .kind = Kind::Melee, .distance = 450, .self = true}};
    Snapshot state{true, true, true, 100, 300, threats};
    CHECK(Choose(state).motion == Motion::None);
    state.retreating = true;
    CHECK(Choose(state).motion == Motion::Retreat);
    threats[0].distance = 480;
    CHECK(Choose(state).motion == Motion::None);
    threats[0].distance = 100;
    threats[0].attacking = true;
    CHECK(Choose(state).intent == Intent::Guard);
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
