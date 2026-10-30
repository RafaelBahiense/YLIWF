#include "followers/QuestPolicies.h"
#include "ShieldDefender.h"
#include "ShieldDefenderRules.h"
#include "DefenderActions.h"
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
#include <chrono>

namespace mod::shield_defender {
    namespace {
        using shield_defender_rules::Stance;
        using Clock = std::chrono::steady_clock;
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
            RE::ActorHandle guardTarget;
            Clock::time_point lastThreat{};
            Clock::time_point lastDecision{};
            Observation observation;
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

        bool MeleeThreat(RE::Actor* defender, RE::Actor* enemy, bool guarding, bool requireAttack) {
            if (!NearbyOpponent(defender, enemy))
                return false;
            const auto target = enemy->GetActorRuntimeData().currentCombatTarget.get();
            const auto* right = enemy->GetEquippedObject(false);
            const auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
            // Casting and projectiles need separate threat detection. A melee
            // animation targeting someone else must not keep this shield raised.
            const bool melee = !weapon || (!weapon->IsBow() && !weapon->IsCrossbow() && !weapon->IsStaff());
            const auto attack = enemy->AsActorState()->GetAttackState();
            return shield_defender_rules::IncomingMelee(
                {defender->GetPosition().GetDistance(enemy->GetPosition()), target.get() == defender,
                 !requireAttack || enemy->IsAttacking(), melee,
                 attack == RE::ATTACK_STATE_ENUM::kDraw || attack == RE::ATTACK_STATE_ENUM::kNextAttack},
                guarding);
        }

        RE::Actor* IncomingMelee(RE::Actor* defender, RE::CombatController* controller, bool guarding) {
            const auto target = defender->GetActorRuntimeData().currentCombatTarget.get();
            if (MeleeThreat(defender, target.get(), guarding, true))
                return target.get();
            auto* group = controller->combatGroup;
            if (!group)
                return nullptr;
            // This executes at the engine combat-update boundary. Inspect only
            // this combat group's candidates, never scan every loaded actor.
            RE::BSReadLockGuard lock(group->lock);
            for (const auto& candidate : group->targets)
                if (auto enemy = candidate.targetHandle.get(); MeleeThreat(defender, enemy.get(), guarding, true))
                    return enemy.get();
            return nullptr;
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
            auto* incoming = IncomingMelee(actor, controller, shield_defender_rules::Guarding(control.stance));
            control.incomingMelee = incoming != nullptr;
            const auto now = Clock::now();
            control.lastDecision = now;
            if (incoming) {
                control.guardTarget = incoming->GetHandle();
                control.lastThreat = now;
            }
            const auto held = control.guardTarget.get();
            const float sinceThreat = std::chrono::duration<float>(now - control.lastThreat).count();
            const auto target = actor->GetActorRuntimeData().currentCombatTarget.get();
            auto* opponent = target.get();
            control.counterOpportunity =
                NearbyOpponent(actor, target.get()) &&
                shield_defender_rules::CounterOpportunity(actor->GetPosition().GetDistance(opponent->GetPosition()),
                                                          opponent->AsActorState()->actorState2.recoil != 0,
                                                          opponent->AsActorState()->actorState2.staggered);
            const bool committed =
                shield_defender_rules::KeepGuard(control.incomingMelee, MeleeThreat(actor, held.get(), true, false),
                                                 sinceThreat, control.counterOpportunity && !control.recovering);
            control.stance =
                shield_defender_rules::ChooseStance(control.recovering, committed, control.counterOpportunity);
            auto* threat = committed && held ? held.get() : opponent;
            auto& observation = control.observation;
            observation.available = true;
            observation.stance = static_cast<std::uint32_t>(control.stance);
            observation.threat = threat ? threat->GetFormID() : 0;
            observation.incoming = control.incomingMelee;
            observation.committed = committed;
            observation.recovering = control.recovering;
            observation.counter = control.counterOpportunity;
            observation.stamina = control.stamina;
            observation.maximumStamina = control.maximumStamina;
            observation.guardRemaining =
                committed ? std::max(0.0f, shield_defender_rules::GuardCommitment - sinceThreat) : 0;
            observation.distance =
                NearbyOpponent(actor, threat) ? actor->GetPosition().GetDistance(threat->GetPosition()) : -1;
            observation.heading = observation.distance >= 0 ? actor->GetHeadingAngle(threat->GetPosition(), false) : 0;
            observation.targeting = threat && threat->GetActorRuntimeData().currentCombatTarget.get().get() == actor;
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
                    interrupt != nullptr)) {
                if (!recruited)
                    return "Shield defense suspended: follower faction or teammate status is missing";
                if (!actor->IsInCombat())
                    return "Shield defense waiting for combat";
                if (combat_orders::Enabled.load())
                    return "Shield defense suspended by Stand down";
                if (actor->GetCurrentScene())
                    return "Shield defense suspended by a scene";
                if (busy)
                    return "Shield defense suspended while a follower command is pending";
                if (ui && ui->IsMenuOpen("Dialogue Menu"))
                    return "Shield defense suspended during dialogue";
                if (actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) > 0)
                    return "Shield defense suspended while waiting";
                if (incapacitated)
                    return "Shield defense unavailable: unloaded, incapacitated, swimming, mounted or staggered";
                return "Shield defense suspended by an interrupt package";
            }
            if (!Equip(actor))
                return "Waiting for a one-handed weapon and shield to be equipped";
            return {};
        }
    }

    void Configure() {
        defender_actions::Install();
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
            control.guardTarget = {};
            control.lastThreat = {};
            control.lastDecision = {};
            control.observation = {};
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
        control.observation.style = current->combatStyle ? current->combatStyle->GetFormID() : 0;
    }

    void Tick(float elapsed) {
        if (!std::isfinite(elapsed) || elapsed <= 0)
            return;
        defender_actions::Tick();
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
            const auto policy = quest_policies::Inspect(actor);
            const bool allowed = !(policy.blocked & yliwf::policy::Bit(quest_policies::Action::Combat));
            std::lock_guard lock(mutex);
            auto& control = controls[actor->GetFormID()];
            control.requested = status.empty() && allowed;
            if (!status.empty())
                control.status = std::move(status);
            else if (!allowed)
                control.status = policy.reason[0] ? policy.reason : "Quest policy suspends shield defense";
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
        if (defender_actions::HasWork())
            return true;
        const auto followers = Followers();
        if (std::ranges::any_of(followers, [](auto* actor) {
                return !actor->IsDead() && combat_roles::Read(actor) == combat_roles::Role::ShieldDefender;
            }))
            return true;
        std::lock_guard lock(mutex);
        return std::ranges::any_of(controls, [](const auto& entry) { return entry.second.requested; });
    }

    void Invalidate() {
        defender_actions::Invalidate();
        std::lock_guard lock(mutex);
        controls.clear();
        elapsedSinceTick = 0;
    }

    std::string Status(std::uint32_t actor) {
        std::lock_guard lock(mutex);
        const auto found = controls.find(actor);
        return found == controls.end() ? "Waiting for shield-defender observation" : found->second.status;
    }

    Observation Inspect(std::uint32_t actor) {
        std::lock_guard lock(mutex);
        const auto found = controls.find(actor);
        if (found == controls.end())
            return {};
        const auto& control = found->second;
        auto result = control.observation;
        result.requested = control.requested;
        result.owned = control.owned;
        result.yielded = control.yielded;
        result.reason = control.status;
        result.age = result.available ? std::chrono::duration<float>(Clock::now() - control.lastDecision).count() : -1;
        return result;
    }
}
