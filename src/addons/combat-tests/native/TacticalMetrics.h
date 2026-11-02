#pragma once
#include "DefenderSuite.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace mod::combat_tests::tactics {
    struct Metrics {
        std::array<float, 7> intents{}, kinds{};
        float closest = -1, initialDistance = -1, moved = 0, previousX = 0, previousY = 0;
        std::uint32_t maxThreats = 0, allyHits = 0, powerWindups = 0;
        std::uint32_t casts = 0, friendlyHits = 0, crossfire = 0;
        float inactivity = 0, longestInactivity = 0, anchorX = 0, anchorY = 0;
        std::uint32_t previousIncoming = 0, previousOutgoing = 0;
        bool contact = false;
        bool positioned = false;

        void Contacts(std::uint32_t incoming, std::uint32_t outgoing) {
            contact |= incoming > previousIncoming || outgoing > previousOutgoing;
            previousIncoming = incoming;
            previousOutgoing = outgoing;
        }

        void Observe(float elapsed, std::uint32_t intent, std::uint32_t kind, std::uint32_t threats, float distance,
                     float x, float y) {
            if (!std::isfinite(elapsed) || elapsed <= 0 || elapsed > 1)
                return;
            if (intent < intents.size())
                intents[intent] += elapsed;
            if (kind < kinds.size())
                kinds[kind] += elapsed;
            maxThreats = std::max(maxThreats, threats);
            if (std::isfinite(distance) && distance >= 0) {
                if (initialDistance < 0)
                    initialDistance = distance;
                closest = closest < 0 ? distance : std::min(closest, distance);
            }
            if (!std::isfinite(x) || !std::isfinite(y))
                return;
            if (positioned) {
                const float step = std::hypot(x - previousX, y - previousY);
                // Arena teleports invalidate displacement evidence instead of
                // masquerading as a successful evasive movement.
                if (step <= 300)
                    moved += step;
            }
            const bool progress = !positioned || std::hypot(x - anchorX, y - anchorY) >= 32;
            if (contact || progress || !threats) {
                inactivity = 0;
                anchorX = x;
                anchorY = y;
            } else {
                inactivity += elapsed;
                longestInactivity = std::max(longestInactivity, inactivity);
            }
            contact = false;
            previousX = x;
            previousY = y;
            positioned = true;
        }
    };

    constexpr bool Tactical(suite::Fixture fixture) {
        return fixture >= suite::Fixture::Heavy;
    }

    inline const char* Missing(suite::Fixture fixture, const Metrics& metrics) {
        if ((fixture == suite::Fixture::Projectile || fixture == suite::Fixture::Concentration ||
             fixture == suite::Fixture::Area) &&
            !metrics.casts)
            return "No actual opponent spell cast was recorded; caster defense was not exercised.";
        if (metrics.friendlyHits || metrics.crossfire)
            return "Fixture friendly fire occurred; inspect its hit counts before comparing effectiveness.";
        if (metrics.inactivity >= 10)
            return "Encounter ended with at least ten seconds of no contact and little movement; earlier coverage is "
                   "insufficient.";
        switch (fixture) {
            case suite::Fixture::Heavy:
                return !metrics.powerWindups
                           ? "No actual power-attack windup observed; heavy response remains untested."
                       : metrics.intents[2] + metrics.intents[3] + metrics.intents[6] <= 0
                           ? "No defensive decision during heavy pressure."
                           : nullptr;
            case suite::Fixture::Archer:
                return metrics.kinds[3] <= 0     ? "No bow threat observed."
                       : metrics.intents[1] <= 0 ? "No archer approach decision observed."
                       : metrics.closest < 0 || metrics.closest > 256
                           ? "Defender did not reach the archer; inspect movement and exposure."
                           : nullptr;
            case suite::Fixture::Projectile:
            case suite::Fixture::Concentration:
            case suite::Fixture::Area:
                return metrics.kinds[fixture == suite::Fixture::Projectile      ? 4
                                     : fixture == suite::Fixture::Concentration ? 5
                                                                                : 6] <= 0
                           ? "Expected spell threat was not observed."
                       : metrics.intents[6] <= 0 || metrics.moved < 96
                           ? "No observed repositioning under spell pressure."
                           : nullptr;
            case suite::Fixture::AllyPressure:
                return metrics.maxThreats == 0                        ? "No ally threat observed."
                       : metrics.closest < 0 || metrics.closest > 256 ? "Defender did not reach the ally's attacker."
                                                                      : nullptr;
            case suite::Fixture::Multiple:
                return metrics.maxThreats < 2    ? "Two simultaneous party threats were not observed."
                       : metrics.intents[6] <= 0 ? "No response to overlapping or rear attacks observed."
                                                 : nullptr;
            default:
                return nullptr;
        }
    }
}
