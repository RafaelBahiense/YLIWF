#pragma once

#include <cstdint>
#include "actions/DispatchRules.h"
#include "actions/GuardRules.h"
#include <algorithm>
#include <cmath>

namespace mod::defender_action_rules {
    inline bool GuardLocomotionReady(float speed, bool sprinting, bool weaponDrawn) {
        return std::isfinite(speed) && speed >= 0 && speed <= 180 && !sprinting && weaponDrawn;
    }

    class MovementHandoff {
    public:
        void Update(float elapsed, bool live, bool direct, bool wanted, bool armed, bool cooldownReady,
                    combat_actions::guard_rules::Phase phase, bool moving) {
            using combat_actions::guard_rules::Phase;
            preparing_ = live && !direct &&
                         ((wanted && armed && cooldownReady && phase == Phase::Idle) || phase == Phase::StartPending ||
                          phase == Phase::ReleasePending);
            if (!preparing_)
                settleRequired_ = false;
            else if (moving || wasMoving_)
                settleRequired_ = true;
            settled_ = preparing_ && !moving ? settled_ + std::clamp(elapsed, 0.0f, .25f) : 0;
            ready_ = direct || (preparing_ && !moving && (!settleRequired_ || settled_ >= .2f));
            wasMoving_ = live && moving;
        }

        bool Preparing() const {
            return preparing_;
        }

        bool Ready() const {
            return ready_;
        }

    private:
        bool preparing_ = false, ready_ = false, settleRequired_ = false, wasMoving_ = false;
        float settled_ = 0;
    };

    enum class Mode : std::uint32_t { Styles, RecoveryGate, Guard, Combined };

    constexpr bool ValidMode(std::uint32_t value) {
        return value <= 3;
    }

    constexpr bool Gate(Mode mode) {
        return mode == Mode::RecoveryGate || mode == Mode::Combined;
    }

    constexpr bool Guard(Mode mode) {
        return mode == Mode::Guard || mode == Mode::Combined;
    }

    constexpr bool Suppress(Mode mode, bool live, bool recovering, bool guarding, bool offensiveRequest,
                            bool dodging = false) {
        return live && !dodging && Gate(mode) && offensiveRequest && (recovering || (Guard(mode) && guarding));
    }

}
