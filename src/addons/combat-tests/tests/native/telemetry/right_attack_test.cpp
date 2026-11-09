#include <doctest/doctest.h>
#include "telemetry/AttackRules.h"
#include "../scenarios/PrimitiveDriver.h"

TEST_CASE("Normal attack evidence excludes outgoing requests intermediate collision events and generic stops") {
    using namespace mod::combat_tests::telemetry::attack_rules;
    using Event = yliwf::testing::AttackAnimationEvent;
    CHECK(Classify("SBF_NormalAttackStart") == Event::Started);
    CHECK(Classify("SBF_NormalAttackStop") == Event::Ended);
    CHECK(Classify("MCO_EndAnimation") == Event::Ended);
    CHECK(Classify("SBF_PowerAttackStart") == Event::ConflictingStarted);
    CHECK(Classify("SBF_BashStop") == Event::ConflictingEnded);
    CHECK(Classify("SBF_StaggerStart") == Event::Interrupted);
    CHECK_FALSE(Classify("attackStart"));
    CHECK_FALSE(Classify("attackStop"));
    CHECK_FALSE(Classify("Collision_AttackEnd"));
    CHECK_FALSE(Classify("Collision_AttackStart"));
    CHECK_FALSE(Classify("HitFrame"));
}

TEST_CASE("Right-hand phase issues once and requires its own completion evidence") {
    using namespace primitive_test;
    Driver test;
    REQUIRE(test.Begin(Kind::RightAttack));
    CHECK(test.requests.size() == 1);
    CHECK(test.requests[0] == harness::Action::RightAttack);
    yliwf::testing::DefenderObservation evidence;
    evidence.nativeBashCompleted = evidence.releaseObserved = 1;
    test.Tick(.1f, evidence);
    CHECK_FALSE(test.session.context.completion);
    evidence.rightAttackAccepted = evidence.rightAttackObserved = 1;
    test.Tick(.1f, evidence);
    CHECK_FALSE(test.session.context.completion);
    evidence.rightAttackCompleted = 1;
    test.Tick(.1f, evidence);
    REQUIRE(test.session.context.completion);
    CHECK(test.session.context.completion->verdict == harness::Verdict::Passed);
    CHECK(test.requests.size() == 1);
}

TEST_CASE("Right-hand phase rejection and interruption remain distinct from success") {
    using namespace primitive_test;
    for (const bool interrupted : {false, true}) {
        Driver test;
        REQUIRE(test.Begin(Kind::RightAttack));
        yliwf::testing::DefenderObservation evidence;
        evidence.rightAttackRejected = !interrupted;
        evidence.rightAttackInterrupted = interrupted;
        test.Tick(.1f, evidence);
        REQUIRE(test.session.context.completion);
        CHECK(test.session.context.completion->verdict == harness::Verdict::Inconclusive);
        CHECK(test.requests.size() == 1);
    }
}
