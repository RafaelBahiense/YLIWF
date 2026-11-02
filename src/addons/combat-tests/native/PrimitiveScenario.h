#pragma once
#include "CombatTestHost.h"

namespace mod::combat_tests::primitive {
    enum class Kind { GuardRelease, Bash, PowerBash, RightAttack };
    enum class Step { Wait, Guard, Release, Bash, Attack, Passed, Inconclusive, Failed };

    // Scenario policy only: each transition emits at most one host request.
    class Scenario {
    public:
        Step Advance(float delta, Kind kind, const yliwf::testing::DefenderObservation& evidence) {
            if (finished)
                return result;
            elapsed += delta;
            if (evidence.actionFault || evidence.actionTimeouts || evidence.nativeBashTimeouts ||
                evidence.rightAttackTimeouts)
                return Finish(Step::Failed, "Primitive acknowledgement or cleanup unresolved; no retry.");
            switch (stage) {
                case Stage::Start:
                    stage = kind == Kind::RightAttack ? Stage::Complete : Stage::Acquire;
                    elapsed = 0;
                    return kind == Kind::RightAttack ? Step::Attack : Step::Guard;
                case Stage::Acquire:
                    if (evidence.guardRejected)
                        return Finish(Step::Inconclusive, "Guard rejected by native eligibility or idle selection.");
                    if (evidence.guardObserved && evidence.guardPhase == 2) {
                        stage = Stage::Hold;
                        elapsed = 0;
                    } else if (elapsed >= 8)
                        return Finish(Step::Inconclusive, "No owned guard observed within the scenario window.");
                    break;
                case Stage::Hold:
                    if (evidence.guardPhase != 2)
                        return Finish(Step::Inconclusive, "Guard ownership changed before the follow-up request.");
                    if (elapsed >= 1) {
                        stage = Stage::Complete;
                        elapsed = 0;
                        return kind == Kind::GuardRelease ? Step::Release : Step::Bash;
                    }
                    break;
                case Stage::Complete:
                    if (kind == Kind::RightAttack) {
                        if (evidence.rightAttackRejected)
                            return Finish(Step::Inconclusive,
                                          "Normal right-hand selection rejected; no replacement request.");
                        if (evidence.rightAttackInterrupted)
                            return Finish(Step::Inconclusive,
                                          "Normal attack interrupted or superseded; cleanup confirmed.");
                        if (evidence.rightAttackCompleted)
                            return Finish(Step::Passed,
                                          "Owned right-hand normal attack started and completed; cleanup confirmed.");
                    } else if (kind == Kind::GuardRelease) {
                        if (evidence.releaseRejected)
                            return Finish(Step::Inconclusive, "Release rejected; cleanup is observed without replay.");
                        if (evidence.releaseObserved && evidence.guardPhase == 0)
                            return Finish(Step::Passed, "Owned guard and release acknowledged; cleanup confirmed.");
                    } else {
                        if (evidence.nativeBashRejected)
                            return Finish(Step::Inconclusive, "Bash eligibility rejected; no replacement request.");
                        if (evidence.nativeBashInterrupted)
                            return Finish(Step::Inconclusive, "Bash interrupted; cleanup confirmed.");
                        if (evidence.nativeBashCompleted)
                            return Finish(Step::Passed, "Owned bash started and completed; cleanup confirmed.");
                    }
                    if (elapsed >= 8)
                        return Finish(Step::Inconclusive, "Follow-up did not finish within the scenario window.");
                    break;
            }
            return Step::Wait;
        }

        Step Result() const {
            return result;
        }

        const char* Reason() const {
            return reason;
        }

        void Reject() {
            Finish(Step::Inconclusive, "Host rejected the explicit request before queueing; no retry.");
        }

    private:
        enum class Stage { Start, Acquire, Hold, Complete };

        Step Finish(Step value, const char* text) {
            finished = true;
            result = value;
            reason = text;
            return value;
        }

        Stage stage = Stage::Start;
        float elapsed = 0;
        bool finished = false;
        Step result = Step::Wait;
        const char* reason = "Scenario has not completed.";
    };
}
