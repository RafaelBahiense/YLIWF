#include <doctest/doctest.h>
#include "combat/actions/Guard.h"
#include "combat/actions/Bash.h"
#include <vector>

using namespace mod::combat_actions;
namespace guard = mod::combat_actions::guard_rules;
namespace bash = mod::combat_actions::bash_rules;

namespace {
    struct Dispatcher {
        Dispatch outcome = Dispatch::Accepted;
        std::vector<Intent> calls;

        Receipt operator()(Intent intent) {
            calls.push_back(intent);
            return {outcome, {}, true};
        }
    };

    GuardSample Idle() {
        GuardSample sample;
        sample.blockReadable = sample.graphIntentReadable = sample.reactionReadable = true;
        sample.graphIntent = 0;
        return sample;
    }

    GuardSample Blocking() {
        auto sample = Idle();
        sample.blocking = sample.wantsBlock = true;
        sample.graphIntent = 1;
        return sample;
    }

    GuardTransition Frame(GuardExecution& action, Dispatcher& dispatch, const GuardSample& sample, float delta = 0.1f,
                          bool wanted = true, bool yielding = false) {
        action.Advance(delta);
        const auto reaction = action.ObserveReaction(sample, wanted, delta);
        return action.Update(sample, wanted, true, true, yielding, reaction, dispatch);
    }
}

TEST_CASE("Guard execution acknowledges its start then sends one release and observes cleanup") {
    GuardExecution action;
    Dispatcher dispatch;
    CHECK(Frame(action, dispatch, Idle()).step == guard::Step::Start);
    CHECK(action.Read().phase == guard::Phase::StartPending);
    CHECK(Frame(action, dispatch, Idle()).step == guard::Step::None);
    CHECK(Frame(action, dispatch, Blocking()).step == guard::Step::Acknowledge);
    CHECK(Frame(action, dispatch, Blocking()).step == guard::Step::None);
    CHECK(Frame(action, dispatch, Blocking(), 0.1f, false).step == guard::Step::Release);
    CHECK(action.Read().releaseCompleted);
    const auto result = Frame(action, dispatch, Idle(), 0.2f, false);
    CHECK(result.step == guard::Step::Finished);
    CHECK(result.cleanupConfirmed);
    CHECK(result.releaseObserved);
    CHECK(action.Read().phase == guard::Phase::Idle);
    REQUIRE(dispatch.calls.size() == 2);
    CHECK(dispatch.calls[0] == Intent::Guard);
    CHECK(dispatch.calls[1] == Intent::ReleaseGuard);
}

TEST_CASE("A missing guard acknowledgement releases once and remembers the missed start") {
    GuardExecution action;
    Dispatcher dispatch;
    Frame(action, dispatch, Idle());
    const auto release = Frame(action, dispatch, Idle(), guard::AcknowledgeTimeout);
    CHECK(release.step == guard::Step::Release);
    CHECK(release.missedStart);
    CHECK(action.Read().missedStart);
    CHECK(Frame(action, dispatch, Idle(), 0.2f, false).cleanupConfirmed);
    CHECK(action.TakeMissedStart());
    CHECK_FALSE(action.TakeMissedStart());
    CHECK(dispatch.calls.size() == 2);
}

TEST_CASE("Unconfirmed guard dispatch cannot replay or be cleared just because the actor looks idle") {
    GuardExecution action;
    Dispatcher dispatch{Dispatch::Unconfirmed};
    Frame(action, dispatch, Idle());
    CHECK(action.Read().phase == guard::Phase::Fault);
    CHECK_FALSE(action.Reconcile(Idle()));
    Frame(action, dispatch, Idle(), 10);
    CHECK(dispatch.calls.size() == 1);
}

