#pragma once
#include "../fixtures/FixtureResourceRules.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace mod::combat_tests::comparison {
    struct Setup {
        std::uint32_t weapon = 0, shield = 0, opponentBase = 0;
        float health = 0, stamina = 0, magicka = 0, allyHealth = 0, enemyHealth = 0;
        resources::Stats subjectStats{}, enemyStats{}, allyStats{};
        std::uint64_t subjectEquipment = 0, enemyEquipment = 0, allyEquipment = 0;
    };

    inline bool Matched(const Setup& a, const Setup& b) {
        if (a.weapon != b.weapon || a.shield != b.shield || a.opponentBase != b.opponentBase ||
            a.subjectEquipment != b.subjectEquipment || a.enemyEquipment != b.enemyEquipment ||
            a.allyEquipment != b.allyEquipment)
            return false;
        const std::array first{a.health, a.stamina, a.magicka, a.allyHealth, a.enemyHealth};
        const std::array second{b.health, b.stamina, b.magicka, b.allyHealth, b.enemyHealth};
        for (std::size_t i = 0; i < first.size(); ++i)
            if (!std::isfinite(first[i]) || !std::isfinite(second[i]) || std::abs(first[i] - second[i]) > .1f)
                return false;
        return a.health > 0 && a.allyHealth > 0 && a.enemyHealth > 0 &&
               resources::Stable(a.subjectStats, b.subjectStats) && resources::Stable(a.enemyStats, b.enemyStats) &&
               resources::Stable(a.allyStats, b.allyStats);
    }
}
