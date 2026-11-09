#pragma once
#include <algorithm>
#include <initializer_list>
#include <vector>
#include <cstdint>
#include "../telemetry/TacticalMetrics.h"
#include "../testing/Assessment.h"

namespace mod::combat_tests::suite {
    enum class Criterion {
        RecoveryExit,
        NativeBash,
        DodgeBudget,
        OpponentAttack,
        OpponentCast,
        HeavyWindup,
        DefensiveDecision,
        BowThreat,
        ProjectileThreat,
        ConcentrationThreat,
        AreaThreat,
        Approach,
        ReachOpponent,
        Reposition,
        RepositionDecision,
        AllyThreat,
        AllyExposure,
        Intervention,
        MultipleThreats,
        AllOpponentsEngaged
    };

    struct Requirements {
        bool tactical = false;
        std::vector<Criterion> criteria;

        static Requirements Melee(std::initializer_list<Criterion> criteria = {}) {
            return {false, criteria};
        }

        static Requirements Tactical(std::initializer_list<Criterion> criteria) {
            return {true, criteria};
        }

        bool Has(Criterion criterion) const {
            return std::ranges::find(criteria, criterion) != criteria.end();
        }

        bool Valid() const {
            for (const auto criterion : criteria)
                if (criterion < Criterion::RecoveryExit || criterion > Criterion::AllOpponentsEngaged)
                    return false;
            return true;
        }
    };

    struct ParticipantCoverage {
        std::uint32_t opponents = 0, attackingOpponents = 0;
    };

    inline const char* Missing(const Requirements& requirements, const tactics::Metrics& metrics, std::uint32_t attacks,
                               const ParticipantCoverage& participants = {}) {
        if (requirements.tactical) {
            if (requirements.criteria.empty())
                return "No tactical assessment criteria were declared.";
            if (metrics.friendlyHits || metrics.crossfire)
                return "Participant friendly fire occurred; inspect hit counts before comparing effectiveness.";
            if (metrics.inactivity >= 10)
                return "Encounter ended with at least ten seconds of no contact and little movement.";
        }
        for (const auto criterion : requirements.criteria) {
            switch (criterion) {
                case Criterion::OpponentAttack:
                    if (!attacks)
                        return "No opponent attack was observed.";
                    break;
                case Criterion::OpponentCast:
                    if (!metrics.casts)
                        return "No actual opponent spell cast was recorded.";
                    break;
                case Criterion::HeavyWindup:
                    if (!metrics.powerWindups)
                        return "No actual power-attack windup observed.";
                    break;
                case Criterion::DefensiveDecision:
                    if (metrics.intents[2] + metrics.intents[3] + metrics.intents[6] <= 0)
                        return "No defensive decision during heavy pressure.";
                    break;
                case Criterion::BowThreat:
                    if (metrics.kinds[3] <= 0)
                        return "No bow threat observed.";
                    break;
                case Criterion::ProjectileThreat:
                    if (metrics.kinds[4] <= 0)
                        return "No projectile spell threat observed.";
                    break;
                case Criterion::ConcentrationThreat:
                    if (metrics.kinds[5] <= 0)
                        return "No concentration spell threat observed.";
                    break;
                case Criterion::AreaThreat:
                    if (metrics.kinds[6] <= 0)
                        return "No area spell threat observed.";
                    break;
                case Criterion::Approach:
                    if (metrics.intents[1] <= 0)
                        return "No approach decision observed.";
                    break;
                case Criterion::ReachOpponent:
                    if (metrics.closest < 0 || metrics.closest > 256)
                        return "Defender did not reach the opponent.";
                    break;
                case Criterion::Reposition:
                    if (metrics.intents[6] <= 0 || metrics.moved < 96)
                        return "No useful repositioning under pressure.";
                    break;
                case Criterion::RepositionDecision:
                    if (metrics.intents[6] <= 0)
                        return "No response to overlapping or rear attacks observed.";
                    break;
                case Criterion::AllyThreat:
                    if (!metrics.maxThreats)
                        return "No ally threat observed.";
                    break;
                case Criterion::AllyExposure:
                    if (metrics.allyExposure <= 0)
                        return "The ally was never targeted during measured combat.";
                    break;
                case Criterion::Intervention:
                    if (metrics.firstIntervention < 0)
                        return "No defender hit or enemy attention shift was observed.";
                    break;
                case Criterion::MultipleThreats:
                    if (metrics.maxThreats < 2)
                        return "Two simultaneous party threats were not observed.";
                    break;
                case Criterion::AllOpponentsEngaged:
                    if (!participants.opponents || participants.attackingOpponents < participants.opponents)
                        return "Not every declared opponent began an attack.";
                    break;
                case Criterion::RecoveryExit:
                case Criterion::NativeBash:
                case Criterion::DodgeBudget:
                    break;  // Checked against native action and recovery evidence.
            }
        }
        return nullptr;
    }

