#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace mod::executor_rules {
    // Game-time deadlines belong to native state. No Papyrus registration is involved.
    inline bool SetDeadline(std::span<float> deadlines, std::int32_t alias, std::optional<float> hoursPassed) {
        if (deadlines.size() != 9 || alias < 0 || static_cast<std::size_t>(alias) >= deadlines.size())
            return false;
        if (hoursPassed && (!std::isfinite(*hoursPassed) || *hoursPassed < 0))
            return false;
        const float deadline = hoursPassed ? *hoursPassed + 72.0f : 0;
        if (!std::isfinite(deadline))
            return false;
        deadlines[alias] = deadline;
        return true;
    }

    inline bool SwapPrimaryDeadlines(std::span<float> deadlines, std::int32_t extra, bool previousPrimary) {
        if (deadlines.size() != 9 || extra < 2 || extra > 8)
            return false;
        const auto previous = previousPrimary ? deadlines[0] : 0;
        deadlines[0] = deadlines[extra];
        deadlines[extra] = previous;
        return true;
    }
    enum class Phase : std::int32_t { Ready = 0, Engine = 1, Delay = 2, Counts = 3 };

    inline bool KnownPhase(Phase phase) {
        return phase >= Phase::Ready && phase <= Phase::Counts;
    }

    inline bool CanAcknowledge(bool active, std::int32_t actualTicket, std::int32_t actualOffset, Phase phase,
                               std::int32_t ticket, std::int32_t offset) {
        return active && ticket > 0 && actualTicket == ticket && actualOffset == offset &&
               (phase == Phase::Engine || phase == Phase::Counts);
    }

    inline bool ValidDelay(float remaining) {
        return std::isfinite(remaining) && remaining >= 0 && remaining <= 2;
    }

    inline float Remaining(float remaining, float elapsed, bool wasPaused, bool paused) {
        return std::max(0.0f,
                        remaining - (!wasPaused && !paused && std::isfinite(elapsed) ? std::max(0.0f, elapsed) : 0));
    }
}
