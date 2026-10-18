#include "combat/HealingSpells.h"
#include "diagnostics/Debug.h"

namespace mod::healing_spells {
    namespace {
        float GameSetting(const char* name) {
            auto* settings = RE::GameSettingCollection::GetSingleton();
            auto* setting = settings ? settings->GetSetting(name) : nullptr;
            const float value = setting ? setting->GetFloat() : 0;
            return std::isfinite(value) ? std::max(0.0f, value) : 0;
        }

        std::vector<healing_rules::HealingEffect> ReadEffects(RE::SpellItem* spell, RE::Actor* caster,
                                                              RE::Actor* target, float strength) {
            using Archetype = RE::EffectArchetypes::ArchetypeID;
            using Flag = RE::EffectSetting::EffectSettingData::Flag;
            std::vector<healing_rules::HealingEffect> effects;
            for (auto* effect : spell->effects) {
                if (!effect || !effect->baseEffect)
                    continue;
                auto* base = effect->baseEffect;
                if (base->data.primaryAV != RE::ActorValue::kHealth || base->IsDetrimental() || base->IsHostile() ||
                    base->data.flags.any(Flag::kRecover) || effect->GetMagnitude() <= 0 ||
                    (base->data.archetype != Archetype::kValueModifier &&
                     base->data.archetype != Archetype::kDualValueModifier) ||
                    !effect->conditions.IsTrue(target, caster) || !base->conditions.IsTrue(target, caster))
                    continue;
                // Recover effects temporarily modify an attribute. Non-recover
                // duration effects restore it each second:
                // https://ck.uesp.net/wiki/Magic_Effect#Flags
                const float duration =
                    base->data.flags.any(Flag::kNoDuration) ? 0.0f : static_cast<float>(effect->GetDuration());
                effects.push_back({effect->GetMagnitude() * strength, duration});
            }
            return effects;
        }
    }

    healing_rules::HealingBudget ReadBudget(RE::Actor* actor) {
        auto* values = actor->AsActorValueOwner();
        const float current = values->GetActorValue(RE::ActorValue::kMagicka);
        const float maximum =
            current - actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka);
        // NPC actor-value estimate; no player-only combat multiplier. The pure
        // forecast accounts for casting and the game's regeneration delay.
        const float regeneration = maximum * values->GetActorValue(RE::ActorValue::kMagickaRate) *
                                   values->GetActorValue(RE::ActorValue::kMagickaRateMult) / 10000;
        return {current, maximum, regeneration, GameSetting("fMagickaRegenDelayMax")};
    }

    float Range(RE::SpellItem* spell) {
        // SPIT range is authored in feet, unlike actor world coordinates.
        return healing_rules::SpellRange(spell->GetRange(), spell->GetDelivery() == RE::MagicSystem::Delivery::kTouch);
    }

    std::vector<healing_rules::SpellOption> ReadOptions(RE::Actor* actor, RE::MagicCaster* caster,
                                                        std::span<RE::Actor* const> targets,
                                                        std::span<const combat_roles::HealingSpell> spells,
                                                        bool trace) {
        std::vector<healing_rules::SpellOption> result;
        result.reserve(spells.size());
        for (const auto& spell : spells) {
            healing_rules::SpellOption option;
            option.cost = spell.cost;
            option.charge = spell.spell->GetChargeTime();
            option.range = Range(spell.spell);
            option.self = spell.self;
            option.targetEffects.resize(targets.size());
            auto reason = RE::MagicSystem::CannotCastReason::kOK;
            float strength = 1;
            option.castable = caster->CheckCast(spell.spell, false, &strength, &reason, false);
            if (option.castable) {
                for (std::size_t i = 0; i < targets.size(); ++i) {
                    auto* target = targets[i];
                    if (!target || target->IsDead() || target->IsDisabled() || !target->Is3DLoaded() ||
                        (spell.self && target != actor) || (!spell.self && target == actor))
                        continue;
                    option.targetEffects[i] = ReadEffects(spell.spell, actor, target, strength);
                    if (trace)
                        debug::Trace("Healer.SpellTarget", actor, "spell={:08X} target={:08X} conditions={}",
                                     spell.spell->GetFormID(), target->GetFormID(), !option.targetEffects[i].empty());
                }
            }
            result.push_back(std::move(option));
        }
        return result;
    }
}
