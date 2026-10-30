#include <doctest/doctest.h>
#include "combat/DefenderActionRules.h"
#include <limits>

using namespace mod::defender_action_rules;

TEST_CASE("Left action classification separates shield starts from bashes and unknown events") {
    CHECK(ClassifyLeftAction("blockStart") == LeftAction::Block);
    CHECK(ClassifyLeftAction("BLOCKSTART") == LeftAction::Block);
    CHECK(ClassifyLeftAction("bashStart") == LeftAction::Bash);
    CHECK(ClassifyLeftAction("BashPowerStart") == LeftAction::Bash);
    CHECK(ClassifyLeftAction("") == LeftAction::Unknown);
    CHECK(ClassifyLeftAction("blockStop") == LeftAction::Unknown);
    CHECK(ClassifyLeftAction("bashRelease") == LeftAction::Unknown);
    CHECK(ClassifyLeftAction("customAttack") == LeftAction::Unknown);
}

TEST_CASE("Offense suppression preserves native blocks while own guards require a resolved block") {
    CHECK_FALSE(RejectLeftAction(LeftAction::Block, false, true));
    CHECK_FALSE(RejectLeftAction(LeftAction::Block, true, true));
    CHECK(RejectLeftAction(LeftAction::Bash, false, true));
    CHECK_FALSE(RejectLeftAction(LeftAction::Bash, false, false));
    CHECK(RejectLeftAction(LeftAction::Bash, true, false));
    CHECK(RejectLeftAction(LeftAction::Unknown, true, false));
    CHECK_FALSE(RejectLeftAction(LeftAction::Unknown, false, true));
}

TEST_CASE("Completed engine rejection is distinct from accepted and unconfirmed dispatch") {
    CHECK(ClassifyDispatch(true, true, 0, 0) == Dispatch::Accepted);
    CHECK(ClassifyDispatch(true, false, 0, 0) == Dispatch::Rejected);
    CHECK(ClassifyDispatch(false, false, 0, 0) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, 2, 0) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, true, 2, 0) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, true, 0, 1) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, -1, 0) == Dispatch::Unconfirmed);
}

TEST_CASE("Loaded guard recovery observes issued releases without reclaiming ambiguous ownership") {
    CHECK(RestoreGuard(Phase::Idle, true) == Recovery::Finished);
    CHECK(RestoreGuard(Phase::Holding, false) == Recovery::Finished);
    CHECK(RestoreGuard(Phase::Holding, true) == Recovery::Manual);
    CHECK(RestoreGuard(Phase::StartPending, false) == Recovery::Manual);
    CHECK(RestoreGuard(Phase::ReleasePending, true) == Recovery::ObserveRelease);
    CHECK(RestoreGuard(Phase::ReleasePending, false) == Recovery::ObserveRelease);
    CHECK(RestoreGuard(Phase::Fault, false) == Recovery::Manual);
}

TEST_CASE("Pre-dispatch rejection requires observed engine evidence and a completed failure") {
    CHECK(ClassifyDispatch(true, false, -1, 0, true) == Dispatch::Rejected);
    CHECK(ClassifyDispatch(true, false, -1, 0, false) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(false, false, -1, 0, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, true, -1, 0, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, -1, 1, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, -1, 2, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, 1, 0, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, 2, 0, true) == Dispatch::Unconfirmed);
}

TEST_CASE("Native defender modes isolate recovery suppression from guard requests") {
    CHECK_FALSE(Suppress(Mode::Styles, true, true, true, true));
    CHECK(Suppress(Mode::RecoveryGate, true, true, false, true));
    CHECK_FALSE(Suppress(Mode::RecoveryGate, true, false, true, true));
    CHECK_FALSE(Suppress(Mode::Guard, true, true, true, true));
    CHECK(Suppress(Mode::Combined, true, false, true, true));
    CHECK_FALSE(Suppress(Mode::Combined, false, true, true, true));
    CHECK_FALSE(Suppress(Mode::Combined, true, true, true, false));
    CHECK_FALSE(ValidMode(4));
    CHECK(ValidMode(0));
    CHECK(ValidMode(3));
}

