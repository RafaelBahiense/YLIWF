#pragma once
#include "Dispatch.h"
#include "AnimationEvidence.h"
#include <cmath>
#include <utility>

namespace mod::combat_actions {
    namespace attack_rules {
        enum class Phase : std::uint32_t { Idle, Pending, Active, Fault };
        enum class Step { None, Observed, Finished, Interrupted, Fault };
        inline constexpr float StartTimeout = 0.75f, CompletionTimeout = 5;
    }

    struct AttackSample {
        bool normalMelee = false, conflicting = false, interrupted = false, idle = false;
        bool recoveryReadable = false, recovering = false;
    };

    enum class AttackEvent { Started, Ended, Interrupted, ConflictingStarted, ConflictingEnded };

    struct AttackTransition {
        attack_rules::Step step = attack_rules::Step::None;
        bool graphStart = false, graphEnd = false, nativeStart = false, recoveryCompletion = false;
    };

    class RightAttackExecution {
    public:
        attack_rules::Phase Phase() const {
            return phase;
        }

        template <class DispatchAction>
        Receipt Start(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence, DispatchAction&& dispatch) {
            if (phase != attack_rules::Phase::Idle || !actor || !token || !sequence)
                return {};
            owner = actor;
            generation = token;
            request = sequence;
            graphStartCounted = graphEndCounted = nativeStartCounted = recoverySeen = recoveryClearSeen = false;
            elapsed = 0;
            idleSamples = 0;
            animation.Arm(actor, token, sequence);
            auto receipt = dispatch(Intent::RightAttack);
            if (receipt.outcome == Dispatch::Rejected)
                animation.Clear();
            else
                phase =
                    receipt.outcome == Dispatch::Accepted ? attack_rules::Phase::Pending : attack_rules::Phase::Fault;
            return receipt;
        }

        AttackTransition Observe(const AttackSample& sample, float delta) {
            using namespace attack_rules;
            AttackTransition result;
            if (phase == attack_rules::Phase::Idle)
                return result;
            if (!std::isfinite(delta) || delta < 0 || phase == attack_rules::Phase::Fault)
                return Fault(result);
            elapsed += delta;
            if (sample.normalMelee)
                animation.NativeStart(owner, generation, request);
            if (sample.conflicting)
                animation.Record(owner, generation, request, AnimationEvent::Conflicting);
            if (sample.interrupted)
                animation.Teardown(owner, generation, request);
            const auto evidence = animation.Read(owner, generation, request);
            result.graphStart = evidence.graphStarted && !std::exchange(graphStartCounted, true);
            result.graphEnd = evidence.ended && !std::exchange(graphEndCounted, true);
            result.nativeStart = evidence.nativeStarted && !std::exchange(nativeStartCounted, true);
            if ((evidence.graphStarted || evidence.nativeStarted) && sample.recoveryReadable) {
                if (!sample.recovering)
                    recoveryClearSeen = true;
                else if (recoveryClearSeen)
                    recoverySeen = true;
            }

            const bool fallback = !evidence.graphStarted && !evidence.conflicting && evidence.nativeStarted &&
                                  recoverySeen && sample.recoveryReadable;
            const bool endKnown = evidence.ended || fallback;
            idleSamples =
                sample.idle && !(sample.recoveryReadable && sample.recovering) && endKnown ? idleSamples + 1 : 0;
            if (phase == attack_rules::Phase::Pending && !evidence.conflicting &&
                (evidence.graphStarted || evidence.nativeStarted)) {
                phase = attack_rules::Phase::Active;
                elapsed = 0;
                idleSamples = 0;
                result.step = Step::Observed;
                return result;
            }
            if (idleSamples >= 2 && (phase == attack_rules::Phase::Active || evidence.conflicting)) {
                result.step = evidence.interrupted ? Step::Interrupted : Step::Finished;
                result.recoveryCompletion = !evidence.ended && fallback;
                phase = attack_rules::Phase::Idle;
                animation.Clear();
                return result;
            }
            const float bound =
                phase == attack_rules::Phase::Pending && !evidence.conflicting ? StartTimeout : CompletionTimeout;
            return elapsed >= bound ? Fault(result) : result;
        }

        void Teardown() {
            if (phase != attack_rules::Phase::Idle)
                animation.Teardown(owner, generation, request);
        }

        void Reset() {
            animation.Clear();
            phase = attack_rules::Phase::Idle;
            owner = generation = request = idleSamples = 0;
            elapsed = 0;
            graphStartCounted = graphEndCounted = nativeStartCounted = recoverySeen = recoveryClearSeen = false;
        }

        std::uint32_t Request(std::uint32_t actor, std::uint32_t token) const {
            return animation.Request(actor, token);
        }

        bool Record(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence, AttackEvent event) {
            switch (event) {
                case AttackEvent::Started:
                    return animation.Record(actor, token, sequence, AnimationEvent::Started);
                case AttackEvent::Ended:
                    return animation.Record(actor, token, sequence, AnimationEvent::Ended);
                case AttackEvent::Interrupted:
                    return animation.Record(actor, token, sequence, AnimationEvent::Interrupted);
                case AttackEvent::ConflictingStarted:
                    return animation.Record(actor, token, sequence, AnimationEvent::Conflicting);
                case AttackEvent::ConflictingEnded:
                    return animation.Read(actor, token, sequence).conflicting &&
                           animation.Record(actor, token, sequence, AnimationEvent::Ended);
            }
            return false;
        }

    private:
        AttackTransition Fault(AttackTransition result) {
            phase = attack_rules::Phase::Fault;
            result.step = attack_rules::Step::Fault;
            return result;
        }

        AnimationEvidenceMailbox animation;
        attack_rules::Phase phase = attack_rules::Phase::Idle;
        std::uint32_t owner = 0, generation = 0, request = 0;
        float elapsed = 0;
        unsigned idleSamples = 0;
        bool graphStartCounted = false, graphEndCounted = false, nativeStartCounted = false, recoverySeen = false,
             recoveryClearSeen = false;
    };
}
