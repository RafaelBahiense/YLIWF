#include <doctest/doctest.h>
#include "interference/ExternalAttackRules.h"

using namespace mod::combat_tests::interference::external_attack_rules;

TEST_CASE("External attack handoff recognizes only supported module and attack pairs") {
    CHECK(Supported("ProjectGapClose.dll", "attackPowerStart_Sprint"));
    CHECK(Supported("projectgapclose.DLL", "attackStartSprint"));
    CHECK_FALSE(Supported("SkyrimSE.exe", "attackStartSprint"));
    CHECK_FALSE(Supported("SCAR.dll", "attackStartSprint"));
    CHECK_FALSE(Supported("ProjectGapClose.dll", "attackStop"));
    CHECK_FALSE(Supported("ProjectGapClose.dll", "blockStart"));
    CHECK(Classify("FixNPCPermaBlock.dll", "attackStart") == Source::PermaBlock);
    CHECK(Classify("fixnpcpermablock.DLL", "ATTACKSTART") == Source::PermaBlock);
    CHECK(Classify("ProjectGapClose.dll", "attackStartSprint") == Source::GapClose);
    CHECK_FALSE(Supported("SkyrimSE.exe", "attackStart"));
    CHECK_FALSE(Supported("Other.dll", "attackStart"));
    CHECK_FALSE(Supported("FixNPCPermaBlock.dll", "attackStop"));
    CHECK_FALSE(Supported("FixNPCPermaBlock.dll", "blockStart"));
    CHECK_FALSE(Supported("FixNPCPermaBlock.dll", "attackStartSprint"));
    CHECK_FALSE(Supported("ProjectGapClose.dll", "attackStart"));
}

TEST_CASE("PermaBlock normal attack retains ownership across the delayed native attack state") {
    REQUIRE(Supported("FixNPCPermaBlock.dll", "attackStart"));
    State state;
    state.Event("SBF_NormalAttackStart");
    state.returned = state.accepted = true;
    for (int i = 0; i < 20; ++i)
        CHECK_FALSE(state.Sample(false, true, false));
    CHECK_FALSE(state.Sample(true, true, false));
    state.Event("SBF_NormalAttackStop");
    CHECK_FALSE(state.Sample(false, true, true));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
}

TEST_CASE("Accepted sprint stays committed across the pre-swing engine-state gap") {
    State state;
    state.returned = state.accepted = true;
    for (int i = 0; i < 100; ++i)
        CHECK_FALSE(state.Sample(false, true, false));
    state.Event("attackStop");  // Old stop before a start must not establish completion.
    CHECK_FALSE(state.Sample(false, true, false));
    state.Event("MCO_SprintPowerAttackInitiate");
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK_FALSE(state.Sample(true, true, false));
    state.Event("SBF_PowerAttackStop");
    CHECK_FALSE(state.Sample(false, true, true));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
}

TEST_CASE("Rejected input rollback waits for stable idle and honors contradictory start evidence") {
    State rejected;
    CHECK_FALSE(rejected.Sample(false, true, false));
    rejected.returned = true;
    CHECK_FALSE(rejected.Sample(false, true, false));
    CHECK(rejected.Sample(false, true, false));
    State contradictory;
    contradictory.returned = true;
    contradictory.Event("SBF_NormalAttackStart");
    CHECK_FALSE(contradictory.Sample(false, true, false));
    CHECK_FALSE(contradictory.Sample(false, true, false));
    contradictory.Event("SBF_NormalAttackStop");
    CHECK_FALSE(contradictory.Sample(false, true, false));
    CHECK(contradictory.Sample(false, true, false));
}

TEST_CASE("Native idle alone cannot complete an attack without a recovery cycle") {
    State state;
    state.returned = state.accepted = true;
    CHECK_FALSE(state.Sample(true, false, false));
    CHECK_FALSE(state.Sample(false, false, false));
    CHECK_FALSE(state.Sample(true, false, false));
    CHECK_FALSE(state.Sample(false, false, false));
    CHECK_FALSE(state.Sample(false, false, false));
    CHECK_FALSE(state.Sample(false, true, true));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
    CHECK(state.finish == State::Finish::RecoveryCycle);
}

TEST_CASE("Hitbox end and generic attack stop cannot release a named animation early") {
    State state;
    state.returned = state.accepted = true;
    state.Event("SBF_NormalAttackStart");
    CHECK_FALSE(state.Sample(true, true, false));
    state.Event("Collision_AttackEnd");
    state.Event("attackStop");
    state.Event("SBF_PowerAttackStop");
    CHECK_FALSE(state.handingOff);
    CHECK_FALSE(state.Sample(false, true, true));
    for (int i = 0; i < 20; ++i)
        CHECK_FALSE(state.Sample(false, true, false));
    state.Event("SBF_NormalAttackStop");
    CHECK(state.handingOff);
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
    CHECK(state.finish == State::Finish::Animation);
}

TEST_CASE("Final animation begins handoff during recovery before the idle confirmation window") {
    State state;
    state.returned = state.accepted = true;
    state.Event("SBF_PowerAttackStart");
    CHECK_FALSE(state.Sample(true, true, false));
    state.Event("SBF_PowerAttackStop");
    CHECK(state.handingOff);
    CHECK_FALSE(state.Sample(true, true, true));
    CHECK_FALSE(state.Sample(false, true, true));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
    CHECK(state.finish == State::Finish::Animation);
}

TEST_CASE("Already issued follow-up attacks do not reopen the handoff to new ordinary attacks") {
    State state;
    state.returned = state.accepted = true;
    state.Event("SBF_PowerAttackStart");
    state.Event("SBF_PowerAttackStop");
    REQUIRE(state.handingOff);
    state.Event("SBF_NormalAttackStart");
    CHECK(state.handingOff);
    CHECK_FALSE(state.Sample(true, true, false));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK_FALSE(state.Sample(false, true, false));
    state.Event("SBF_NormalAttackStop");
    CHECK_FALSE(state.Sample(false, true, true));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
}

TEST_CASE("Renewed animation invalidates final stop and idle confirmation") {
    State state;
    state.returned = state.accepted = true;
    state.Event("SBF_NormalAttackStart");
    state.Event("SBF_NormalAttackStop");
    CHECK_FALSE(state.Sample(false, true, false));
    state.Event("SBF_NormalAttackStart");
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK_FALSE(state.Sample(false, true, false));
    state.Event("MCO_EndAnimation");
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK_FALSE(state.Sample(true, true, false));
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
}

TEST_CASE("A later graph start cannot use a previous attack cycle as its completion evidence") {
    State state;
    state.returned = state.accepted = true;
    CHECK_FALSE(state.Sample(true, true, false));
    CHECK_FALSE(state.Sample(false, true, false));
    state.Event("SBF_NormalAttackStart");
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK_FALSE(state.Sample(false, true, false));
    state.Event("SBF_NormalAttackStop");
    CHECK_FALSE(state.Sample(false, true, false));
    CHECK(state.Sample(false, true, false));
}