TEST_CASE("Rejected guard release keeps ownership until both graph and intent clear") {
    GuardExecution action;
    Dispatcher dispatch;
    Frame(action, dispatch, Idle());
    Frame(action, dispatch, Blocking());
    dispatch.outcome = Dispatch::Rejected;
    Frame(action, dispatch, Blocking(), 0.1f, false);
    auto intentOnly = Idle();
    intentOnly.graphIntent = 1;
    CHECK(Frame(action, dispatch, intentOnly, 0.2f, false).step == guard::Step::None);
    const auto result = Frame(action, dispatch, Idle(), 0.2f, false);
    CHECK(result.cleanupConfirmed);
    CHECK_FALSE(result.releaseObserved);
    CHECK(dispatch.calls.size() == 2);
}

TEST_CASE("Guard reaction deferral expires without issuing an unsafe release") {
    GuardExecution action;
    Dispatcher dispatch;
    Frame(action, dispatch, Idle());
    Frame(action, dispatch, Blocking());
    auto reacting = Blocking();
    reacting.reacting = true;
    CHECK(Frame(action, dispatch, reacting, 0.2f, false).step == guard::Step::None);
    const auto result = Frame(action, dispatch, reacting, guard::BlockReactionTimeout, false);
    CHECK(result.step == guard::Step::Fault);
    CHECK(result.timeout);
    CHECK(action.Read().releaseNotIssued);
    CHECK(dispatch.calls.size() == 1);
    CHECK(action.Reconcile(Idle()));
}

TEST_CASE("An unconfirmed release retains fault ownership even after apparent cleanup") {
    GuardExecution action;
    Dispatcher dispatch;
    Frame(action, dispatch, Idle());
    Frame(action, dispatch, Blocking());
    dispatch.outcome = Dispatch::Unconfirmed;
    Frame(action, dispatch, Blocking(), 0.1f, false);
    CHECK(action.Read().phase == guard::Phase::Fault);
    CHECK_FALSE(action.Read().releaseCompleted);
    CHECK_FALSE(action.Reconcile(Idle()));
    Frame(action, dispatch, Idle(), 10, false);
    CHECK(dispatch.calls.size() == 2);
}

TEST_CASE("Guard yielding retains accepted work and reports confirmed interrupted cleanup") {
    GuardExecution action;
    Dispatcher dispatch;
    Frame(action, dispatch, Idle());
    CHECK(Frame(action, dispatch, Blocking(), 0.1f, false, true).step == guard::Step::Acknowledge);
    CHECK(Frame(action, dispatch, Blocking(), 0.1f, false, true).step == guard::Step::Release);
    CHECK(Frame(action, dispatch, Idle(), 0.2f, false, true).cleanupConfirmed);
    CHECK(dispatch.calls.size() == 2);
}

TEST_CASE("Foreign guard ownership and restored releases never send a replacement command") {
    GuardExecution action;
    Dispatcher dispatch;
    Frame(action, dispatch, Idle());
    Frame(action, dispatch, Blocking());
    action.YieldOwnership();
    CHECK(action.Read().phase == guard::Phase::Idle);
    CHECK(dispatch.calls.size() == 1);

    CHECK(action.Restore(guard::Phase::ReleasePending, true) == guard::Recovery::ObserveRelease);
    CHECK(Frame(action, dispatch, Idle(), 0.2f, false).step == guard::Step::Finished);
    CHECK(dispatch.calls.size() == 1);
    CHECK(action.Restore(guard::Phase::StartPending, false) == guard::Recovery::Manual);
    CHECK_FALSE(action.Reconcile(Idle()));
}

