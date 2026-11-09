#pragma once
#include "actions/DispatchRules.h"
#include <cmath>
#include <cstdint>
#include <string_view>

namespace mod::defensive_bash_rules {
    inline constexpr float Cooldown = 5;
    enum class Kind : std::uint32_t { Off, Normal, Power };

    struct Opportunity {
        bool enabled = false, live = false, ownsGuard = false, blocking = false;
        bool interrupted = false, reacting = false, attacking = false, targetWindingUp = false;
        float distance = -1, heading = 180, stamina = 0, maximum = 0, cooldown = 0;
        Kind kind = Kind::Normal;
        bool recovering = false;
    };

    inline bool CanRequest(const Opportunity& value) {
        if (!value.enabled || !value.live || !value.ownsGuard || !value.blocking || value.interrupted ||
            value.recovering || value.reacting || value.attacking || !value.targetWindingUp ||
            !std::isfinite(value.distance) || value.distance < 0 || value.distance > 144 ||
            !std::isfinite(value.heading) || std::abs(value.heading) > 60 || !std::isfinite(value.stamina) ||
            !std::isfinite(value.maximum) || value.maximum <= 0 || !std::isfinite(value.cooldown) || value.cooldown > 0)
            return false;
        // Conservative budget, not a claim about a modded bash's exact cost.
        const float reserve = value.maximum * 0.2f;
        const float budget = value.kind == Kind::Power ? 60.0f : 35.0f;
        return value.kind != Kind::Off && value.stamina >= reserve + budget && value.stamina >= value.maximum * 0.3f;
    }

    inline Kind Select(Opportunity value, bool powerEnabled, bool powerEligible) {
        value.kind = Kind::Power;
        if (powerEnabled && powerEligible && CanRequest(value))
            return Kind::Power;
        value.kind = Kind::Normal;
        return CanRequest(value) ? Kind::Normal : Kind::Off;
    }

    constexpr bool AllowedEvent(std::string_view event, Kind kind) {
        return (kind == Kind::Normal || kind == Kind::Power) &&
               combat_actions::AllowedBashEvent(
                   event, kind == Kind::Power ? combat_actions::Intent::PowerBash : combat_actions::Intent::Bash);
    }

}
