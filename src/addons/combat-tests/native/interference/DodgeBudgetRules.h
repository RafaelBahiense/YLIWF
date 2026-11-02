#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mod::combat_tests::interference::dodge_budget_rules {
    enum class Denial { None, Snapshot, Reserve, Cooldown };

    inline bool Valid(float reserve, float cooldown) {
        return std::isfinite(reserve) && reserve >= 0 && reserve <= 0.75f && std::isfinite(cooldown) && cooldown >= 0 &&
               cooldown <= 10;
    }

    inline Denial Check(std::uint64_t now, std::uint64_t expires, std::uint64_t next, float stamina, float maximum,
                        float cost, float reserve) {
        if (now > expires || !std::isfinite(stamina) || !std::isfinite(maximum) || maximum <= 0 ||
            !std::isfinite(cost) || cost < 0 || !std::isfinite(reserve) || reserve < 0 || reserve > 0.75f)
            return Denial::Snapshot;
        if (stamina < cost + maximum * reserve)
            return Denial::Reserve;
        return now < next ? Denial::Cooldown : Denial::None;
    }
}