    using testing::Name;
    using testing::Verdict;

    struct Evidence {
        bool completed = false, restored = false, suspended = false;
        std::uint32_t timeouts = 0, attacks = 0, guards = 0, blocked = 0;
        bool readable = false;
        bool fixtureComplete = true;
        std::uint32_t recoveredInterruptTimeouts = 0;
        bool nativeBashRequired = false, budgetRequired = false;
        std::uint32_t completedBashes = 0, budgetDecisions = 0;
        bool recoveryRequired = false;
        std::uint32_t recoveryExits = 0;
    };

    constexpr const char* MissingActions(const Evidence& evidence) {
        if (evidence.nativeBashRequired && !evidence.completedBashes)
            return "No native defensive bash completed; bash coverage remains untested.";
        if (evidence.budgetRequired && !evidence.budgetDecisions)
            return "No valid UND budget decision was exercised.";
        return nullptr;
    }

    constexpr Verdict Assess(const Evidence& evidence) {
        if (!testing::Clean({evidence.completed, evidence.restored}) || evidence.suspended ||
            evidence.timeouts != evidence.recoveredInterruptTimeouts ||
            (evidence.recoveryRequired && !evidence.recoveryExits))
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
        if (evidence.recoveryRequired && !evidence.recoveryExits)
            return "Stamina recovery never exited under pressure; recovery behavior needs improvement.";
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
        if (const auto* missing = MissingActions(evidence))
            return missing;
        return evidence.recoveredInterruptTimeouts
                   ? "Defense and clean restoration observed; interrupted guard timeouts recovered through verified "
                     "cleanup and resume."
                   : "Defense and clean restoration observed under the phase conditions.";
    }

    constexpr bool Clean(const Evidence& evidence) {
        return testing::Clean({evidence.completed, evidence.restored}) && !evidence.suspended &&
               evidence.timeouts == evidence.recoveredInterruptTimeouts &&
               (!evidence.recoveryRequired || evidence.recoveryExits);
    }

    constexpr Verdict AssessTactical(const Evidence& evidence, const char* missing) {
        return !Clean(evidence)                                                   ? Verdict::Failed
               : !evidence.fixtureComplete || missing || MissingActions(evidence) ? Verdict::Inconclusive
                                                                                  : Verdict::Passed;
    }

    constexpr const char* TacticalReason(const Evidence& evidence, const char* missing) {
        return !Clean(evidence)            ? Reason(evidence)
               : !evidence.fixtureComplete ? "Not all declared phase interventions were applied."
               : missing                   ? missing
               : MissingActions(evidence)
                   ? MissingActions(evidence)
                   : "Scenario response and clean restoration observed; compare exposure and damage for effectiveness.";
    }

    struct Assessment {
        Verdict verdict;
        const char* reason;
        const char* missing;
    };

    inline Assessment Evaluate(const Requirements& requirements, Evidence evidence, const tactics::Metrics& metrics,
                               const ParticipantCoverage& participants = {}) {
        if (!requirements.Valid())
            return {Verdict::Failed, "Case assessment contains an unknown criterion.", nullptr};
        evidence.recoveryRequired = requirements.Has(Criterion::RecoveryExit);
        evidence.nativeBashRequired &= requirements.Has(Criterion::NativeBash);
        evidence.budgetRequired &= requirements.Has(Criterion::DodgeBudget);
        const auto* missing = Missing(requirements, metrics, evidence.attacks, participants);
        if (requirements.tactical)
            return {AssessTactical(evidence, missing), TacticalReason(evidence, missing), missing};
        const auto verdict = Assess(evidence);
        if (verdict == Verdict::Passed && missing)
            return {Verdict::Inconclusive, missing, missing};
        return {verdict, Reason(evidence), missing};
    }
}
