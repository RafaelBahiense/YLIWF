#pragma once

#include "FollowerAdapter.h"
#include <cmath>

namespace mod::adapter_movement_rules {
    using yliwf::sdk::Movement;
    using yliwf::sdk::MovementBit;
    inline constexpr float RefreshSeconds = 0.2f;

    struct Snapshot {
        yliwf::sdk::State state = yliwf::sdk::State::Unavailable;
        std::uint32_t permissions = 0;
        bool pending = false;
        bool available = false;
        bool scene = false;
        bool normalMovement = false;
        bool foreignInterrupt = false;
        bool combat = false;
    };

    inline bool Permitted(const Snapshot& value, Movement feature) {
        using yliwf::sdk::State;
        const bool active = value.state == State::Following || value.state == State::Waiting;
        return active && !(value.permissions & ~yliwf::sdk::AllMovement) &&
               (value.permissions & MovementBit(feature)) && !value.pending && value.available && !value.scene &&
               value.normalMovement && !value.foreignInterrupt &&
               (feature == Movement::StandDown || (value.state == State::Following && !value.combat));
    }

    // A requested higher-priority behavior never silently falls back to travel
    // when its owner withdraws permission.
    inline std::uint32_t Select(const Snapshot& value, bool distance, bool formation, bool standDown) {
        const auto feature = standDown ? Movement::StandDown : formation ? Movement::Formation : Movement::Distance;
        return (standDown || formation || distance) && Permitted(value, feature) ? MovementBit(feature) : 0;
    }

    inline bool RefreshDue(float& seconds, float elapsed) {
        if (std::isfinite(elapsed) && elapsed > 0)
            seconds += elapsed;
        if (seconds < RefreshSeconds)
            return false;
        seconds = std::fmod(seconds, RefreshSeconds);
        return true;
    }
}
