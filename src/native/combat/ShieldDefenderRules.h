#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

namespace mod::shield_defender_rules {
    inline constexpr float ExhaustedFraction = 0.40f;
    inline constexpr float RecoveredFraction = 0.65f;
    inline constexpr float GuardDistance = 288;
    inline constexpr float GuardReleaseDistance = 352;
    inline constexpr float GuardCommitment = 0.8f;
    inline constexpr float CounterDistance = 192;

    enum class Stance { Pressure, Guard, Recover, GuardRecover, Counter, Count };

    struct Threat {
        float distance = 0;
        bool targetingDefender = false, attacking = false, melee = false, windingUp = false;
    };

    inline bool IncomingMelee(const Threat& threat, bool guarding) {
        return threat.targetingDefender && (threat.attacking || threat.windingUp) && threat.melee &&
               std::isfinite(threat.distance) && threat.distance >= 0 &&
               threat.distance <= (guarding ? GuardReleaseDistance : GuardDistance);
    }

    inline bool KeepGuard(bool incoming, bool threatStillNearby, float sinceThreat, bool safeCounterOpening = false) {
        return incoming || (!safeCounterOpening && threatStillNearby && std::isfinite(sinceThreat) &&
                            sinceThreat >= 0 && sinceThreat < GuardCommitment);
    }

    constexpr bool Guarding(Stance stance) {
        return stance == Stance::Guard || stance == Stance::GuardRecover;
    }

    inline bool Recovering(bool previous, float stamina, float maximum) {
        if (!std::isfinite(stamina) || !std::isfinite(maximum) || maximum <= 0)
            return true;
        const float fraction = std::clamp(stamina / maximum, 0.0f, 1.0f);
        return previous ? fraction < RecoveredFraction : fraction <= ExhaustedFraction;
    }

    inline bool CounterOpportunity(float distance, bool recoiling, bool staggered) {
        return std::isfinite(distance) && distance >= 0 && distance <= CounterDistance && (recoiling || staggered);
    }

    constexpr Stance ChooseStance(bool recovering, bool incomingMelee, bool counterOpportunity = false) {
        if (recovering)
            return incomingMelee ? Stance::GuardRecover : Stance::Recover;
        if (incomingMelee)
            return Stance::Guard;
        return counterOpportunity ? Stance::Counter : Stance::Pressure;
    }

    constexpr std::string_view Description(Stance stance) {
        switch (stance) {
            case Stance::Pressure:
                return "Defensive frontline: attacking between threats";
            case Stance::Guard:
                return "Incoming melee: favoring shield blocks";
            case Stance::Recover:
                return "Conserving stamina between threats";
            case Stance::GuardRecover:
                return "Incoming melee: favoring blocks without bashing";
            case Stance::Counter:
                return "Opponent recoiling or staggered: favoring a counterattack";
            case Stance::Count:
                break;
        }
        return "Defender stance unavailable";
    }

    constexpr bool CanDefend(bool recruited, bool combat, bool standDown, bool scene, bool waiting, bool incapacitated,
                             bool foreignPackage) {
        return recruited && combat && !standDown && !scene && !waiting && !incapacitated && !foreignPackage;
    }

    enum class StyleAction { None, Acquire, Restore, Reapply, Release };

    constexpr StyleAction DecideStyle(bool requested, bool owned, bool ours, bool original, bool desired = true) {
        if (!requested)
            return owned && ours ? StyleAction::Restore : StyleAction::Release;
        if (!owned)
            return StyleAction::Acquire;
        if (ours)
            return desired ? StyleAction::None : StyleAction::Reapply;
        return original ? StyleAction::Reapply : StyleAction::Release;
    }
}
