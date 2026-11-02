#include "DefenderObservation.h"
#include "DefenderCastingRules.h"
#include "HealingGeometry.h"
#include "integrations/Precision.h"

namespace mod::defender_observation {
    namespace {
        bool Hostile(const RE::MagicItem* spell) {
            return spell && (spell->IsHostile() || std::ranges::any_of(spell->effects, [](const auto* effect) {
                                 return effect && effect->baseEffect && effect->baseEffect->IsHostile();
                             }));
        }
    }

    std::vector<defender_tactics::Threat> Threats(RE::Actor* actor) {
        using defender_tactics::Kind;
        std::vector<RE::ActorHandle> handles;
        auto* player = RE::PlayerCharacter::GetSingleton();
        for (auto* member : {actor, static_cast<RE::Actor*>(player)}) {
            auto* controller = member ? member->GetActorRuntimeData().combatController : nullptr;
            auto* group = controller ? controller->combatGroup : nullptr;
            if (!group)
                continue;
            RE::BSReadLockGuard lock(group->lock);
            for (const auto& target : group->targets) {
                if (handles.size() >= 16)
                    break;
                if (std::ranges::find(handles, target.targetHandle) == handles.end())
                    handles.push_back(target.targetHandle);
            }
        }
        if (auto current = actor->GetActorRuntimeData().currentCombatTarget.get();
            current && std::ranges::find(handles, current->GetHandle()) == handles.end())
            handles.insert(handles.begin(), current->GetHandle());
        std::vector<defender_tactics::Threat> result;
        for (const auto& handle : handles) {
            const auto enemy = handle.get();
            if (!enemy || enemy.get() == actor || enemy->IsDead() || enemy->IsDisabled() || !enemy->Is3DLoaded() ||
                enemy->IsPlayerRef() || enemy->IsPlayerTeammate() || enemy->GetWorldspace() != actor->GetWorldspace() ||
                (!actor->GetWorldspace() && enemy->GetParentCell() != actor->GetParentCell()))
                continue;
            defender_tactics::Threat threat;
            threat.actor = enemy->GetFormID();
            threat.distance = actor->GetPosition().GetDistance(enemy->GetPosition());
            if (threat.distance > 2400)
                continue;
            const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
            threat.self = target.get() == actor;
            threat.ally = target && (target.get() == player || target->IsPlayerTeammate());
            if (!threat.self && !threat.ally)
                continue;
            threat.heading = actor->GetHeadingAngle(enemy->GetPosition(), false);
            const auto sight = healing_geometry::Check(actor, enemy.get());
            threat.visible = sight.available && sight.clear;
            const auto attack = enemy->AsActorState()->GetAttackState();
            threat.attacking = enemy->IsAttacking() && threat.visible;
            threat.windup = threat.visible &&
                            (attack == RE::ATTACK_STATE_ENUM::kDraw || attack == RE::ATTACK_STATE_ENUM::kNextAttack);
            threat.opening = enemy->AsActorState()->actorState2.recoil || enemy->AsActorState()->actorState2.staggered;
            threat.readable = sight.available;
            const auto* right = enemy->GetEquippedObject(false);
            const auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
            if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                threat.kind = Kind::Bow;
            } else if (!weapon || !weapon->IsStaff()) {
                threat.kind = Kind::Melee;
                const auto* process = enemy->GetActorRuntimeData().currentProcess;
                const auto data = process && process->high ? process->high->attackData : nullptr;
                if (data && data->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack))
                    threat.kind = Kind::HeavyMelee;
                const auto precision = precision::Observe(enemy.get());
                if (precision.reach > 0)
                    threat.reach = precision.reach + 96;
            }
            for (const auto hand :
                 {RE::MagicSystem::CastingSource::kRightHand, RE::MagicSystem::CastingSource::kLeftHand}) {
                const auto* caster = enemy->GetMagicCaster(hand);
                const auto* equipped = enemy->GetEquippedObject(hand == RE::MagicSystem::CastingSource::kLeftHand);
                const auto* spell = caster && Hostile(caster->currentSpell) ? caster->currentSpell
                                    : equipped                              ? equipped->As<RE::SpellItem>()
                                                                            : nullptr;
                if (!Hostile(spell))
                    continue;
                threat.kind = spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration
                                  ? Kind::Concentration
                                  : Kind::Projectile;
                if (spell->GetDelivery() == RE::MagicSystem::Delivery::kTargetLocation ||
                    std::ranges::any_of(spell->effects,
                                        [](const auto* effect) { return effect && effect->effectItem.area > 0; }))
                    threat.kind = Kind::Area;
                const auto activity =
                    defender_casting_rules::Observe(caster && caster->currentSpell == spell,
                                                    caster ? static_cast<std::uint32_t>(caster->state.get()) : 0);
                threat.attacking = activity == defender_casting_rules::Activity::Casting && threat.visible;
                threat.windup = activity == defender_casting_rules::Activity::Preparing && threat.visible;
                break;
            }
            result.push_back(threat);
        }
        return result;
    }
}
