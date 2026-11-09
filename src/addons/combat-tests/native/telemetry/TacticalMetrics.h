#pragma once

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
        float allyInitialHealth = -1, allyHealth = -1, allyDamage = 0, allyExposure = 0, firstIntervention = -1;
        bool allyThreatObserved = false;
        std::uint32_t tauntHitEvents = 0;
        std::int32_t subjectHostile = -1, enemyHostile = -1;

        void ObserveAlly(float clock, float elapsed, float health, bool targeted, bool redirected) {
            if (!std::isfinite(health))
                return;
            if (allyInitialHealth < 0)
                allyInitialHealth = std::max(0.0f, health);
            if (allyHealth >= 0)
                allyDamage += std::max(0.0f, allyHealth - std::max(0.0f, health));
            allyHealth = std::max(0.0f, health);
            allyThreatObserved |= targeted;
            if (targeted && std::isfinite(elapsed) && elapsed > 0 && elapsed <= 1)
                allyExposure += elapsed;
            if (redirected && allyThreatObserved && firstIntervention < 0)
                firstIntervention = clock;
        }

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

}
