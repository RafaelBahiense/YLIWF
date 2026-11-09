#pragma once
#include <DefenderTuning.h>

namespace mod::defender_tuning {
    // Main-thread test configuration, separate from engine/action ownership.
    class Scope {
    public:
        void Begin(std::uint32_t actor, std::uint32_t token) {
            actor_ = actor;
            token_ = token;
            configured_ = false;
            value_ = {};
        }

        bool Configure(std::uint32_t actor, std::uint32_t token, const yliwf::testing::DefenderTuning& value) {
            if (!actor || !token || actor_ != actor || token_ != token || configured_ ||
                yliwf::testing::TuningError(value))
                return false;
            value_ = value;
            configured_ = true;
            return true;
        }

        yliwf::testing::DefenderTuning Read(std::uint32_t actor) const {
            return actor && actor_ == actor ? value_ : yliwf::testing::DefenderTuning{};
        }

        void End(std::uint32_t actor, std::uint32_t token) {
            if (actor_ == actor && token_ == token)
                *this = {};
        }

    private:
        std::uint32_t actor_ = 0, token_ = 0;
        bool configured_ = false;
        yliwf::testing::DefenderTuning value_;
    };

    extern Scope TestScope;
}
