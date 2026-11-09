#pragma once
#include <array>
#include <cstdint>
#include <cmath>

namespace mod::combat_tests::spell_diagnostic_rules {
    struct Case {
        bool control, invulnerable;
    };

    inline constexpr std::array Cases{Case{true, true}, Case{false, true}, Case{true, false}, Case{false, false}};
    inline constexpr float Settle = .5f, Observe = 2, LoadTimeout = 10;
    enum class Arrival { Wait, Ready, Failed };

    inline Arrival ObserveArrival(bool sameCell, bool loaded, float elapsed) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return Arrival::Failed;
        if (sameCell && loaded)
            return Arrival::Ready;
        return elapsed >= 15 ? Arrival::Failed : Arrival::Wait;
    }
    enum class Cleanup { Wait, Complete, Failed };

    inline Cleanup ObserveCleanup(bool clear, float elapsed) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return Cleanup::Failed;
        return clear ? Cleanup::Complete : elapsed >= 1 ? Cleanup::Failed : Cleanup::Wait;
    }

    struct Evidence {
        bool dispatched = false, hasEffect = false, spellEntry = false, baseEntry = false, strictEntry = false;
    };

    constexpr bool Applied(const Evidence& value) {
        return value.hasEffect || value.spellEntry || value.baseEntry;
    }

    constexpr const char* Finding(const std::array<Evidence, 4>& values) {
        for (const auto& value : values)
            if (!value.dispatched)
                return "Incomplete dispatch coverage; no immunity conclusion.";
        if (Applied(values[1]) && !values[1].strictEntry)
            return "Challenge application observed with invulnerability on, but the production detector missed it.";
        if (Applied(values[3]) && !values[3].strictEntry)
            return "Challenge application observed with invulnerability off, but the production detector missed it.";
        if (!Applied(values[0]) || !Applied(values[2]))
            return "Control application missing in at least one condition; casting/fixture setup is not validated.";
        if (!Applied(values[1]) && Applied(values[3]))
            return "Challenge application depends on invulnerability in this isolated comparison.";
        if (!Applied(values[1]) && !Applied(values[3]))
            return "Control applies, but Challenge is absent with invulnerability both on and off; inspect Challenge "
                   "configuration/dispatch.";
        if (Applied(values[1]) && Applied(values[3]))
            return "Challenge applies and is detected in both conditions; investigate combat-specific eligibility and "
                   "target selection next.";
        return "Challenge only applies with invulnerability on; repeat before attributing a cause.";
    }
}
