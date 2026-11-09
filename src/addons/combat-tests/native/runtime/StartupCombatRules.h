#pragma once
#include <cstdint>

namespace mod::combat_tests::startup_combat {
    struct Snapshot {
        std::uint32_t subject = 0, opponent = 0, auxiliary = 0, observer = 0;
        std::uint32_t subjectTarget = 0, opponentTarget = 0, auxiliaryTarget = 0;
        bool allyScenario = false, allyReady = false, natural = false;
        bool subjectHostile = false, opponentHostileToSubject = false, opponentHostileToAlly = false;
    };

    enum class State { Waiting, Ready, UnsafeTarget };

    constexpr State Observe(const Snapshot& state) {
        if (!state.subject || !state.opponent || state.subject == state.opponent)
            return State::Waiting;
        // A temporary lack of target is normal during combat startup. A target
        // outside the fixture is not permission to measure a different fight.
        const auto foreign = [](std::uint32_t target, std::uint32_t first, std::uint32_t second = 0) {
            return target && target != first && target != second;
        };
        if ((state.observer && (state.subjectTarget == state.observer || state.opponentTarget == state.observer ||
                                state.auxiliaryTarget == state.observer)) ||
            foreign(state.subjectTarget, state.opponent, state.allyScenario ? 0 : state.auxiliary) ||
            foreign(state.opponentTarget, state.subject, state.allyScenario ? state.auxiliary : 0) ||
            (state.auxiliary && foreign(state.auxiliaryTarget, state.allyScenario ? state.opponent : state.subject)))
            return State::UnsafeTarget;
        if (!state.natural && !state.subjectHostile)
            return State::Waiting;
        if (state.allyScenario) {
            if (!state.auxiliary || !state.allyReady)
                return State::Waiting;
            if (state.opponentTarget == state.auxiliary)
                return state.opponentHostileToAlly ? State::Ready : State::Waiting;
            // The defender may have intercepted before the measurement starts.
            return state.opponentTarget == state.subject && state.opponentHostileToSubject ? State::Ready
                                                                                           : State::Waiting;
        }
        return state.natural || state.opponentHostileToSubject ? State::Ready : State::Waiting;
    }
}
