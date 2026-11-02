#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

namespace mod::defender_tactics {
    enum class Kind : std::uint32_t { Unknown, Melee, HeavyMelee, Bow, Projectile, Concentration, Area };
    enum class Intent : std::uint32_t { Yield, Approach, Guard, Interrupt, Recover, Counter, Reposition };
    enum class Motion : std::uint32_t { None, Close, Retreat, Lateral, Intercept };

    struct Threat {
        std::uint32_t actor = 0;
        Kind kind = Kind::Unknown;
        float distance = -1, heading = 0, reach = 288;
        bool self = false, ally = false, attacking = false, windup = false, opening = false;
        bool visible = false, readable = false;
    };

    struct Snapshot {
        bool eligible = false, recovering = false, magicGuard = false, movement = false;
        float stamina = 0, maximum = 0;
        // A readable attack state is not a projectile-flight observation.
        // Never call an archer's reload gap safe without independent evidence.
        std::span<const Threat> threats;
    };

    struct Decision {
        Intent intent = Intent::Yield;
        Motion motion = Motion::None;
        std::uint32_t target = 0;
        bool guard = false, offense = false, interrupt = false, uncertain = false;
        const char* reason = "Yielding to live follower restrictions";
    };

    inline bool Valid(const Threat& threat) {
        return threat.actor && std::isfinite(threat.distance) && threat.distance >= 0 && threat.distance <= 2400 &&
               std::isfinite(threat.heading) && (threat.self || threat.ally);
    }

    inline bool Melee(Kind kind) {
        return kind == Kind::Melee || kind == Kind::HeavyMelee;
    }

    inline float Reach(const Threat& threat) {
        // Precision's weapon capsule omits bodies and tactical margin. Keep a
        // conservative fallback and bound unusually long modded weapons.
        return std::isfinite(threat.reach) && threat.reach > 0 ? std::clamp(threat.reach, 192.0f, 448.0f) : 288;
    }

    inline bool Imminent(const Threat& threat) {
        if (!Valid(threat) || !threat.self)
            return false;
        if (Melee(threat.kind))
            return threat.distance <= Reach(threat) && (threat.attacking || threat.windup);
        return threat.attacking || threat.windup;
    }

    inline Decision Choose(const Snapshot& state) {
        if (!state.eligible)
            return {};
        const Threat* selected = nullptr;
        float best = -1;
        std::uint32_t imminent = 0;
        bool rear = false;
        for (const auto& threat : state.threats) {
            if (!Valid(threat))
                continue;
            const bool danger = Imminent(threat);
            imminent += danger;
            rear |= danger && std::abs(threat.heading) > 85;
            const float score = (danger ? 10000.0f : 0) + (threat.ally ? 3000.0f : 0) +
                                (threat.kind == Kind::HeavyMelee ? 500.0f : 0) - threat.distance;
            if (score > best || !selected) {
                selected = &threat;
                best = score;
            }
        }
        if (!selected)
            return {Intent::Approach,
                    Motion::None,
                    0,
                    false,
                    true,
                    false,
                    true,
                    "No readable party threat; ordinary combat AI retains target selection"};
        const auto& threat = *selected;
        const bool danger = Imminent(threat);
        const bool front = std::abs(threat.heading) <= 85;
        const bool affordable = std::isfinite(state.stamina) && std::isfinite(state.maximum) && state.maximum > 0 &&
                                state.stamina >= 35 + state.maximum * 0.2f;
        Decision result{Intent::Approach,
                        state.movement && Melee(threat.kind) && threat.distance > 192 ? Motion::Close : Motion::None,
                        threat.actor,
                        false,
                        true,
                        false,
                        !threat.readable,
                        "Pressure the party threat; ordinary AI chooses attacks and combos"};
        if (danger && (rear || imminent > 1) && state.movement) {
            result = {Intent::Reposition,
                      Motion::Retreat,
                      threat.actor,
                      front,
                      false,
                      false,
                      false,
                      "Overlapping or rear threats: create space and bring attackers in front"};
        } else if (danger && threat.kind == Kind::HeavyMelee) {
            if (front && threat.windup && affordable && !state.recovering && threat.distance <= 144) {
                result = {Intent::Interrupt,
                          Motion::None,
                          threat.actor,
                          true,
                          false,
                          true,
                          false,
                          "Heavy windup in bash range with stamina reserve: interrupt"};
            } else {
                result = {Intent::Reposition,
                          state.movement ? Motion::Retreat : Motion::None,
                          threat.actor,
                          front,
                          false,
                          false,
                          false,
                          "Heavy attack: create space; shield is the fallback"};
            }
        } else if (danger && Melee(threat.kind)) {
            result = {Intent::Guard, Motion::None, threat.actor, front,
                      false,         false,        false,        "Committed melee attack: face the threat and brace"};
        } else if (threat.kind == Kind::Bow) {
            result.guard = front && threat.self && threat.visible && (threat.distance > 192 || danger);
            result.motion = state.movement && threat.distance > 192 ? Motion::Close : Motion::None;
            result.offense = threat.distance <= 192 && !danger;
            result.reason = "Close on the archer behind the shield; projectile-flight gap is unverified";
        } else if (!Melee(threat.kind) && threat.kind != Kind::Unknown) {
            result.guard = state.magicGuard && front && danger;
            result.motion =
                state.movement ? (danger && !state.magicGuard ? Motion::Lateral : Motion::Close) : Motion::None;
            result.intent = danger && !state.magicGuard ? Intent::Reposition : Intent::Approach;
            result.offense = !danger && threat.distance <= 192;
            result.reason = state.magicGuard ? "Guard-specific magic protection available: close behind the shield"
                                             : "Magic protection unverified: move off the casting line";
        } else if (state.recovering) {
            result = {Intent::Recover, state.movement ? Motion::Retreat : Motion::None,
                      threat.actor,    false,
                      false,           false,
                      false,           "Low stamina: create a recovery gap and conserve offensive actions"};
        } else if (threat.opening && !imminent && threat.distance <= 192) {
            result.intent = Intent::Counter;
            result.reason = "Opponent recoiling or staggered, no overlapping attack: counter";
        } else if (threat.ally && state.movement && threat.distance > 192) {
            result.motion = Motion::Intercept;
            result.reason = "Close on the enemy threatening an ally; pressure does not guarantee a taunt";
        }
        return result;
    }

    constexpr std::string_view Name(Intent intent) {
        switch (intent) {
            case Intent::Yield:
                return "Yield";
            case Intent::Approach:
                return "Approach";
            case Intent::Guard:
                return "Guard";
            case Intent::Interrupt:
                return "Interrupt";
            case Intent::Recover:
                return "Recover";
            case Intent::Counter:
                return "Counter";
            case Intent::Reposition:
                return "Reposition";
        }
        return "Unknown";
    }
}
