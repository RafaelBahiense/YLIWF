#pragma once
#include "Dispatch.h"
#include "AnimationEvidence.h"
#include "BashRules.h"
#include <utility>

namespace mod::combat_actions {
    struct BashSample {
        bool bashing = false, interrupted = false, idle = false, recovering = false;
    };

    struct BashTransition {
        bash_rules::Step step = bash_rules::Step::None;
        bool graphStart = false, graphEnd = false, nativeStart = false;
    };

    // Main-thread execution state; graph callbacks touch only the keyed mailbox.
    class BashExecution {
    public:
        bash_rules::Phase Phase() const {
            return phase;
        }

        template <class DispatchAction>
        Receipt Start(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence, Intent intent,
                      DispatchAction&& dispatch) {
            if (phase != bash_rules::Phase::Idle || !actor || !token || !sequence ||
                (intent != Intent::Bash && intent != Intent::PowerBash))
                return {};
            owner = actor;
            generation = token;
            request = sequence;
            graphStartCounted = graphEndCounted = nativeStartCounted = false;
            animation.Arm(actor, token, sequence);
            auto receipt = dispatch(intent);
            if (receipt.outcome == Dispatch::Rejected)
                animation.Clear();
            else {
                elapsed = 0;
                idleSamples = 0;
                phase = receipt.outcome == Dispatch::Accepted ? bash_rules::Phase::Pending : bash_rules::Phase::Fault;
            }
            return receipt;
        }

        BashTransition Observe(const BashSample& sample, float delta) {
            using namespace bash_rules;
            BashTransition result;
            if (phase == bash_rules::Phase::Idle)
                return result;
            elapsed += delta;
            if (sample.bashing)
                animation.NativeStart(owner, generation, request);
            if (sample.interrupted)
                animation.Record(owner, generation, request, AnimationEvent::Interrupted);
            const auto evidence = animation.Read(owner, generation, request);
            result.graphStart = evidence.graphStarted && !std::exchange(graphStartCounted, true);
            result.graphEnd = evidence.ended && !std::exchange(graphEndCounted, true);
            result.nativeStart = evidence.nativeStarted && !std::exchange(nativeStartCounted, true);
            const bool completionReady = !evidence.graphStarted || evidence.ended;
            idleSamples = sample.idle && !sample.recovering && completionReady ? idleSamples + 1 : 0;
            result.step = bash_rules::Observe(phase, elapsed, sample.bashing, sample.idle, sample.recovering,
                                              idleSamples, evidence);
            switch (result.step) {
                case Step::Observed:
                    phase = bash_rules::Phase::Active;
                    elapsed = 0;
                    idleSamples = 0;
                    break;
                case Step::Finished:
                case Step::Interrupted:
                    phase = bash_rules::Phase::Idle;
                    animation.Clear();
                    break;
                case Step::Fault:
                    phase = bash_rules::Phase::Fault;
                    break;
                case Step::None:
                    break;
            }
            return result;
        }

        void Teardown() {
            if (phase != bash_rules::Phase::Idle)
                animation.Teardown(owner, generation, request);
        }

        void Reset() {
            animation.Clear();
            phase = bash_rules::Phase::Idle;
            owner = generation = request = idleSamples = 0;
            elapsed = 0;
            graphStartCounted = graphEndCounted = nativeStartCounted = false;
        }

        std::uint32_t Request(std::uint32_t actor, std::uint32_t token) const {
            return animation.Request(actor, token);
        }

        bool Record(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence, AnimationEvent event) {
            return animation.Record(actor, token, sequence, event);
        }

    private:
        AnimationEvidenceMailbox animation;
        bash_rules::Phase phase = bash_rules::Phase::Idle;
        std::uint32_t owner = 0, generation = 0, request = 0;
        float elapsed = 0;
        unsigned idleSamples = 0;
        bool graphStartCounted = false, graphEndCounted = false, nativeStartCounted = false;
    };
}
