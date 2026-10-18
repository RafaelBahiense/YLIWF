#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace mod::healing_rules {
    inline constexpr float HealthThreshold = 0.70f;
    inline constexpr float RecoveryHealthThreshold = 0.999f;
    inline constexpr float MaximumRange = 1024.0f;
    inline constexpr float TouchRange = 128.0f;
    inline constexpr float RetryBackoff = 4.0f;
    inline constexpr float TargetReservation = 2.0f;
    inline constexpr float WorldUnitsPerFoot = 128.0f / 6.0f;
    inline constexpr float SearchRange = 4096;
    inline constexpr float ThreatDistance = 256;
    inline constexpr float SafeDistance = 512;
    inline constexpr float RetreatCompleteDistance = 640;
    inline constexpr float EmergencyDistance = 128;
    inline constexpr float CastThreatGrace = 2.5f;
    inline constexpr float CastTimeout = 8;
    inline constexpr float TravelTimeout = 12;
    inline constexpr float PursuitTimeout = 8;
    inline constexpr float OffensiveInterval = 6;
    inline constexpr float EscapeProgress = 64;

    struct EscapeAttempt {
        float noProgress = 0;
        std::optional<float> separation;

        void Reset() {
            noProgress = 0;
            separation.reset();
        }

        bool Failed(float distance, float elapsed) {
            if (!separation || distance >= *separation + EscapeProgress) {
                separation = distance;
                noProgress = 0;
                return false;
            }
            noProgress += std::max(0.0f, elapsed);
            return noProgress >= PursuitTimeout;
        }
    };

    inline bool Retreating(bool previous, float enemyDistance) {
        return enemyDistance < (previous ? RetreatCompleteDistance : SafeDistance);
    }

    inline bool SafeToStartCast(float enemyDistance) {
        return enemyDistance >= SafeDistance;
    }

    inline bool InterruptForThreat(float enemyDistance, float elapsed) {
        return enemyDistance < EmergencyDistance || (enemyDistance < ThreatDistance && elapsed >= CastThreatGrace);
    }

    inline float SpellRange(float authoredFeet, bool touch) {
        if (touch)
            return TouchRange;
        if (!std::isfinite(authoredFeet) || authoredFeet <= 0)
            return MaximumRange;
        return std::min(authoredFeet * WorldUnitsPerFoot, MaximumRange);
    }

    struct Target {
        std::uint32_t id = 0;
        float health = 0, maximumHealth = 0, distance = 0;
        bool alive = false, loaded = false, friendly = false, sameArea = false, visible = false;
        bool reserved = false;
        bool bleedingOut = false, spellApplicable = true;
        bool actorSight = false, geometryAvailable = false;
        std::uint32_t obstruction = 0;
    };

    inline std::optional<float> HealthFraction(const Target& target) {
        if (!std::isfinite(target.health) || !std::isfinite(target.maximumHealth) || target.maximumHealth <= 0 ||
            (target.health <= 0 && !target.bleedingOut))
            return std::nullopt;
        return std::clamp(target.health / target.maximumHealth, 0.0f, 1.0f);
    }

    inline bool Seekable(const Target& target, std::uint32_t healer, bool combat = true) {
        const auto fraction = HealthFraction(target);
        return target.id && target.id != healer && target.alive && target.loaded && target.friendly &&
               target.sameArea && !target.reserved && target.spellApplicable && fraction &&
               *fraction < (combat ? HealthThreshold : RecoveryHealthThreshold) && std::isfinite(target.distance) &&
               target.distance >= 0 && target.distance <= SearchRange;
    }

    inline bool SelfEligible(const Target& target, std::uint32_t healer, bool combat) {
        const auto fraction = HealthFraction(target);
        return target.id == healer && target.alive && target.loaded && !target.bleedingOut && target.spellApplicable &&
               fraction && *fraction < (combat ? HealthThreshold : RecoveryHealthThreshold);
    }

    inline bool Eligible(const Target& target, std::uint32_t healer, float range, bool combat = true) {
        return Seekable(target, healer, combat) && target.visible && std::isfinite(range) && range > 0 &&
               target.distance <= std::min(range, MaximumRange);
    }

    inline bool MoreUrgent(const Target& target, const Target& current) {
        const auto fraction = HealthFraction(target), previous = HealthFraction(current);
        return fraction &&
               (!previous || *fraction < *previous || (*fraction == *previous && target.distance < current.distance));
    }

    inline std::optional<std::size_t> Choose(std::span<const Target> targets, std::uint32_t healer, float range) {
        std::optional<std::size_t> selected;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (!Eligible(targets[i], healer, range))
                continue;
            if (!selected || MoreUrgent(targets[i], targets[*selected]))
                selected = i;
        }
        return selected;
    }

    inline float Remaining(float seconds, float elapsed) {
        return std::isfinite(elapsed) && elapsed > 0 ? std::max(0.0f, seconds - elapsed) : seconds;
    }

}
