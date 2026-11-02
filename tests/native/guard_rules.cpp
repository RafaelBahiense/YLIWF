#include <doctest/doctest.h>
#include "combat/actions/GuardRules.h"
#include <limits>

using namespace mod::combat_actions::guard_rules;

TEST_CASE("Release reaction credit requires an accepted release and cleared intent") {
    CHECK(ReleaseReactionActive(Phase::ReleasePending, true, false, true, true, false, true));
    CHECK_FALSE(ReleaseReactionActive(Phase::ReleasePending, false, false, true, true, false, true));
    CHECK_FALSE(ReleaseReactionActive(Phase::ReleasePending, true, true, true, true, false, true));
    CHECK_FALSE(ReleaseReactionActive(Phase::ReleasePending, true, false, false, true, false, true));
    CHECK_FALSE(ReleaseReactionActive(Phase::ReleasePending, true, false, true, true, true, true));
    CHECK_FALSE(ReleaseReactionActive(Phase::ReleasePending, true, false, true, true, false, false));
    CHECK_FALSE(ReleaseReactionActive(Phase::Holding, true, false, true, true, false, true));
}

TEST_CASE("Reaction after release pauses the ordinary deadline with a finite cumulative budget") {
    // Late hit begins after 0.2 s; 0.8 s of confirmed reaction does not exhaust the release deadline.
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 1.0f, true, false, false, true, 0.8f) == Step::None);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 1.5f, true, false, false, false, 0.8f) ==
          Step::None);
    CHECK(Decide(Phase::ReleasePending, false, false, false, false, 1.6f, true, false, false, false, 0.8f) ==
          Step::Finished);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 2.0f, true, false, false, false, 0.8f) ==
          Step::Fault);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 3.2f, true, false, false, true, 3.0f) ==
          Step::Fault);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 4.0f, true, false, false, true, 2.9f) ==
          Step::Fault);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 1.0f, true, false, false, true, 0) == Step::Fault);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, 1.0f, true, false, false, true, 1.1f) ==
          Step::Fault);
}

TEST_CASE("Observed block-hit reaction defers release before dispatch with a separate finite bound") {
    CHECK(Decide(Phase::Holding, false, false, true, false, 20, true, true, false, true, 0) == Step::None);
    CHECK(Decide(Phase::Holding, false, false, true, false, 20, true, true, false, true, 1.5f) == Step::None);
    CHECK(Decide(Phase::Holding, false, false, true, false, 20, true, true, false, true, BlockReactionTimeout) ==
          Step::Fault);
    CHECK(Decide(Phase::Holding, false, false, true, false, 20, true, true, false, false, 0) == Step::Release);
    CHECK(Decide(Phase::Holding, false, false, false, false, 20, true, false, false, true, 2) == Step::Finished);
    CHECK(Decide(Phase::Holding, true, false, true, false, 20, true, true, false, true, 10) == Step::None);
    CHECK(Decide(Phase::StartPending, false, false, false, false, AcknowledgeTimeout, true, false, false, true, 0) ==
          Step::Release);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, ReleaseTimeout, true, false, false, true, 0) ==
          Step::Fault);
}

TEST_CASE("Known graph intent prevents cleanup even after native block intent cleared") {
    CHECK(BlockIntent(false, true, 1));
    CHECK(BlockIntent(true, true, 0));
    CHECK_FALSE(BlockIntent(false, true, 0));
    CHECK_FALSE(BlockIntent(false, false, -1));
    CHECK(BlockIntent(false, true, 2));
    CHECK_FALSE(ReleaseCleared(true, false, BlockIntent(false, true, 1)));
    CHECK(ReleaseCleared(true, false, BlockIntent(false, true, 0)));
}

TEST_CASE("A dodge preserves acknowledgement and release deadlines for already accepted guards") {
    CHECK(GuardInterrupted(Phase::StartPending, true));
    CHECK(GuardInterrupted(Phase::Holding, true));
    CHECK_FALSE(GuardInterrupted(Phase::Idle, true));
    CHECK_FALSE(GuardInterrupted(Phase::ReleasePending, true));
    CHECK_FALSE(GuardInterrupted(Phase::StartPending, false));
    CHECK(Decide(Phase::StartPending, false, false, false, false, 0.2f, true, false, true) == Step::None);
    CHECK(Decide(Phase::StartPending, false, false, true, false, 0.2f, true, false, true) == Step::Acknowledge);
    CHECK(Decide(Phase::StartPending, false, false, false, false, AcknowledgeTimeout, true, false, true) ==
          Step::Release);
    CHECK(Decide(Phase::Holding, true, false, true, false, 0, true, false, true) == Step::Release);
    CHECK(Decide(Phase::Holding, true, false, false, false, 0, true, false, true) == Step::Finished);
    CHECK(Decide(Phase::ReleasePending, false, false, true, false, ReleaseTimeout, true, false, true) == Step::Fault);
}

TEST_CASE("Only an observed dodge interruption with confirmed cleanup can resume guard evaluation") {
    CHECK(ResumeAfterYield(Phase::Idle, true, true, true, false, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, false, true, true, false, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, true, false, true, false, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, true, true, false, false, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, true, true, true, true, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, true, true, true, false, false, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, true, true, true, false, true, true, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Idle, true, true, true, false, true, false, true));
    CHECK_FALSE(ResumeAfterYield(Phase::StartPending, true, true, true, false, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::ReleasePending, true, true, true, false, true, false, false));
    CHECK_FALSE(ResumeAfterYield(Phase::Fault, true, true, true, false, true, false, false));
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
