#include <doctest/doctest.h>
#include "telemetry/AttackRules.h"
#include "PrimitiveScenario.h"

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

TEST_CASE("Direct normal attack scenario issues once and requires its own completion evidence") {
    using namespace mod::combat_tests::primitive;
    Scenario scenario;
    yliwf::testing::DefenderObservation evidence;
    CHECK(scenario.Advance(0.1f, Kind::RightAttack, evidence) == Step::Attack);
    evidence.nativeBashCompleted = evidence.releaseObserved = 1;
    CHECK(scenario.Advance(0.1f, Kind::RightAttack, evidence) == Step::Wait);
    evidence.rightAttackAccepted = evidence.rightAttackObserved = 1;
    CHECK(scenario.Advance(0.1f, Kind::RightAttack, evidence) == Step::Wait);
    evidence.rightAttackCompleted = 1;
    CHECK(scenario.Advance(0.1f, Kind::RightAttack, evidence) == Step::Passed);
}

TEST_CASE("Direct normal attack rejection and interruption remain distinct from success") {
    using namespace mod::combat_tests::primitive;
    Scenario scenario;
    yliwf::testing::DefenderObservation evidence;
    scenario.Advance(0.1f, Kind::RightAttack, evidence);
    evidence.rightAttackRejected = 1;
    CHECK(scenario.Advance(0.1f, Kind::RightAttack, evidence) == Step::Inconclusive);
    scenario = {};
    evidence = {};
    scenario.Advance(0.1f, Kind::RightAttack, evidence);
    evidence.rightAttackInterrupted = 1;
    CHECK(scenario.Advance(0.1f, Kind::RightAttack, evidence) == Step::Inconclusive);
}