TEST_CASE("Guard starts only at an idle opportunity and does not take an existing block") {
    CHECK(Decide(Phase::Idle, true, true, false, true, 0) == Step::Start);
    CHECK(Decide(Phase::Idle, true, false, false, true, 0) == Step::None);
    CHECK(Decide(Phase::Idle, true, true, true, true, 0) == Step::None);
    CHECK(Decide(Phase::Idle, true, true, false, false, 0) == Step::None);
    CHECK(Decide(Phase::Idle, false, true, false, true, 0) == Step::None);
}

TEST_CASE("Accepted guards require acknowledgement and a single bounded release") {
    CHECK(Decide(Phase::StartPending, true, true, false, false, 0.2f) == Step::None);
    CHECK(Decide(Phase::StartPending, true, true, true, false, 0.2f) == Step::Acknowledge);
    CHECK(Decide(Phase::StartPending, true, true, false, false, AcknowledgeTimeout) == Step::Release);
    CHECK(Decide(Phase::Holding, true, true, true, false, 0) == Step::None);
    CHECK(Decide(Phase::Holding, false, true, true, false, 0) == Step::Release);
    CHECK(Decide(Phase::Holding, true, true, false, false, 0) == Step::Finished);
    CHECK(Decide(Phase::ReleasePending, false, true, true, false, 0.2f) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, 0) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, 0.2f) == Step::Finished);
    CHECK(Decide(Phase::ReleasePending, false, true, true, false, ReleaseTimeout) == Step::Fault);
    CHECK(Decide(Phase::Fault, true, true, true, true, 10) == Step::Fault);
    CHECK(Decide(Phase::Holding, true, true, true, false, std::numeric_limits<float>::quiet_NaN()) == Step::Fault);
}

TEST_CASE("Unreadable blocking state cannot acknowledge or clear guard ownership") {
    CHECK(Decide(Phase::Idle, true, true, false, true, 0, false) == Step::None);
    CHECK(Decide(Phase::StartPending, true, true, true, false, 0.2f, false) == Step::None);
    CHECK(Decide(Phase::StartPending, true, true, false, false, AcknowledgeTimeout, false) == Step::Release);
    CHECK(Decide(Phase::Holding, true, true, false, false, 0.2f, false) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, 0.2f, false) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, ReleaseTimeout, false) == Step::Fault);
}

TEST_CASE("A rejected release clears only after both graph blocking and block intent are absent") {
    CHECK_FALSE(ReleaseCleared(true, false, true));
    CHECK_FALSE(ReleaseCleared(true, true, false));
    CHECK_FALSE(ReleaseCleared(false, false, false));
    CHECK(ReleaseCleared(true, false, false));
    CHECK(RestoreGuard(Phase::Holding, !ReleaseCleared(true, false, true)) == Recovery::Manual);
    CHECK(RestoreGuard(Phase::Holding, !ReleaseCleared(false, false, false)) == Recovery::Manual);
    CHECK(Decide(Phase::Idle, true, true, false, true, 0, true, true) == Step::None);
    CHECK(Decide(Phase::Holding, true, true, false, false, 0.2f, true, true) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, 0.2f, true, true) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, ReleaseTimeout, true, true) == Step::Fault);
    CHECK(Decide(Phase::ReleasePending, false, true, false, false, 0.2f, true, false) == Step::Finished);
}

TEST_CASE("Recovery rechecks never discard an unconfirmed release even when the actor looks idle") {
    CHECK(ReconcileRelease(true, true, false, false));
    CHECK_FALSE(ReconcileRelease(false, true, false, false));
    CHECK_FALSE(ReconcileRelease(true, false, false, false));
    CHECK_FALSE(ReconcileRelease(true, true, true, false));
    CHECK_FALSE(ReconcileRelease(true, true, false, true));
}
