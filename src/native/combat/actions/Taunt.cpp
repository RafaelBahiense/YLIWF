#include "Taunt.h"
#include "combat/ShieldDefender.h"
#include "combat/integrations/TargetPreference.h"
#include "ModInfo.h"

namespace mod::combat_actions::taunt {
    namespace {
        std::uint32_t subject = 0, generation = 0, reportedSubject = 0;
        taunt_rules::State state;
        RE::SpellItem* challenge = nullptr;
        bool effectPresent = false;

        bool Applied(RE::Actor* actor) {
            auto* effects = actor ? actor->AsMagicTarget()->GetActiveEffectList() : nullptr;
            if (!effects)
                return false;
            for (const auto* effect : *effects)
                if (effect && effect->spell == challenge && effect->caster.get() &&
                    effect->caster.get()->GetFormID() == subject &&
                    !effect->flags.any(RE::ActiveEffect::Flag::kInactive))
                    return true;
            return false;
        }

        bool Usable(RE::Actor* enemy, RE::Actor* actor) {
            return enemy && enemy != actor && !enemy->IsPlayerRef() && !enemy->IsPlayerTeammate() && !enemy->IsDead() &&
                   !enemy->IsDisabled() && enemy->Is3DLoaded() && enemy->IsInCombat() && !enemy->GetCurrentScene() &&
                   !enemy->AsActorState()->IsBleedingOut() && enemy->GetParentCell() == actor->GetParentCell() &&
                   enemy->GetPosition().GetDistance(actor->GetPosition()) <= 1000 && enemy->IsHostileToActor(actor);
        }
    }

    void Configure() {
        target_preference::Install();
        auto* data = RE::TESDataHandler::GetSingleton();
        challenge = data ? data->LookupForm<RE::SpellItem>(0xA1D, info::PluginFile) : nullptr;
    }

    void Begin(std::uint32_t actor, std::uint32_t token) {
        Clear();
        subject = actor;
        reportedSubject = actor;
        generation = token;
    }

    void Update(RE::Actor* actor, const shield_defender::Observation& defender, float elapsed, bool live) {
        if (!actor || actor->GetFormID() != subject || !generation)
            return;
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
            target_preference::Publish(0, 0);
            return;
        }
        auto* enemy = RE::TESForm::LookupByID<RE::Actor>(state.enemy);
        effectPresent = Applied(enemy);
        const auto target = enemy ? enemy->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
        state.Observe(elapsed, live && Usable(enemy, actor), effectPresent, target.get() == actor);
        if (live && !defender.targeting && !defender.recovering && state.phase == taunt_rules::Phase::Idle &&
            state.cooldown <= 0 && challenge && target_preference::Available()) {
            enemy = RE::TESForm::LookupByID<RE::Actor>(defender.threat);
            auto victim = enemy ? enemy->GetActorRuntimeData().currentCombatTarget.get() : nullptr;
            const bool ally = victim && victim.get() != actor && (victim->IsPlayerRef() || victim->IsPlayerTeammate());
            auto* values = actor->AsActorValueOwner();
            const auto maximumHealth =
                values->GetPermanentActorValue(RE::ActorValue::kHealth) +
                actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
            const auto health = values->GetActorValue(RE::ActorValue::kHealth);
            const taunt_rules::Opportunity opportunity{live && Usable(enemy, actor),
                                                       ally,
                                                       defender.threatVisible,
                                                       enemy && enemy->IsHostileToActor(actor),
                                                       defender.recovering,
                                                       defender.distance,
                                                       defender.stamina,
                                                       defender.maximumStamina,
                                                       maximumHealth > 0 ? health / maximumHealth : 0};
            auto* caster = taunt_rules::Eligible(opportunity)
                               ? actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)
                               : nullptr;
            if (caster && state.Request(enemy->GetFormID())) {
                values->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina,
                                          -taunt_rules::Cost);
                caster->CastSpellImmediate(challenge, false, enemy, 1.0f, false, 0, actor);
                effectPresent = Applied(enemy);
                // Ask the normal planner to reconsider once; never restart combat.
                enemy->UpdateCombatControllerSettings();
            }
        }
        target_preference::Publish(state.phase != taunt_rules::Phase::Idle && effectPresent ? state.enemy : 0, subject);
    }

    void End(std::uint32_t actor, std::uint32_t token) {
        if (subject == actor && generation == token) {
            target_preference::Publish(0, 0);
            state.Release();
            effectPresent = false;
            subject = generation = 0;
        }
    }

    void Clear() {
        target_preference::Clear();
        subject = generation = reportedSubject = 0;
        state = {};
        effectPresent = false;
    }

    Observation Inspect(std::uint32_t actor) {
        return actor && actor == reportedSubject
                   ? Observation{state, challenge && target_preference::Available(), effectPresent,
                                 target_preference::Rejections(), challenge ? challenge->GetFormID() : 0}
                   : Observation{};
    }
}
