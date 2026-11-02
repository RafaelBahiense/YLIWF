#pragma once
#include "AnimationEvidence.h"
#include <cmath>
#include <cstdint>

namespace mod::combat_actions::bash_rules {
    inline constexpr float StartTimeout = 0.75f, CompletionTimeout = 5;
    enum class Phase : std::uint32_t { Idle, Pending, Active, Fault };
    enum class Step { None, Observed, Finished, Interrupted, Fault };

    using combat_actions::AnimationEvidence;

    inline Step Observe(Phase phase, float elapsed, bool bashing, bool idle, bool recovering, unsigned idleSamples,
                        const AnimationEvidence& animation = {}) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return Step::Fault;
        if (phase == Phase::Pending)
            return bashing || animation.graphStarted ? Step::Observed
                   : elapsed >= StartTimeout         ? Step::Fault
                                                     : Step::None;
        if (phase == Phase::Active) {
            if (idle && !recovering && idleSamples >= 2 && (!animation.graphStarted || animation.ended))
                return animation.interrupted ? Step::Interrupted : Step::Finished;
            return elapsed >= CompletionTimeout ? Step::Fault : Step::None;
        }
        return phase == Phase::Fault ? Step::Fault : Step::None;
    }
}
