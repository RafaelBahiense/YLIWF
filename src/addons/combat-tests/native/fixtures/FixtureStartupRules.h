#pragma once
#include "FixtureResourceRules.h"
#include <cstdint>

namespace mod::combat_tests::fixture_startup {
    enum class Result { Waiting, Ready, Expired };

    class Settling {
    public:
        Result Observe(float delta, const resources::Stats& primary, const resources::Stats& auxiliary,
                       std::uint64_t equipment, float minimum) {
            if (!std::isfinite(delta) || delta <= 0 || delta > 1) {
                quiet_ = 0;
                sampled_ = false;
                return Result::Waiting;
            }
            elapsed_ += delta;
            const bool same = sampled_ && equipment_ == equipment && resources::Stable(primary_, primary) &&
                              resources::Stable(auxiliary_, auxiliary);
            quiet_ = same ? quiet_ + delta : 0;
            primary_ = primary;
            auxiliary_ = auxiliary;
            equipment_ = equipment;
            sampled_ = true;
            if (elapsed_ >= minimum && quiet_ >= 1)
                return Result::Ready;
            return elapsed_ >= 20 ? Result::Expired : Result::Waiting;
        }

    private:
        resources::Stats primary_{}, auxiliary_{};
        std::uint64_t equipment_ = 0;
        float elapsed_ = 0, quiet_ = 0;
        bool sampled_ = false;
    };
}
