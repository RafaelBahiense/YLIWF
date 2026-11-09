#pragma once
#include <cmath>
#include <cstdint>

namespace mod::combat_tests::engagement {
    struct Metrics {
        float firstReach = -1, firstContact = -1, outsideReach = 0, closingDamage = 0;
        float castingPressure = 0, aimLineExposure = 0;
        std::uint32_t aimLineExits = 0, targetSwitches = 0;
        bool castingEpisode = false, lineWasInside = false, positioned = false;
        float previousX = 0, previousY = 0, originX = 0, originY = 0, directionX = 0, directionY = 1;
        std::uint32_t previousTarget = 0;
        float flightReadableSeconds = 0, projectileGapSeconds = 0, projectilePressureSeconds = 0;

        void ObserveFlight(float delta, bool readable, bool gap, bool incoming) {
            if (!std::isfinite(delta) || delta <= 0 || delta > 1)
                return;
            if (readable)
                flightReadableSeconds += delta;
            if (readable && gap)
                projectileGapSeconds += delta;
            if (incoming)
                projectilePressureSeconds += delta;
        }

        void Observe(float clock, float delta, float distance, float damage, bool contact, std::uint32_t target,
                     float x, float y, float casterX, float casterY, float casterAngle, bool activeCast) {
            if (!std::isfinite(clock) || !std::isfinite(delta) || delta <= 0 || delta > 1 || !std::isfinite(distance) ||
                distance < 0 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(casterX) ||
                !std::isfinite(casterY) || !std::isfinite(casterAngle)) {
                positioned = castingEpisode = lineWasInside = false;
                return;
            }
            if (previousTarget && target && target != previousTarget)
                ++targetSwitches;
            previousTarget = target;
            if (distance <= 256 && firstReach < 0)
                firstReach = clock;
            if (contact && firstContact < 0)
                firstContact = clock;
            if (distance > 256) {
                outsideReach += delta;
                if (std::isfinite(damage) && damage > 0)
                    closingDamage += damage;
            }
            const bool continuous = positioned && std::hypot(x - previousX, y - previousY) <= 300;
            if (activeCast) {
                castingPressure += delta;
                if (!castingEpisode || !continuous) {
                    originX = casterX;
                    originY = casterY;
                    directionX = std::sin(casterAngle);
                    directionY = std::cos(casterAngle);
                    lineWasInside = false;
                }
                const float dx = x - originX, dy = y - originY;
                const bool inside =
                    dx * directionX + dy * directionY >= 0 && std::abs(dx * directionY - dy * directionX) <= 96;
                if (inside)
                    aimLineExposure += delta;
                if (castingEpisode && continuous && lineWasInside && !inside)
                    ++aimLineExits;
                lineWasInside = inside;
            } else {
                lineWasInside = false;
            }
            castingEpisode = activeCast;
            positioned = true;
            previousX = x;
            previousY = y;
        }
    };
}
