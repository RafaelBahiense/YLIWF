#pragma once
#include "Dispatch.h"
#include "GuardRules.h"
#include <utility>

namespace mod::combat_actions {
    struct GuardSample {
        bool blockReadable = false, blocking = false, wantsBlock = false;
        bool graphIntentReadable = false;
        std::int32_t graphIntent = -1;
        bool reactionReadable = false, reacting = false;

        bool IntentActive() const {
            return guard_rules::BlockIntent(wantsBlock, graphIntentReadable, graphIntent);
        }

        bool ReactionActive() const {
            return reactionReadable && reacting;
        }
    };

    struct GuardState {
        guard_rules::Phase phase = guard_rules::Phase::Idle;
        float elapsed = 0, releaseWait = 0;
        bool releaseRejected = false, missedStart = false, releaseCompleted = false;
        bool releaseNotIssued = false, releaseDeferred = false;
    };

    struct GuardReaction {
        bool beforeRelease = false, afterRelease = false, newlyDeferred = false;
    };

    struct GuardTransition {
        guard_rules::Step step = guard_rules::Step::None;
        Receipt receipt;
        bool missedStart = false, cleanupConfirmed = false, releaseObserved = false, timeout = false;
    };

    // Main-thread state machine. The caller supplies permission and tactical
    // intent; only this primitive issues guard/release and advances ownership.
    class GuardExecution {
    public:
        const GuardState& Read() const {
            return state;
        }

        void Advance(float delta) {
            state.elapsed += delta;
        }

        GuardReaction ObserveReaction(const GuardSample& sample, bool wanted, float delta) {
            using namespace guard_rules;
            GuardReaction result;
            result.beforeRelease = state.phase == Phase::Holding && !wanted && sample.ReactionActive() &&
                                   !ReleaseCleared(sample.blockReadable, sample.blocking, sample.IntentActive());
            result.afterRelease =
                ReleaseReactionActive(state.phase, state.releaseCompleted, state.releaseRejected,
                                      sample.blockReadable && sample.graphIntentReadable, sample.blocking,
                                      sample.IntentActive(), sample.ReactionActive());
            const bool waiting = result.beforeRelease || result.afterRelease;
            result.newlyDeferred = waiting && !state.releaseDeferred;
            if (waiting)
                state.releaseWait += delta;
            else if (state.phase != Phase::ReleasePending)
                state.releaseWait = 0;
            state.releaseDeferred = waiting;
            return result;
        }

        template <class DispatchAction>
        GuardTransition Update(const GuardSample& sample, bool wanted, bool eligible, bool armed, bool yielding,
                               const GuardReaction& reaction, DispatchAction&& dispatch) {
            using namespace guard_rules;
            GuardTransition result;
            result.step =
                Decide(state.phase, wanted, eligible, sample.blocking, armed, state.elapsed, sample.blockReadable,
                       sample.IntentActive(), yielding, sample.ReactionActive(), state.releaseWait);
            switch (result.step) {
                case Step::Start:
                    state.elapsed = 0;
                    state.phase = Phase::StartPending;
                    state.releaseRejected = state.missedStart = state.releaseCompleted = state.releaseNotIssued = false;
                    result.receipt = dispatch(Intent::Guard);
                    if (result.receipt.outcome == Dispatch::Rejected)
                        state.phase = Phase::Idle;
                    else if (result.receipt.outcome == Dispatch::Unconfirmed)
                        state.phase = Phase::Fault;
                    break;
                case Step::Acknowledge:
                    state.phase = Phase::Holding;
                    state.elapsed = 0;
                    break;
                case Step::Release:
                    result.missedStart = state.phase == Phase::StartPending;
                    state.missedStart |= result.missedStart;
                    state.phase = Phase::ReleasePending;
                    state.elapsed = state.releaseWait = 0;
                    state.releaseDeferred = false;
                    result.receipt = dispatch(Intent::ReleaseGuard);
                    state.releaseRejected = result.receipt.outcome == Dispatch::Rejected;
                    state.releaseCompleted = result.receipt.outcome != Dispatch::Unconfirmed;
                    if (!state.releaseCompleted)
                        state.phase = Phase::Fault;
                    break;
                case Step::Finished:
                    result.cleanupConfirmed = state.phase == Phase::Holding ||
                                              (state.phase == Phase::ReleasePending && state.releaseCompleted);
                    result.releaseObserved = state.phase == Phase::ReleasePending && !state.releaseRejected;
                    state.phase = Phase::Idle;
                    state.elapsed = 0;
                    break;
                case Step::Fault:
                    result.timeout = state.phase != Phase::Fault;
                    state.releaseNotIssued = reaction.beforeRelease;
                    state.phase = Phase::Fault;
                    break;
                case Step::None:
                    break;
            }
            return result;
        }

        void YieldOwnership() {
            state.phase = guard_rules::Phase::Idle;
        }

        void Fail() {
            state.phase = guard_rules::Phase::Fault;
        }

        void ActorUnavailable() {
            if (state.phase != guard_rules::Phase::Idle)
                Fail();
        }

        bool TakeMissedStart() {
            return std::exchange(state.missedStart, false);
        }

        guard_rules::Recovery Restore(guard_rules::Phase saved, bool stillBlocking) {
            const auto result = guard_rules::RestoreGuard(saved, stillBlocking);
            state = {};
            if (result != guard_rules::Recovery::Finished)
                state.phase = result == guard_rules::Recovery::Manual ? guard_rules::Phase::Fault
                                                                      : guard_rules::Phase::ReleasePending;
            return result;
        }

        bool Reconcile(const GuardSample& sample) {
            if (state.phase != guard_rules::Phase::Fault ||
                !guard_rules::ReconcileRelease(state.releaseCompleted || state.releaseNotIssued, sample.blockReadable,
                                               sample.blocking, sample.IntentActive()))
                return false;
            state.phase = guard_rules::Phase::Idle;
            return true;
        }

    private:
        GuardState state;
    };
}
