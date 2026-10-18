#pragma once

#include "combat/HealingForecast.h"
#include "combat/HealingRules.h"

namespace mod::healing_rules {
    // Indexes identify inputs, never engine objects. Empty target effects mean
    // the spell cannot heal that target (for example, a failed spell condition).
    struct SpellOption {
        float cost = 0, charge = 0, range = 0;
        bool self = false, castable = false;
        std::vector<std::vector<HealingEffect>> targetEffects;
    };

    struct HealingChoice {
        std::size_t spell = 0, target = 0;
        float throughput = 0;
    };

    inline std::optional<std::size_t> HealingTarget(const SpellOption& spell, std::span<const Target> targets,
                                                    std::uint32_t healer, bool combat) {
        std::optional<std::size_t> selected;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (i >= spell.targetEffects.size() || spell.targetEffects[i].empty())
                continue;
            const bool eligible =
                spell.self ? SelfEligible(targets[i], healer, combat) : Seekable(targets[i], healer, combat);
            if (eligible && (!selected || MoreUrgent(targets[i], targets[*selected])))
                selected = i;
        }
        return selected;
    }

    inline bool PreferHealingChoice(const HealingChoice& candidate, const HealingChoice& previous,
                                    std::span<const SpellOption> spells, std::span<const Target> targets,
                                    std::uint32_t healer, bool combat) {
        const auto& spell = spells[candidate.spell];
        const auto& oldSpell = spells[previous.spell];
        if (spell.self != oldSpell.self)
            return spell.self;
        if (candidate.target != previous.target)
            return MoreUrgent(targets[candidate.target], targets[previous.target]);

        if (!spell.self) {
            const bool reachable = Eligible(targets[candidate.target], healer, spell.range, combat);
            const bool oldReachable = Eligible(targets[previous.target], healer, oldSpell.range, combat);
            if (reachable != oldReachable)
                return reachable;
            if (!reachable && spell.range != oldSpell.range)
                return spell.range > oldSpell.range;
        }
        if (candidate.throughput != previous.throughput)
            return candidate.throughput > previous.throughput;
        return spell.cost < oldSpell.cost;
    }

    inline std::optional<HealingChoice> SelectHealing(std::span<const SpellOption> spells,
                                                      std::span<const Target> targets, std::uint32_t healer,
                                                      bool combat, const HealingBudget& budget, float recovery) {
        std::optional<HealingChoice> selected;
        for (std::size_t i = 0; i < spells.size(); ++i) {
            const auto& spell = spells[i];
            if (!spell.castable || spell.cost > budget.magicka)
                continue;
            const auto target = HealingTarget(spell, targets, healer, combat);
            if (!target)
                continue;
            const float missingHealth = targets[*target].maximumHealth - targets[*target].health;
            const float throughput = HealingPerSecond(spell.targetEffects[*target], spell.cost, spell.charge, recovery,
                                                      budget, missingHealth);
            if (throughput <= 0)
                continue;
            const HealingChoice candidate{i, *target, throughput};
            if (!selected || PreferHealingChoice(candidate, *selected, spells, targets, healer, combat))
                selected = candidate;
        }
        return selected;
    }
}
