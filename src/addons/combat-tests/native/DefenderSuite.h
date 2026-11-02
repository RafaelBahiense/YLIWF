#pragma once
#include <array>
#include <cstdint>

namespace mod::combat_tests::suite {
    enum class Fixture : std::uint32_t {
        Sustained,
        LowStamina,
        RecoveryWaves,
        Approach,
        Reposition,
        Endurance,
        Heavy,
        Archer,
        Projectile,
        Concentration,
        AllyPressure,
        Multiple,
        Area
    };
    enum class Loadout : std::uint32_t { Sword, Heavy, Bow, Projectile, Concentration, Area };

    constexpr std::uint32_t Template(Loadout loadout) {
        return loadout >= Loadout::Projectile ? 0x44CD7 : 0xC3CA0;  // fire-mage or melee template
    }

    constexpr bool BashCoverage(Fixture fixture) {
        return fixture == Fixture::Sustained || fixture == Fixture::Endurance || fixture == Fixture::Heavy;
    }

    constexpr Loadout Equipment(Fixture fixture) {
        switch (fixture) {
            case Fixture::Heavy:
                return Loadout::Heavy;
            case Fixture::Archer:
                return Loadout::Bow;
            case Fixture::Projectile:
                return Loadout::Projectile;
            case Fixture::Concentration:
                return Loadout::Concentration;
            case Fixture::Area:
                return Loadout::Area;
            default:
                return Loadout::Sword;
        }
    }

    constexpr bool AdditionalActor(Fixture fixture) {
        return fixture == Fixture::AllyPressure || fixture == Fixture::Multiple;
    }

    struct Phase {
        Fixture fixture;
        const char* name;
        const char* description;
        float seconds;
    };

    inline constexpr std::uint32_t DefaultRepeats = 1, MaxRepeats = 100;
    inline constexpr std::array Phases{
        Phase{Fixture::Sustained, "Sustained pressure", "Full resources; sustained sword pressure.", 60},
        Phase{Fixture::LowStamina, "Low stamina", "Begin combat at 15% stamina; allow natural recovery.", 60},
        Phase{Fixture::RecoveryWaves, "Recovery waves", "Lower stamina to 15% at 0, 20 and 40 seconds.", 60},
        Phase{Fixture::Approach, "Distance closing", "Begin 1,500 units apart; observe attack and guard handoff.", 60},
        Phase{Fixture::Reposition, "Repositioning", "Move the opponent to alternating flanks at 20 and 40 seconds.",
              60},
        Phase{Fixture::Endurance, "Endurance", "Two minutes of uninterrupted pressure and stamina management.", 120},
        Phase{Fixture::Heavy, "Heavy melee", "Two-handed weapon pressure; report actual power-attack coverage.", 45},
        Phase{Fixture::Archer, "Archer approach", "Bow opponent begins at range; measure exposure and closing.", 45},
        Phase{Fixture::Projectile, "Projectile caster", "Firebolt opponent; observe lateral avoidance and approach.",
              45},
        Phase{Fixture::Concentration, "Sustained caster", "Flames opponent; observe movement out of the casting line.",
              45},
        Phase{Fixture::AllyPressure, "Ally protection",
              "Sword opponent targets a private protected passive ally; observe interception.", 45},
        Phase{Fixture::Multiple, "Two attackers",
              "Two protected sword opponents pressure the defender from different angles.", 45},
        Phase{Fixture::Area, "Area-capable caster",
              "Fireball opponent; observe spacing against an area-capable projectile.", 45}};

    constexpr bool ValidRepeats(std::uint32_t repeats) {
        return repeats >= 1 && repeats <= MaxRepeats;
    }

    constexpr std::uint32_t Runs(std::uint32_t repeats) {
        return ValidRepeats(repeats) ? static_cast<std::uint32_t>(Phases.size()) * repeats : 0;
    }

    constexpr std::uint32_t PhaseIndex(std::uint32_t run, std::uint32_t repeats) {
        return run && ValidRepeats(repeats) ? (run - 1) / repeats : 0;
    }

    constexpr std::uint32_t Repetition(std::uint32_t run, std::uint32_t repeats) {
        return run && ValidRepeats(repeats) ? (run - 1) % repeats + 1 : 0;
    }

    constexpr const Phase* ForRun(std::uint32_t run, std::uint32_t repeats) {
        return run && run <= Runs(repeats) ? &Phases[PhaseIndex(run, repeats)] : nullptr;
    }

