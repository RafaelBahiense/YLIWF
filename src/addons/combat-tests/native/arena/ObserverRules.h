#pragma once
#include <algorithm>
#include <cstdint>
#include <span>

namespace mod::combat_tests::observer_rules {
    constexpr bool Reject(std::uint32_t attacker, std::uint32_t target, std::uint32_t observer,
                          std::span<const std::uint32_t> opponents, std::span<const std::uint32_t> allies = {}) {
        if (!attacker || !target)
            return false;
        const bool friendly = std::ranges::find(allies, attacker) != allies.end();
        const bool hostile = std::ranges::find(opponents, attacker) != opponents.end();
        return (observer && target == observer && (friendly || hostile)) ||
               (friendly && std::ranges::find(allies, target) != allies.end()) ||
               (hostile && std::ranges::find(opponents, target) != opponents.end());
    }
}
