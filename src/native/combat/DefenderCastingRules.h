#pragma once
#include <cstdint>

namespace mod::defender_casting_rules {
    enum class Activity { Idle, Preparing, Casting };

    // Active casting outranks preparation; an idle equipped spell cannot hide
    // the other hand. Equal activity retains the first candidate deterministically.
    constexpr bool Prefer(Activity candidate, Activity selected) {
        return static_cast<unsigned>(candidate) > static_cast<unsigned>(selected);
    }

    // A live hostile currentSpell is required. Startup/ready states indicate
    // preparation, not projectile flight; equipped spells alone are idle.
    constexpr Activity Observe(bool hostileCurrentSpell, std::uint32_t state) {
        if (!hostileCurrentSpell)
            return Activity::Idle;
        if (state >= 1 && state <= 5)
            return Activity::Preparing;
        return state == 6 ? Activity::Casting : Activity::Idle;
    }
}
