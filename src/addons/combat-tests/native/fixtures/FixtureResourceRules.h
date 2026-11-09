#pragma once
#include <array>
#include <cmath>

namespace mod::combat_tests::resources {
    // Maximum health/stamina/magicka, their regeneration rates, then multipliers.
    using Stats = std::array<float, 9>;

    inline bool Stable(const Stats& expected, const Stats& actual) {
        for (std::size_t i = 0; i < expected.size(); ++i)
            if (!std::isfinite(expected[i]) || !std::isfinite(actual[i]) || std::abs(expected[i] - actual[i]) > .1f)
                return false;
        return true;
    }

    inline bool AtTarget(float maximum, float target) {
        return std::isfinite(maximum) && std::isfinite(target) && target > 0 && std::abs(maximum - target) <= .1f;
    }

    inline bool Undamaged(float baseline, float current) {
        return std::isfinite(baseline) && std::isfinite(current) && current >= baseline - .01f;
    }
}
