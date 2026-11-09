#include <doctest/doctest.h>
#include "combat/DefenderActionRules.h"
#include "combat/actions/DispatchRules.h"

TEST_CASE("Recovery blocks new offense during external commitments without applying stale policy") {
    using namespace mod::combat_actions;
    CHECK(RejectNewOffense(RecoveryOffense, true, true));
    CHECK(RejectNewOffense(RecoveryOffense, true, false));
    CHECK_FALSE(RejectNewOffense(SuppressOffense, true, true));
    CHECK(RejectNewOffense(SuppressOffense, true, false));
    CHECK_FALSE(RejectNewOffense(RecoveryOffense, false, true));
    CHECK_FALSE(RejectNewOffense(0, true, false));
    CHECK(RejectDiscretionaryGuard(RecoveryGuard, true, false, LeftAction::Block));
    CHECK_FALSE(RejectDiscretionaryGuard(RecoveryGuard, true, true, LeftAction::Block));
    CHECK_FALSE(RejectDiscretionaryGuard(RecoveryGuard, false, false, LeftAction::Block));
    CHECK_FALSE(RejectDiscretionaryGuard(RecoveryGuard, true, false, LeftAction::Bash));
}

#include <limits>

#include "combat/actions/GuardRules.h"

using namespace mod::combat_actions::guard_rules;
using namespace mod::defender_action_rules;

TEST_CASE("Guard acquisition waits for locomotion eligibility and releases preparation after acknowledgement") {
    MovementHandoff handoff;
    handoff.Update(.1f, true, false, true, true, true, Phase::Idle, true);
    CHECK(handoff.Preparing());
    CHECK_FALSE(handoff.Ready());
    handoff.Update(.1f, true, false, true, true, true, Phase::Idle, false);
    CHECK_FALSE(handoff.Ready());
    handoff.Update(.1f, true, false, true, true, true, Phase::Idle, false);
    CHECK(handoff.Ready());
    handoff.Update(.1f, true, false, true, false, false, Phase::StartPending, false);
    CHECK(handoff.Preparing());
    handoff.Update(.1f, true, false, true, false, false, Phase::Holding, false);
    CHECK_FALSE(handoff.Preparing());
    handoff.Update(.1f, true, false, false, false, false, Phase::ReleasePending, false);
    CHECK(handoff.Preparing());
    handoff.Update(.1f, true, false, true, false, true, Phase::Idle, false);
    CHECK_FALSE(handoff.Preparing());  // An unarmed episode cannot retain preparation.
}

TEST_CASE("Direct primitive tests and lost permission do not retain guard preparation") {
    MovementHandoff handoff;
    handoff.Update(.1f, true, false, true, true, true, Phase::Idle, true);
    handoff.Update(.1f, false, false, true, true, true, Phase::StartPending, false);
    CHECK_FALSE(handoff.Preparing());
    CHECK_FALSE(handoff.Ready());
    handoff.Update(.1f, true, true, true, true, true, Phase::Idle, true);
    CHECK_FALSE(handoff.Preparing());
    CHECK(handoff.Ready());
}

TEST_CASE("Stationary melee guards retain immediate eligibility without a movement settling delay") {
    MovementHandoff handoff;
    handoff.Update(.01f, true, false, true, true, true, Phase::Idle, false);
    CHECK(handoff.Ready());
}

TEST_CASE("Guard locomotion eligibility requires observed walking speed and a drawn weapon") {
    CHECK(GuardLocomotionReady(0, false, true));
    CHECK(GuardLocomotionReady(180, false, true));
    CHECK_FALSE(GuardLocomotionReady(181, false, true));
    CHECK_FALSE(GuardLocomotionReady(100, true, true));
    CHECK_FALSE(GuardLocomotionReady(0, false, false));
    CHECK_FALSE(GuardLocomotionReady(-1, false, true));
    CHECK_FALSE(GuardLocomotionReady(std::numeric_limits<float>::quiet_NaN(), false, true));
}

TEST_CASE("Guard tuning changes eligibility and settling without inventing action acknowledgement") {
    CHECK_FALSE(GuardLocomotionReady(150, false, true, 120));
    CHECK(GuardLocomotionReady(150, false, true, 180));
    MovementHandoff handoff;
    handoff.Update(.1f, true, false, true, true, true, Phase::Idle, true, .4f);
    for (int i = 0; i < 3; ++i) {
        handoff.Update(.1f, true, false, true, true, true, Phase::Idle, false, .4f);
        CHECK_FALSE(handoff.Ready());
    }
    handoff.Update(.1f, true, false, true, true, true, Phase::Idle, false, .4f);
    CHECK(handoff.Ready());
}

TEST_CASE("Active dodge defers fresh guards and yields offense suppression without assuming unavailable flags") {
    CHECK(ActiveDodge(true, true));
    CHECK_FALSE(ActiveDodge(false, true));
    CHECK_FALSE(ActiveDodge(true, false));
    CHECK(Decide(Phase::Idle, true, true, false, true, 0, true, false, true) == Step::None);
    CHECK(Decide(Phase::Idle, true, true, false, true, 0, true, false, false) == Step::Start);
    CHECK_FALSE(Suppress(true, true, true, true, true));
    CHECK_FALSE(Suppress(true, true, false, true, true));
    CHECK(Suppress(true, true, true, true, false));
}

TEST_CASE("Defender offense gating requires live recovery or guard intent") {
    CHECK(Suppress(true, true, false, true));
    CHECK(Suppress(true, false, true, true));
    CHECK_FALSE(Suppress(true, false, false, true));
    CHECK_FALSE(Suppress(false, true, true, true));
    CHECK_FALSE(Suppress(true, true, true, false));
    CHECK_FALSE(Suppress(true, true, true, true, true));
}
