#pragma once
#include "../scenarios/Encounters.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <optional>
#include <span>

namespace mod::test_rules {
    inline constexpr float Duration = 60;
    inline constexpr float Countdown = 3;
    inline constexpr float StartTimeout = 10;
    inline constexpr std::uint32_t ShieldRole = 5;
    inline constexpr std::uint32_t MaxRuns = 100;
    inline constexpr float DefeatRecoveryTimeout = 30;
    inline constexpr float StartupSettleTimeout = 30;

    enum class StartupDecision { Ready, WaitScene, WaitCombat, Reject };

    struct StartupState {
        bool exists = false, managed = false, allowed = false;
        bool dead = false, disabled = false, mounted = false, unconscious = false, downed = false;
        bool scene = false, combat = false;
    };

    constexpr StartupDecision Startup(const StartupState& state, float waited) {
        if (!state.exists || !state.managed || !state.allowed || state.dead || state.disabled || state.mounted ||
            state.unconscious || state.downed)
            return StartupDecision::Reject;
        if (state.scene || state.combat) {
            if (waited >= StartupSettleTimeout)
                return StartupDecision::Reject;
            return state.scene ? StartupDecision::WaitScene : StartupDecision::WaitCombat;
        }
        return StartupDecision::Ready;
    }

    constexpr bool RecoverableDefeat(bool running, bool exists, bool dead, bool downed, bool disabled, bool scene) {
        return running && exists && !dead && downed && !disabled && !scene;
    }

    constexpr bool RepeatableRun(bool completed, bool defeated, bool cancelled, bool failedSuiteRun = false) {
        return !cancelled && (completed || defeated || failedSuiteRun);
    }

    enum class DefeatRecovery { Ready, Wait, Review };

    inline DefeatRecovery ObserveDefeatRecovery(bool downed, float health, float elapsed) {
        if (!std::isfinite(health) || health <= 0)
            return DefeatRecovery::Review;
        if (!downed)
            return DefeatRecovery::Ready;
        return elapsed < DefeatRecoveryTimeout ? DefeatRecovery::Wait : DefeatRecovery::Review;
    }

    constexpr bool ContinueSeries(std::uint32_t finished, std::uint32_t requested, bool repeatable, bool restored,
                                  std::uint32_t limit = MaxRuns) {
        return requested > 0 && requested <= limit && finished < requested && repeatable && restored;
    }
    enum class PositionAction { Complete, Move, Wait };
    enum class StartDecision { Wait, Ready, Failed };
    enum class BeginResult : std::int32_t { Accepted = 1, Expired = 2, MissingSword = 3, MissingActor = 4 };
    enum class StartBlock { None, Loading, Active, Unsupported, Recovery };

    constexpr bool CanRecheckRecovery(bool ready, bool active, bool unsupported, bool retained, bool restored,
                                      bool callbackPending) {
        return ready && !active && !unsupported && retained && restored && !callbackPending;
    }

    constexpr StartBlock BlockStart(bool ready, bool active, bool unsupported, bool recovery) {
        if (!ready)
            return StartBlock::Loading;
        if (active)
            return StartBlock::Active;
        if (unsupported)
            return StartBlock::Unsupported;
        return recovery ? StartBlock::Recovery : StartBlock::None;
    }

    constexpr const char* StartReason(StartBlock block) {
        switch (block) {
            case StartBlock::None:
                return "";
            case StartBlock::Loading:
                return "Combat tests are waiting for game/host initialization.";
            case StartBlock::Active:
                return "A test or restoration is still running.";
            case StartBlock::Unsupported:
                return "Unsupported recovery data retained; new tests are disabled.";
            case StartBlock::Recovery:
                return "Previous recovery is unfinished. Review the last report; load the pre-test save if manual "
                       "review is required.";
        }
        return "Combat tests are unavailable.";
    }
    enum class HitKind { Ignore, Incoming, Outgoing, Foreign, AllyIncoming, AllyOutgoing, Friendly, Crossfire };

    constexpr HitKind ClassifyHit(std::uint32_t subject, std::uint32_t enemy, std::uint32_t target,
                                  std::uint32_t cause) {
        if (!subject || !enemy || subject == enemy)
            return HitKind::Ignore;
        if (target == subject && cause == enemy)
            return HitKind::Incoming;
        if (target == enemy && cause == subject)
            return HitKind::Outgoing;
        if ((target == subject || target == enemy) && cause && cause != subject && cause != enemy)
            return HitKind::Foreign;
        return HitKind::Ignore;
    }

