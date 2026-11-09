#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace yliwf::testing {
    struct DefenderTuning {
        std::uint32_t size = sizeof(DefenderTuning), revision = 0;
        char name[32] = "Baseline";
        float approachStep = 384, retreatStep = 256, lateralStep = 256;
        float movementBurst = 2, movementRest = .6f, movementRefresh = .35f;
        float recoveryEnter = .4f, recoveryExit = .65f;
        float guardSpeed = 180, guardSettle = .2f;
        std::uint32_t challengeEnabled = 1;
    };

    struct TuningField {
        const char* key;
        const char* label;
        float DefenderTuning::* member;
        float minimum, maximum;
    };

    inline constexpr std::array TuningFields{
        TuningField{"approach_step", "Approach step (units)", &DefenderTuning::approachStep, 128, 512},
        TuningField{"retreat_step", "Retreat step (units)", &DefenderTuning::retreatStep, 128, 512},
        TuningField{"lateral_step", "Lateral step (units)", &DefenderTuning::lateralStep, 128, 512},
        TuningField{"movement_burst", "Movement burst (seconds)", &DefenderTuning::movementBurst, .25f, 5},
        TuningField{"movement_rest", "Movement rest (seconds)", &DefenderTuning::movementRest, 0, 2},
        TuningField{"movement_refresh", "Movement refresh (seconds)", &DefenderTuning::movementRefresh, .1f, 1},
        TuningField{"recovery_enter", "Recovery enter (fraction)", &DefenderTuning::recoveryEnter, .05f, .8f},
        TuningField{"recovery_exit", "Recovery exit (fraction)", &DefenderTuning::recoveryExit, .1f, 1},
        TuningField{"guard_speed", "Guard maximum speed (units/second)", &DefenderTuning::guardSpeed, 60, 250},
        TuningField{"guard_settle", "Guard settling (seconds)", &DefenderTuning::guardSettle, .05f, .75f}};

    inline const char* TuningError(const DefenderTuning& value) {
        if (value.size != sizeof(value))
            return "Profile structure size mismatch.";
        if (value.challengeEnabled > 1)
            return "Challenge must be enabled or disabled.";
        std::size_t length = 0;
        for (; length < sizeof(value.name) && value.name[length]; ++length) {
            const char c = value.name[length];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
                  c == '_' || c == '.'))
                return "Use letters, numbers, spaces, hyphens, underscores or periods in the profile name.";
        }
        if (!length || length == sizeof(value.name))
            return "Profile name must contain 1 to 31 characters.";
        if (value.name[0] == ' ' || value.name[length - 1] == ' ')
            return "Profile name must not start or end with a space.";
        for (const auto& field : TuningFields) {
            const float number = value.*field.member;
            if (!std::isfinite(number) || number < field.minimum || number > field.maximum)
                return field.label;
        }
        if (value.recoveryExit - value.recoveryEnter < .05f - .000001f)
            return "Recovery exit must exceed entry by at least 0.05.";
        if (value.movementRefresh > value.movementBurst)
            return "Movement refresh must not exceed the movement burst.";
        if (value.guardSettle > value.movementBurst)
            return "Guard settling must not exceed the movement burst.";
        return nullptr;
    }

    static_assert(sizeof(DefenderTuning) == 84);
}
