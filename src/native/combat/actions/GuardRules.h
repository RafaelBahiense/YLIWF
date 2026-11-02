#pragma once
#include "DispatchRules.h"
#include <cmath>
#include <cstdint>

namespace mod::combat_actions::guard_rules {
    enum class Phase : std::uint32_t { Idle, StartPending, Holding, ReleasePending, Fault };
    enum class Step { None, Start, Acknowledge, Release, Finished, Fault };
    enum class Recovery { Finished, ObserveRelease, Manual };

    inline constexpr float AcknowledgeTimeout = 0.75f;
    inline constexpr float ReleaseTimeout = 1.0f;
    inline constexpr float BlockReactionTimeout = 3.0f;

    constexpr bool ReleaseCleared(bool graphReadable, bool blocking, bool wantsBlock) {
        return graphReadable && !blocking && !wantsBlock;
    }

    constexpr bool BlockIntent(bool actorIntent, bool graphIntentReadable, std::int32_t graphIntent) {
        return actorIntent || (graphIntentReadable && graphIntent != 0);
    }

    constexpr bool ReconcileRelease(bool completed, bool graphReadable, bool blocking, bool wantsBlock) {
        return completed && ReleaseCleared(graphReadable, blocking, wantsBlock);
    }

    constexpr bool ReleaseReactionActive(Phase phase, bool completed, bool rejected, bool graphReadable, bool blocking,
                                         bool wantsBlock, bool reacting) {
        return phase == Phase::ReleasePending && completed && !rejected && graphReadable && blocking && !wantsBlock &&
               reacting;
    }

    constexpr bool ActiveDodge(bool readable, bool active) {
        return readable && active;
    }

    constexpr bool GuardInterrupted(Phase phase, bool yielding) {
        return yielding && (phase == Phase::StartPending || phase == Phase::Holding);
    }

    constexpr bool ResumeAfterYield(Phase phase, bool interrupted, bool cleanupCompleted, bool stateReadable,
                                    bool yielding, bool graphReadable, bool blocking, bool wantsBlock) {
        return phase == Phase::Idle && interrupted && cleanupCompleted && stateReadable && !yielding &&
               ReleaseCleared(graphReadable, blocking, wantsBlock);
    }

    constexpr Recovery RestoreGuard(Phase phase, bool blocking) {
        if (phase == Phase::Idle || (phase == Phase::Holding && !blocking))
            return Recovery::Finished;
        if (phase == Phase::ReleasePending)
            return Recovery::ObserveRelease;
        return Recovery::Manual;
    }

    inline Step Decide(Phase phase, bool wanted, bool canStart, bool blocking, bool armed, float elapsed,
                       bool graphReadable = true, bool wantsBlock = false, bool dodging = false,
                       bool blockReaction = false, float releaseWait = 0) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return Step::Fault;
        switch (phase) {
            case Phase::Idle:
                return wanted && canStart && graphReadable && !wantsBlock && !blocking && armed && !dodging
                           ? Step::Start
                           : Step::None;
            case Phase::StartPending:
                if (graphReadable && blocking)
                    return Step::Acknowledge;
                return elapsed >= AcknowledgeTimeout ? Step::Release : Step::None;
            case Phase::Holding:
                if (ReleaseCleared(graphReadable, blocking, wantsBlock))
                    return Step::Finished;
                if ((!wanted || dodging) && blockReaction) {
                    if (!std::isfinite(releaseWait) || releaseWait < 0 || releaseWait >= BlockReactionTimeout)
                        return Step::Fault;
                    return Step::None;
                }
                return wanted && !dodging ? Step::None : Step::Release;
            case Phase::ReleasePending:
                if (ReleaseCleared(graphReadable, blocking, wantsBlock) && elapsed >= 0.1f)
                    return Step::Finished;
                if (!std::isfinite(releaseWait) || releaseWait < 0 || releaseWait > elapsed ||
                    releaseWait >= BlockReactionTimeout || elapsed >= ReleaseTimeout + BlockReactionTimeout)
                    return Step::Fault;
                return elapsed - releaseWait >= ReleaseTimeout ? Step::Fault : Step::None;
            case Phase::Fault:
                return Step::Fault;
        }
        return Step::Fault;
    }
}