    constexpr HitKind ClassifyFixtureHit(std::uint32_t subject, std::uint32_t enemy, std::uint32_t auxiliary, bool ally,
                                         std::uint32_t target, std::uint32_t cause) {
        if (auxiliary) {
            if (ally) {
                if (target == auxiliary && cause == enemy)
                    return HitKind::AllyIncoming;
                if (target == enemy && cause == auxiliary)
                    return HitKind::AllyOutgoing;
                if ((target == subject && cause == auxiliary) || (target == auxiliary && cause == subject))
                    return HitKind::Friendly;
            } else {
                if ((target == enemy && cause == auxiliary) || (target == auxiliary && cause == enemy))
                    return HitKind::Crossfire;
                if (target == auxiliary || cause == auxiliary)
                    return ClassifyHit(subject, auxiliary, target, cause);
            }
        }
        return ClassifyHit(subject, enemy, target, cause);
    }

    inline StartDecision ObserveStart(float elapsed, bool equipped, bool subjectCombat, bool enemyCombat,
                                      bool engagementReady = true) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return StartDecision::Failed;
        if (equipped && subjectCombat && enemyCombat && engagementReady)
            return StartDecision::Ready;
        return elapsed >= StartTimeout ? StartDecision::Failed : StartDecision::Wait;
    }

    constexpr PositionAction RestorePosition(bool atOrigin, bool issued) {
        return atOrigin ? PositionAction::Complete : issued ? PositionAction::Wait : PositionAction::Move;
    }

    constexpr bool CurrentTicket(std::uint32_t expected, std::uint32_t actual, bool ready, bool active, bool stopped) {
        return expected && expected == actual && ready && active && !stopped;
    }

    constexpr bool RestoreRole(bool changed, std::uint32_t current, std::uint32_t encounter = 0) {
        return changed && current == EncounterRole(encounter);
    }

    struct Plan {
        std::uint32_t subject = 0, enemy = 0, enemyBase = 0, origin = 0, role = 0;
        float health = 0, stamina = 0, magicka = 0;
        std::uint32_t changedRole = 0, restoreIssued = 0, generation = 0;
        std::uint32_t mode = 0, guardPhase = 0;
        std::uint32_t encounter = 0;
        float playerHealth = 0;
        std::uint32_t playerChanged = 0;
        std::uint32_t wasWaiting = 0, followRequest = 0, waitRequest = 0;
        std::uint32_t auxiliary = 0, fixture = 0;
    };

    static_assert(sizeof(Plan) == 84 && offsetof(Plan, mode) == 44 && offsetof(Plan, encounter) == 52);

    inline std::optional<Plan> DecodePlan(std::uint32_t version, std::span<const std::uint8_t> bytes) {
        if ((version != 1 || bytes.size() != 44) && (version != 2 || bytes.size() != 52) &&
            (version != 3 || bytes.size() != 64) && (version != 4 || bytes.size() != 76) &&
            (version != 5 || bytes.size() != sizeof(Plan)))
            return std::nullopt;
        Plan result;
        std::memcpy(&result, bytes.data(), bytes.size());
        return result;
    }

    inline bool Valid(const Plan& plan) {
        return plan.fixture < 13 &&
               (!plan.auxiliary ||
                (plan.auxiliary != plan.subject && plan.auxiliary != plan.enemy && plan.auxiliary != plan.origin &&
                 plan.auxiliary != plan.enemyBase && (plan.fixture == 10 || plan.fixture == 11))) &&
               plan.subject && plan.subject != plan.enemy && plan.subject != plan.origin &&
               (!plan.origin || (plan.origin != plan.enemy && plan.origin != plan.enemyBase)) &&
               plan.subject != plan.enemyBase && plan.role < 7 && plan.changedRole <= 1 && plan.restoreIssued <= 1 &&
               plan.generation > 0 && plan.generation < 0x7FFFFFFE &&
               ValidSavedConfiguration(plan.encounter, plan.mode) && plan.guardPhase <= 4 &&
               (!plan.guardPhase ||
                ((plan.mode & 2) && plan.origin &&
                 (plan.encounter == 0 || NaturalEncounter(plan.encounter) || DirectEncounter(plan.encounter)))) &&
               plan.wasWaiting <= 1 && plan.followRequest <= 0x7FFFFFFF && plan.waitRequest <= 0x7FFFFFFF &&
               (!plan.waitRequest || (plan.followRequest && plan.waitRequest > plan.followRequest)) &&
               (!plan.followRequest || plan.wasWaiting) && plan.playerChanged <= 1 &&
               std::isfinite(plan.playerHealth) && plan.playerHealth >= 0 &&
               (!plan.playerChanged || (plan.encounter == 1 && plan.playerHealth > 0)) && std::isfinite(plan.health) &&
               plan.health > 0 && std::isfinite(plan.stamina) && plan.stamina >= 0 && std::isfinite(plan.magicka) &&
               plan.magicka >= 0;
    }

    inline float ActiveDelta(float elapsed, bool paused) {
        return paused || !std::isfinite(elapsed) || elapsed <= 0 ? 0 : std::min(elapsed, 0.5f);
    }

    inline bool ProtectionPassed(bool protectedActor, bool alive, float before, float after) {
        return protectedActor && alive && std::isfinite(before) && std::isfinite(after) && before > 0 &&
               std::abs(after - before) < 0.01f;
    }
}
