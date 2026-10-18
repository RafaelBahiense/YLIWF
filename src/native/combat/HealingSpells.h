#pragma once

#include "combat/CombatRoles.h"
#include "combat/HealingSelection.h"

namespace RE {
    class MagicCaster;
}

namespace mod::healing_spells {
    healing_rules::HealingBudget ReadBudget(RE::Actor* actor);
    float Range(RE::SpellItem* spell);
    std::vector<healing_rules::SpellOption> ReadOptions(RE::Actor* actor, RE::MagicCaster* caster,
                                                        std::span<RE::Actor* const> targets,
                                                        std::span<const combat_roles::HealingSpell> spells, bool trace);
}