TEST_CASE("Graph-only synchronous bash evidence survives dispatch and needs confirmed end plus idle samples") {
    BashExecution action;
    unsigned calls = 0;
    const auto receipt = action.Start(1, 2, 3, Intent::PowerBash, [&](Intent intent) {
        ++calls;
        CHECK(intent == Intent::PowerBash);
        CHECK(action.Request(1, 2) == 3);
        REQUIRE(action.Record(1, 2, 3, AnimationEvent::Started));
        REQUIRE(action.Record(1, 2, 3, AnimationEvent::Ended));
        return Receipt{Dispatch::Accepted};
    });
    CHECK(receipt.outcome == Dispatch::Accepted);
    const BashSample idle{false, false, true, false};
    const auto first = action.Observe(idle, 0.1f);
    CHECK(first.step == bash::Step::Observed);
    CHECK(first.graphStart);
    CHECK(first.graphEnd);
    CHECK_FALSE(first.nativeStart);
    CHECK(action.Observe(idle, 0.1f).step == bash::Step::None);
    CHECK(action.Observe(idle, 0.1f).step == bash::Step::Finished);
    CHECK(action.Request(1, 2) == 0);
    CHECK_FALSE(action.Record(1, 2, 3, AnimationEvent::Started));
    CHECK(calls == 1);
}

TEST_CASE("Bash interruption preserves graph-end ownership and never counts as normal completion") {
    BashExecution action;
    Dispatcher dispatch;
    action.Start(1, 2, 3, Intent::Bash, dispatch);
    action.Record(1, 2, 3, AnimationEvent::Started);
    CHECK(action.Observe({true, false, false, false}, 0.1f).step == bash::Step::Observed);
    CHECK(action.Observe({false, true, true, false}, 0.1f).step == bash::Step::None);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::None);
    action.Record(1, 2, 3, AnimationEvent::Ended);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::None);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::Interrupted);
    CHECK(action.Phase() == bash::Phase::Idle);
}

TEST_CASE("Teardown before bash start retains the delayed action and returns interrupted cleanup") {
    BashExecution action;
    Dispatcher dispatch;
    action.Start(1, 2, 3, Intent::Bash, dispatch);
    action.Teardown();
    action.Record(1, 2, 3, AnimationEvent::Started);
    action.Record(1, 2, 3, AnimationEvent::Ended);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::Observed);
    action.Observe({false, false, true, false}, 0.1f);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::Interrupted);
}

TEST_CASE("Bash timeouts and unknown dispatches do not permit a new request") {
    for (const auto outcome : {Dispatch::Accepted, Dispatch::Unconfirmed}) {
        BashExecution action;
        Dispatcher dispatch{outcome};
        action.Start(1, 2, 3, Intent::Bash, dispatch);
        CHECK(action.Observe({}, bash::StartTimeout).step == bash::Step::Fault);
        CHECK(action.Start(1, 2, 4, Intent::PowerBash, dispatch).outcome == Dispatch::Rejected);
        CHECK(action.Request(1, 2) == 3);
        CHECK(dispatch.calls.size() == 1);
    }
}

TEST_CASE("Native-only bash observation waits through recovery and requires two fresh idle samples") {
    BashExecution action;
    Dispatcher dispatch;
    action.Start(1, 2, 3, Intent::Bash, dispatch);
    const auto start = action.Observe({true, false, false, false}, 0.1f);
    CHECK(start.step == bash::Step::Observed);
    CHECK(start.nativeStart);
    CHECK_FALSE(start.graphStart);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::None);
    CHECK(action.Observe({false, false, true, true}, 0.1f).step == bash::Step::None);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::None);
    CHECK(action.Observe({false, false, true, false}, 0.1f).step == bash::Step::Finished);
    CHECK(action.Request(1, 2) == 0);
}

TEST_CASE("Rejected bash and reset invalidate callback ownership across requests and generations") {
    BashExecution action;
    Dispatcher dispatch{Dispatch::Rejected};
    action.Start(1, 2, 3, Intent::Bash, dispatch);
    CHECK(action.Phase() == bash::Phase::Idle);
    CHECK(action.Request(1, 2) == 0);
    dispatch.outcome = Dispatch::Accepted;
    action.Start(1, 2, 4, Intent::Bash, dispatch);
    CHECK_FALSE(action.Record(1, 2, 3, AnimationEvent::Started));
    action.Reset();
    action.Start(1, 9, 1, Intent::Bash, dispatch);
    CHECK_FALSE(action.Record(1, 2, 4, AnimationEvent::Started));
    CHECK(action.Request(1, 9) == 1);
}
