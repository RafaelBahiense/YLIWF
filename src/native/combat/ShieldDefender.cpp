#include "ShieldDefender.h"
#include "ShieldDefenderRules.h"
#include "CombatRoles.h"
#include "CombatOrders.h"
#include "adapters/Adapters.h"
#include "followers/ControllerRuntime.h"
#include "diagnostics/Debug.h"
#include <cmath>
#include <array>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace mod::shield_defender {
    namespace {
        using shield_defender_rules::Stance;
        std::array<RE::TESCombatStyle*, static_cast<std::size_t>(Stance::Count)> styles{};
        float elapsedSinceTick = 0;

        struct Control {
            bool requested = false, owned = false, yielded = false;
            RE::CombatController* controller = nullptr;  // Identity only; never dereference a retained controller.
            RE::TESCombatStyle* original = nullptr;
            Stance stance = Stance::Pressure;
            bool recovering = false, incomingMelee = false, counterOpportunity = false;
            bool blocking = false, bashing = false;
            std::uint32_t blockObservations = 0, bashObservations = 0;
            std::uint32_t loggedBlocks = 0, loggedBashes = 0;
            float stamina = 0, maximumStamina = 0;
            std::string status = "Waiting for combat";
            std::string loggedStatus;
        };

        std::mutex mutex;
        std::unordered_map<std::uint32_t, Control> controls;

        bool OneHanded(const RE::TESObjectWEAP* weapon) {
            return weapon && (weapon->IsOneHandedSword() || weapon->IsOneHandedDagger() || weapon->IsOneHandedAxe() ||
                              weapon->IsOneHandedMace());
        }

        bool Equipped(RE::Actor* actor) {
            const auto* right = actor->GetEquippedObject(false);
            const auto* left = actor->GetEquippedObject(true);
            const auto* shield = left ? left->As<RE::TESObjectARMO>() : nullptr;
            return OneHanded(right ? right->As<RE::TESObjectWEAP>() : nullptr) && shield && shield->IsShield();
        }

        bool NearbyOpponent(RE::Actor* defender, RE::Actor* enemy) {
            return enemy && enemy != defender && !enemy->IsDead() && !enemy->IsDisabled() && enemy->Is3DLoaded() &&
                   enemy->GetWorldspace() == defender->GetWorldspace() &&
                   (defender->GetWorldspace() || enemy->GetParentCell() == defender->GetParentCell());
        }

        bool IncomingMelee(RE::Actor* defender, RE::Actor* enemy, bool guarding) {
            if (!NearbyOpponent(defender, enemy))
                return false;
            const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
            const auto* right = enemy->GetEquippedObject(false);
            const auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
            // Casting and projectiles need separate threat detection. A melee
            // animation targeting someone else must not keep this shield raised.
            const bool melee = !weapon || (!weapon->IsBow() && !weapon->IsCrossbow() && !weapon->IsStaff());
            return shield_defender_rules::IncomingMelee({defender->GetPosition().GetDistance(enemy->GetPosition()),
                                                         target.get() == defender, enemy->IsAttacking(), melee},
                                                        guarding);
        }

        bool IncomingMelee(RE::Actor* defender, RE::CombatController* controller, bool guarding) {
            const auto target = defender->GetActorRuntimeData().currentCombatTarget.get();
            if (IncomingMelee(defender, target.get(), guarding))
                return true;
            auto* group = controller->combatGroup;
            if (!group)
                return false;
            // This executes at the engine combat-update boundary. Inspect only
            // this combat group's candidates, never scan every loaded actor.
            RE::BSReadLockGuard lock(group->lock);
            for (const auto& candidate : group->targets)
                if (auto enemy = candidate.targetHandle.get(); IncomingMelee(defender, enemy.get(), guarding))
                    return true;
            return false;
        }

        RE::TESCombatStyle* SelectStyle(RE::Actor* actor, RE::CombatController* controller, Control& control) {
            const bool blocking = actor->IsBlocking();
            const bool bashing = actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash;
            if (blocking && !control.blocking)
                ++control.blockObservations;
            if (bashing && !control.bashing)
                ++control.bashObservations;
            control.blocking = blocking;
            control.bashing = bashing;
            control.stamina = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
            control.maximumStamina = control.stamina - actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                                    RE::ActorValue::kStamina);
            control.recovering =
                shield_defender_rules::Recovering(control.recovering, control.stamina, control.maximumStamina);
            control.incomingMelee = IncomingMelee(actor, controller, shield_defender_rules::Guarding(control.stance));
            const auto target = actor->GetActorRuntimeData().currentCombatTarget.get();
            const auto* opponent = target.get();
            control.counterOpportunity =
                NearbyOpponent(actor, target.get()) &&
                shield_defender_rules::CounterOpportunity(actor->GetPosition().GetDistance(opponent->GetPosition()),
                                                          opponent->AsActorState()->actorState2.recoil != 0,
                                                          opponent->AsActorState()->actorState2.staggered);
            control.stance = shield_defender_rules::ChooseStance(control.recovering, control.incomingMelee,
                                                                 control.counterOpportunity);
            return styles[static_cast<std::size_t>(control.stance)];
        }

        bool Equip(RE::Actor* actor) {
            if (Equipped(actor))
                return true;
            // Preserve active attacks, blocks and casts. Never repeatedly queue equips.
            if (actor->IsAttacking() || actor->IsBlocking())
                return false;
            for (const auto hand :
                 {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                if (const auto* caster = actor->GetMagicCaster(hand);
                    caster && caster->state != RE::MagicCaster::State::kNone)
                    return false;

            auto* right = actor->GetEquippedObject(false);
            auto* left = actor->GetEquippedObject(true);
            auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
            auto* shield = left ? left->As<RE::TESObjectARMO>() : nullptr;
            const bool keepWeapon = OneHanded(weapon), keepShield = shield && shield->IsShield();
            if (!keepWeapon)
                weapon = nullptr;
            if (!keepShield)
                shield = nullptr;
            for (const auto& [item, entry] :
                 actor->GetInventory([](RE::TESBoundObject& form) { return form.IsWeapon() || form.IsArmor(); })) {
                if (!item || entry.first <= 0)
                    continue;
                if (auto* candidate = item->As<RE::TESObjectWEAP>(); !keepWeapon && OneHanded(candidate)) {
                    if (!weapon || candidate->GetAttackDamage() > weapon->GetAttackDamage() ||
                        (candidate->GetAttackDamage() == weapon->GetAttackDamage() &&
                         candidate->GetFormID() < weapon->GetFormID()))
                        weapon = candidate;
                }
                if (auto* candidate = item->As<RE::TESObjectARMO>();
                    !keepShield && candidate && candidate->IsShield()) {
                    if (!shield || candidate->armorRating > shield->armorRating ||
                        (candidate->armorRating == shield->armorRating && candidate->GetFormID() < shield->GetFormID()))
                        shield = candidate;
                }
            }
            auto* manager = RE::ActorEquipManager::GetSingleton();
            const auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
            const auto* rightSlot =
                defaults
                    ? defaults->GetObject<RE::BGSEquipSlot>(RE::BGSDefaultObjectManager::DefaultObject::kRightHandEquip)
                    : nullptr;
            if (!weapon || !shield || !manager || !rightSlot)
                return false;
            if (!keepWeapon)
                manager->EquipObject(actor, weapon, nullptr, 1, rightSlot, false, false, false, true);
            if (actor->GetEquippedObject(true) != shield)
                manager->EquipObject(actor, shield, nullptr, 1, nullptr, false, false, false, true);
            const bool equipped = Equipped(actor);
            debug::Trace("ShieldDefender.Equip", actor, "weapon={:08X} shield={:08X} equipped={}", weapon->GetFormID(),
                         shield->GetFormID(), equipped);
            return equipped;
        }

        std::vector<RE::Actor*> Followers() {
            std::vector<RE::Actor*> result;
            if (const auto* quest = controller::detail::GetContext().quest)
                for (const auto* alias : quest->aliases)
                    if (alias && (alias->aliasID == 0 || (alias->aliasID >= 2 && alias->aliasID <= 8)))
                        if (const auto* reference = skyrim_cast<const RE::BGSRefAlias*>(alias))
                            if (auto* actor = reference->GetActorReference();
                                actor && !adapters::Owns(actor->GetFormID()))
                                result.push_back(actor);
            return result;
        }

        std::string Prepare(RE::Actor* actor) {
            if (std::ranges::any_of(styles, [](const auto* style) { return style == nullptr; }))
                return "Shield-defender style unavailable; install the matching ESP";
            auto* ui = RE::UI::GetSingleton();
            const auto* state = actor->AsActorState();
            const auto* process = actor->GetActorRuntimeData().currentProcess;
            const auto* interrupt =
                process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
            const bool busy = controller::storage::Get().active || !controller::storage::Get().queue.empty();
            const bool incapacitated = !actor->Is3DLoaded() || actor->IsDisabled() || state->IsBleedingOut() ||
                                       state->IsUnconscious() || state->IsSwimming() || actor->IsOnMount() ||
                                       state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
                                       state->actorState2.staggered ||
                                       state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
                                       actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kParalysis) > 0;
            const auto* faction = controller::detail::GetContext().currentFaction;
            const bool recruited = faction && actor->IsInFaction(faction) && actor->IsPlayerTeammate();
            if (!shield_defender_rules::CanDefend(
                    recruited, actor->IsInCombat(), combat_orders::Enabled.load(),
                    actor->GetCurrentScene() || busy || (ui && ui->IsMenuOpen("Dialogue Menu")),
                    actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) > 0, incapacitated,
                    interrupt != nullptr))
                return "Ordinary behavior: not fighting or another order controls this follower";
            if (!Equip(actor))
                return "Waiting for a one-handed weapon and shield to be equipped";
            return {};
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        for (std::size_t i = 0; i < styles.size(); ++i)
            styles[i] = data
                            ? data->LookupForm<RE::TESCombatStyle>(static_cast<RE::FormID>(0xA17 + i), info::PluginFile)
                            : nullptr;
    }

    void UpdateCombatStyle(RE::Actor* actor) {
        if (!actor)
            return;
        std::lock_guard lock(mutex);
        const auto found = controls.find(actor->GetFormID());
        if (found == controls.end())
            return;
        auto& control = found->second;
        auto* current = actor->GetActorRuntimeData().combatController;
        if (current != control.controller) {
            control.controller = current;
            control.owned = control.yielded = false;
            control.original = nullptr;
            control.recovering = control.incomingMelee = control.counterOpportunity = false;
            control.blocking = control.bashing = false;
            control.stance = Stance::Pressure;
        }
        if (!current)
            return;
        const auto* process = actor->GetActorRuntimeData().currentProcess;
        const bool foreign = process && process->middleHigh && process->middleHigh->runOncePackage.package;
        const bool requested = control.requested && !control.yielded && !combat_orders::Enabled &&
                               !actor->GetCurrentScene() && !foreign && Equipped(actor);
        auto* selected = requested ? SelectStyle(actor, current, control) : nullptr;
        const bool ours = current->combatStyle && std::ranges::find(styles, current->combatStyle) != styles.end();
        using shield_defender_rules::StyleAction;
        switch (shield_defender_rules::DecideStyle(requested, control.owned, ours,
                                                   current->combatStyle == control.original,
                                                   current->combatStyle == selected)) {
            case StyleAction::Acquire:
                control.original = current->combatStyle;
                control.owned = true;
                [[fallthrough]];
            case StyleAction::Reapply:
                current->combatStyle = selected;
                control.status = shield_defender_rules::Description(control.stance);
                break;
            case StyleAction::Restore:
                current->combatStyle = control.original;
                [[fallthrough]];
            case StyleAction::Release:
                if (requested && control.owned)
                    control.yielded = true;
                control.owned = false;
                if (control.yielded)
                    control.status = "Another combat-style override has priority";
                break;
            case StyleAction::None:
                break;
        }
    }

    void Tick(float elapsed) {
        if (!std::isfinite(elapsed) || elapsed <= 0)
            return;
        elapsedSinceTick += elapsed;
        if (elapsedSinceTick < 1)
            return;
        elapsedSinceTick = 0;
        std::unordered_set<std::uint32_t> active;
        for (auto* actor : Followers()) {
            if (actor->IsDead() || combat_roles::Read(actor) != combat_roles::Role::ShieldDefender)
                continue;
            active.insert(actor->GetFormID());
            auto status = Prepare(actor);
            std::lock_guard lock(mutex);
            auto& control = controls[actor->GetFormID()];
            control.requested = status.empty();
            if (!status.empty())
                control.status = std::move(status);
            if (control.loggedStatus != control.status) {
                control.loggedStatus = control.status;
                debug::Trace("ShieldDefender.Status", actor,
                             "{} stamina={} maximum={} incomingMelee={} counterOpportunity={}", control.status,
                             control.stamina, control.maximumStamina, control.incomingMelee,
                             control.counterOpportunity);
            }
            if (control.loggedBlocks != control.blockObservations || control.loggedBashes != control.bashObservations) {
                control.loggedBlocks = control.blockObservations;
                control.loggedBashes = control.bashObservations;
                debug::Trace("ShieldDefender.Actions", actor, "observedBlockStarts={} observedBashStarts={}",
                             control.blockObservations, control.bashObservations);
            }
        }
        std::lock_guard lock(mutex);
        for (auto& [actor, control] : controls)
            if (!active.contains(actor))
                control.requested = false;
    }

    bool NeedsObservation() {
        const auto followers = Followers();
        if (std::ranges::any_of(followers, [](auto* actor) {
                return !actor->IsDead() && combat_roles::Read(actor) == combat_roles::Role::ShieldDefender;
            }))
            return true;
        std::lock_guard lock(mutex);
        return std::ranges::any_of(controls, [](const auto& entry) { return entry.second.requested; });
    }

    void Invalidate() {
        std::lock_guard lock(mutex);
        controls.clear();
        elapsedSinceTick = 0;
    }

    std::string Status(std::uint32_t actor) {
        std::lock_guard lock(mutex);
        const auto found = controls.find(actor);
        return found == controls.end() ? "Waiting for shield-defender observation" : found->second.status;
    }
}
