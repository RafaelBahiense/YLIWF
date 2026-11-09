#pragma once
#include <cstdint>
#include <cmath>

namespace mod::combat_tests::fixture_protection_rules {
    constexpr bool Scoped(std::uint32_t actor, std::uint32_t primary, std::uint32_t secondary) {
        return actor && ((primary && actor == primary) || (secondary && actor == secondary));
    }

    inline bool Suppress(bool scoped, bool damageModifier, bool health, float delta) {
        return scoped && damageModifier && health && std::isfinite(delta) && delta < 0;
    }
}
