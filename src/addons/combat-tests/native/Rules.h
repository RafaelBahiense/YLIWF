#pragma once
#include "Encounters.h"
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
    enum class HitKind { Ignore, Incoming, Outgoing, Foreign };

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

    inline StartDecision ObserveStart(float elapsed, bool equipped, bool subjectCombat, bool enemyCombat) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return StartDecision::Failed;
        if (equipped && subjectCombat && enemyCombat)
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
    };

    static_assert(sizeof(Plan) == 64 && offsetof(Plan, mode) == 44 && offsetof(Plan, encounter) == 52);

    inline std::optional<Plan> DecodePlan(std::uint32_t version, std::span<const std::uint8_t> bytes) {
        if ((version != 1 || bytes.size() != 44) && (version != 2 || bytes.size() != 52) &&
            (version != 3 || bytes.size() != sizeof(Plan)))
            return std::nullopt;
        Plan result;
        std::memcpy(&result, bytes.data(), bytes.size());
        return result;
    }

    inline bool Valid(const Plan& plan) {
        return plan.subject && plan.subject != plan.enemy && plan.subject != plan.origin &&
               (!plan.origin || (plan.origin != plan.enemy && plan.origin != plan.enemyBase)) &&
               plan.subject != plan.enemyBase && plan.role < 7 && plan.changedRole <= 1 && plan.restoreIssued <= 1 &&
               plan.generation > 0 && plan.generation < 0x7FFFFFFE && ValidConfiguration(plan.encounter, plan.mode) &&
               plan.guardPhase <= 4 && (!plan.guardPhase || ((plan.mode & 2) && plan.origin && plan.encounter == 0)) &&
               plan.playerChanged <= 1 && std::isfinite(plan.playerHealth) && plan.playerHealth >= 0 &&
               (!plan.playerChanged || (plan.encounter == 1 && plan.playerHealth > 0)) && std::isfinite(plan.health) &&
               plan.health > 0 && std::isfinite(plan.stamina) && plan.stamina >= 0 && std::isfinite(plan.magicka) &&
               plan.magicka >= 0;
    }

    inline float ActiveDelta(float elapsed, bool paused) {
        return paused || !std::isfinite(elapsed) || elapsed <= 0 ? 0 : std::min(elapsed, 0.5f);
    }

    inline bool ProtectionPassed(bool invulnerable, bool alive, float before, float after) {
        return invulnerable && alive && std::isfinite(before) && std::isfinite(after) && before > 0 &&
               std::abs(after - before) < 0.01f;
    }
}
