#pragma once

#include "formations/FormationPath.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>

namespace mod::formation_rules {
    enum class RecoveryReason { None, Stalled, TooFar };

    constexpr std::string_view RecoveryName(RecoveryReason reason) {
        switch (reason) {
            case RecoveryReason::None:
                return "none";
            case RecoveryReason::Stalled:
                return "stalled";
            case RecoveryReason::TooFar:
                return "too far";
        }
        return "unknown";
    }

    class Progress {
    public:
        static constexpr float ArrivalRadius = 160;
        static constexpr float MotionThreshold = 64;
        static constexpr float StallSeconds = 6;
        static constexpr float MaximumDistance = 2048;
        static constexpr float RetrySeconds = 10;
        static constexpr float RejoinDistance = 1024;

        bool Observe(Point actor, Point target, Point leader, float elapsed) {
            // A long frame or loading hitch is not evidence of six seconds of failed AI.
            elapsed = std::clamp(elapsed, 0.0f, 1.0f);
            const float distance = Distance3D(actor, target);
            const float leaderDistance = Distance3D(actor, leader);
            if (reason != RecoveryReason::None) {
                retryAfter = std::max(0.0f, retryAfter - elapsed);
                if (retryAfter > 0 || (distance > RejoinDistance && leaderDistance > RejoinDistance))
                    return false;
                reason = RecoveryReason::None;
                Suspend();
            }
            // A rear slot can be several thousand units behind the player.
            // An actor near the player must still be allowed to move back into it.
            if (distance > MaximumDistance && leaderDistance > MaximumDistance)
                return Recover(RecoveryReason::TooFar);
            if (distance <= ArrivalRadius || !checkpoint || Distance3D(actor, *checkpoint) >= MotionThreshold) {
                checkpoint = actor;
                stalledSeconds = 0;
                return true;
            }
            stalledSeconds += elapsed;
            if (stalledSeconds >= StallSeconds)
                return Recover(RecoveryReason::Stalled);
            return true;
        }

        // Preserve recovery while suspending progress measurements for scenes,
        // combat, unloaded actors or unavailable route history.
        void Suspend() {
            checkpoint.reset();
            stalledSeconds = 0;
        }

        RecoveryReason Reason() const {
            return reason;
        }

        float RetryAfter() const {
            return retryAfter;
        }

        float StalledSeconds() const {
            return stalledSeconds;
        }

    private:
        static float Distance3D(Point a, Point b) {
            return std::hypot(Distance(a, b), a.z - b.z);
        }

        bool Recover(RecoveryReason value) {
            reason = value;
            retryAfter = RetrySeconds;
            Suspend();
            return false;
        }

        std::optional<Point> checkpoint;
        RecoveryReason reason = RecoveryReason::None;
        float stalledSeconds = 0;
        float retryAfter = 0;
    };
}