    constexpr std::uint32_t ExpectedSteps(Fixture fixture) {
        return fixture == Fixture::LowStamina      ? 1
               : fixture == Fixture::RecoveryWaves ? 3
               : fixture == Fixture::Reposition    ? 2
                                                   : 0;
    }

    constexpr bool StepDue(Fixture fixture, std::uint32_t steps, float elapsed) {
        if (steps >= ExpectedSteps(fixture))
            return false;
        const float deadline =
            fixture == Fixture::Reposition ? static_cast<float>(steps + 1) * 20 : static_cast<float>(steps) * 20;
        return elapsed >= deadline;
    }

    constexpr float ActiveSeconds(std::uint32_t repeats) {
        if (!ValidRepeats(repeats))
            return 0;
        float seconds = 0;
        for (const auto& phase : Phases)
            seconds += phase.seconds * repeats;
        return seconds;
    }

    enum class Verdict : std::uint32_t { NotAssessed, Passed, Failed, Inconclusive };

    constexpr const char* Name(Verdict verdict) {
        switch (verdict) {
            case Verdict::Passed:
                return "Passed";
            case Verdict::Failed:
                return "Failed";
            case Verdict::Inconclusive:
                return "Inconclusive";
            default:
                return "Not assessed";
        }
    }

    struct Evidence {
        bool completed = false, restored = false, suspended = false;
        std::uint32_t timeouts = 0, attacks = 0, guards = 0, blocked = 0;
        bool readable = false;
        bool fixtureComplete = true;
        std::uint32_t recoveredInterruptTimeouts = 0;
        bool nativeBashRequired = false, budgetRequired = false;
        std::uint32_t completedBashes = 0, budgetDecisions = 0;
    };

    constexpr Verdict Assess(const Evidence& evidence) {
        if (!evidence.completed || !evidence.restored || evidence.suspended ||
            evidence.timeouts != evidence.recoveredInterruptTimeouts)
            return Verdict::Failed;
        if (!evidence.fixtureComplete || !evidence.readable || evidence.attacks < 5 || !evidence.guards ||
            !evidence.blocked || (evidence.nativeBashRequired && !evidence.completedBashes) ||
            (evidence.budgetRequired && !evidence.budgetDecisions))
            return Verdict::Inconclusive;
        return Verdict::Passed;
    }

    constexpr const char* Reason(const Evidence& evidence) {
        if (!evidence.completed)
            return "Run did not complete; see the stop reason.";
        if (!evidence.restored)
            return "Restoration remains unresolved.";
        if (evidence.suspended || evidence.timeouts != evidence.recoveredInterruptTimeouts)
            return "Native guards suspended or a timeout lacks verified interruption recovery.";
        if (!evidence.fixtureComplete)
            return "Not all fixture steps were applied.";
        if (!evidence.readable)
            return "Guard state was unavailable.";
        if (evidence.attacks < 5)
            return "Fewer than five opponent attack starts were observed.";
        if (!evidence.guards)
            return "No native guard was observed.";
        if (!evidence.blocked)
            return "No blocked hit event was observed.";
        if (evidence.nativeBashRequired && !evidence.completedBashes)
            return "No native defensive bash completed; bash coverage remains untested.";
        if (evidence.budgetRequired && !evidence.budgetDecisions)
            return "No valid UND budget decision was exercised.";
        return evidence.recoveredInterruptTimeouts
                   ? "Defense and clean restoration observed; interrupted guard timeouts recovered through verified "
                     "cleanup and resume."
                   : "Defense and clean restoration observed under the phase conditions.";
    }

    constexpr bool Clean(const Evidence& evidence) {
        return evidence.completed && evidence.restored && !evidence.suspended &&
               evidence.timeouts == evidence.recoveredInterruptTimeouts;
    }

    constexpr Verdict AssessTactical(const Evidence& evidence, const char* missing) {
        return !Clean(evidence) ? Verdict::Failed : missing ? Verdict::Inconclusive : Verdict::Passed;
    }

    constexpr const char* TacticalReason(const Evidence& evidence, const char* missing) {
        return !Clean(evidence) ? Reason(evidence)
               : missing
                   ? missing
                   : "Scenario response and clean restoration observed; compare exposure and damage for effectiveness.";
    }
}
