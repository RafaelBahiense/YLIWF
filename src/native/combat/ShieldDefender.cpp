#include "followers/QuestPolicies.h"
#include "ShieldDefender.h"
#include "actions/Taunt.h"
#include "actions/Evade.h"
#include "ShieldDefenderRules.h"
#include "DefenderActions.h"
#include "CombatRoles.h"
#include "CombatOrders.h"
#include "DefenderObservation.h"
#include "DefenderMovement.h"
#include "DefenderTuning.h"
#include "ProjectileObservation.h"
#include "ProjectileRules.h"
#include "integrations/Precision.h"
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
            Clock::time_point lastDecision{};
            Observation observation;
            std::size_t movementSlot = defender_movement::Slots;
            float movingFor = 0, movementRest = 0, movementRefresh = 0;
            projectile_rules::QuietWindow flightWindow;
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

        void ObserveTactics(RE::Actor* actor, Control& control, float elapsed) {
            defender_movement::Sample(actor, elapsed);
            const auto tuning = defender_tuning::TestScope.Read(actor->GetFormID());
            const bool blocking = actor->IsBlocking();
            const bool bashing = actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kBash;
            control.blockObservations += blocking && !control.blocking;
            control.bashObservations += bashing && !control.bashing;
            control.blocking = blocking;
            control.bashing = bashing;
            control.stamina = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
            control.maximumStamina = control.stamina - actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                                    RE::ActorValue::kStamina);
            control.recovering = shield_defender_rules::Recovering(
                control.recovering, control.stamina, control.maximumStamina, tuning.recoveryEnter, tuning.recoveryExit);
            auto threats = defender_observation::Threats(actor);
            const auto flight = projectile_observation::Read(actor);
            for (auto& threat : threats) {
                threat.flightKnown = flight.readable;
                for (std::uint32_t i = 0; i < flight.count; ++i)
                    if (flight.incoming[i].shooter == threat.actor) {
                        threat.incomingProjectile = true;
                        threat.impactSeconds = threat.impactSeconds < 0
                                                   ? flight.incoming[i].seconds
                                                   : std::min(threat.impactSeconds, flight.incoming[i].seconds);
                    }
            }
            const bool projectileGap = control.flightWindow.Update(flight.readable, flight.count != 0, elapsed);
            const bool closing = control.observation.moving &&
                                 (control.observation.tactic.motion == defender_tactics::Motion::Close ||
                                  control.observation.tactic.motion == defender_tactics::Motion::Intercept ||
                                  control.observation.tactic.motion == defender_tactics::Motion::AdvanceLateral);
            const auto decision = defender_tactics::Choose(
                {control.requested, control.recovering, defender_movement::Ready(control.movementSlot),
                 control.stamina, control.maximumStamina, threats, closing,
                 control.observation.moving && control.observation.tactic.motion == defender_tactics::Motion::Retreat,
                 control.observation.threat, projectileGap});
            control.lastDecision = Clock::now();
            auto& observation = control.observation;
            observation.available = true;
            observation.flightReadable = flight.readable;
            observation.incomingProjectiles = flight.count;
            observation.projectilesExamined = flight.examined;
            observation.projectileGap = projectileGap;
            observation.tactic = decision;
            observation.threat = decision.target;
            observation.threats = static_cast<std::uint32_t>(threats.size());
            observation.incoming = false;
            observation.committed = decision.guard;
            observation.recovering = control.recovering;
            observation.counter = decision.intent == defender_tactics::Intent::Counter;
            observation.stamina = control.stamina;
            observation.maximumStamina = control.maximumStamina;
            observation.distance = -1;
            observation.targeting = false;
            observation.threatKind = 0;
            observation.threatVisible = observation.threatAttacking = observation.threatPreparing = false;
            for (const auto& threat : threats)
                if (threat.actor == decision.target) {
                    observation.distance = threat.distance;
                    observation.heading = threat.heading;
                    observation.targeting = threat.self;
                    observation.threatKind = static_cast<std::uint32_t>(threat.kind);
                    observation.threatVisible = threat.visible;
                    observation.threatAttacking = threat.attacking;
                    observation.threatPreparing = threat.windup;
                    observation.incoming = decision.guard && defender_tactics::Melee(threat.kind);
                }
            const auto precision = precision::Observe(actor);
            observation.weaponReach = precision.reach;
            observation.precisionAvailable = precision.available;
            observation.precisionCallbacks = precision.callbacks;
            observation.precisionIncoming = precision.incoming;
            observation.precisionOutgoing = precision.outgoing;
            control.incomingMelee = observation.incoming;
            control.counterOpportunity = observation.counter;
            control.stance = shield_defender_rules::ChooseStance(
                control.recovering, decision.guard, observation.counter,
                decision.offense && !observation.targeting && observation.threat != 0);
            observation.stance = static_cast<std::uint32_t>(control.stance);
            control.status = decision.reason;
            // Bound each locomotion intervention, then give ordinary combat AI
            // a handoff window. Never move the actor directly or retain a stale path.
            control.movementRest = std::max(0.0f, control.movementRest - elapsed);
            control.movementRefresh = std::max(0.0f, control.movementRefresh - elapsed);
            control.movingFor = observation.moving ? control.movingFor + elapsed : 0;
            observation.guardPreparing = defender_actions::Inspect(actor->GetFormID()).prepareGuardMovement;
            const bool expired = control.movingFor >= tuning.movementBurst;
            auto* target = RE::TESForm::LookupByID<RE::Actor>(decision.target);
            const bool displacing = decision.motion != defender_tactics::Motion::None;
            if (!displacing || expired || control.movementRest > 0 || actor->IsAttacking() ||
                combat_actions::evade::Busy(actor->GetFormID())) {
                defender_movement::Release(actor);
                observation.moving = false;
                if (expired)
                    control.movementRest = tuning.movementRest;
            } else if (!observation.moving || control.movementRefresh <= 0 || observation.guardPreparing) {
                if (!observation.moving)
                    ++observation.movementRequests;
                observation.moving =
                    defender_movement::Move(actor, target, control.movementSlot, decision.motion, threats,
                                            actor->IsBlocking() || observation.guardPreparing);
                control.movementRefresh = tuning.movementRefresh;
                if (!observation.moving) {
                    ++observation.movementFailures;
                    control.movementRest = tuning.movementRest;
                    defender_movement::Release(actor);
                }
            }
            observation.movement = defender_movement::Inspect(actor->GetFormID());
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
                    interrupt && !defender_movement::Owned(interrupt))) {
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
        defender_movement::Configure();
        precision::Configure();
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
        }
        if (!current)
            return;
        const auto* process = actor->GetActorRuntimeData().currentProcess;
        const bool foreign = process && process->middleHigh && process->middleHigh->runOncePackage.package &&
                             !defender_movement::Owned(process->middleHigh->runOncePackage.package);
        const bool fresh =
            control.observation.available && Clock::now() - control.lastDecision < std::chrono::milliseconds(300);
        const bool requested = fresh && control.requested && !control.yielded && !combat_orders::Enabled &&
                               !actor->GetCurrentScene() && !foreign && Equipped(actor);
        auto* selected = requested ? styles[static_cast<std::size_t>(control.stance)] : nullptr;
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
                // Tactical reason is published by the main-thread observation.
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
        if (elapsedSinceTick < 0.1f)
            return;
        const auto decisionElapsed = elapsedSinceTick;
        elapsedSinceTick = 0;
        std::unordered_set<std::uint32_t> active;
        for (auto* actor : Followers()) {
            if (actor->IsDead() || combat_roles::Read(actor) != combat_roles::Role::ShieldDefender) {
                defender_movement::Release(actor);
                continue;
            }
            const auto id = actor->GetFormID();
            active.insert(id);
            auto status = Prepare(actor);
            const auto policy = quest_policies::Inspect(actor);
            const bool allowed = !(policy.blocked & yliwf::policy::Bit(quest_policies::Action::Combat));
            Control next;
            {
                std::lock_guard lock(mutex);
                auto& shared = controls[id];
                if (shared.movementSlot == defender_movement::Slots)
                    for (std::size_t slot = 0; slot < defender_movement::Slots; ++slot)
                        if (std::ranges::none_of(controls, [slot, id](const auto& entry) {
                                return entry.first != id && entry.second.movementSlot == slot;
                            })) {
                            shared.movementSlot = slot;
                            break;
                        }
                next = shared;
            }
            next.requested = status.empty() && allowed && !next.yielded;
            // Queries, navigation and package changes can enter engine code.
            // Do them without the mutex used by the combat-worker style hook.
            if (next.requested)
                ObserveTactics(actor, next, decisionElapsed);
            else {
                defender_movement::Release(actor);
                next.observation.moving = false;
            }
            if (!status.empty())
                next.status = std::move(status);
            else if (!allowed)
                next.status = policy.reason[0] ? policy.reason : "Quest policy suspends shield defense";
            else if (next.yielded)
                next.status = "Another combat-style override has priority";
            if (next.loggedStatus != next.status) {
                next.loggedStatus = next.status;
                debug::Trace("ShieldDefender.Status", actor,
                             "{} stamina={} maximum={} incomingMelee={} counterOpportunity={}", next.status,
                             next.stamina, next.maximumStamina, next.incomingMelee, next.counterOpportunity);
            }
            if (next.loggedBlocks != next.blockObservations || next.loggedBashes != next.bashObservations) {
                next.loggedBlocks = next.blockObservations;
                next.loggedBashes = next.bashObservations;
                debug::Trace("ShieldDefender.Actions", actor, "observedBlockStarts={} observedBashStarts={}",
                             next.blockObservations, next.bashObservations);
            }
            bool yielded = false;
            {
                std::lock_guard lock(mutex);
                auto& shared = controls[id];
                // Preserve ownership changes made by the combat worker while
                // the main thread was collecting and applying its decision.
                next.controller = shared.controller;
                next.original = shared.original;
                next.owned = shared.owned;
                next.yielded = shared.yielded;
                next.observation.style = shared.observation.style;
                yielded = next.yielded;
                if (yielded) {
                    next.requested = false;
                    next.observation.moving = false;
                    next.status = "Another combat-style override has priority";
                }
                shared = std::move(next);
            }
            if (yielded)
                defender_movement::Release(actor);
        }
        std::vector<std::uint32_t> released;
        {
            std::lock_guard lock(mutex);
            for (auto& [actor, control] : controls)
                if (!active.contains(actor)) {
                    control.requested = false;
                    control.movementSlot = defender_movement::Slots;
                    released.push_back(actor);
                }
        }
        for (const auto actor : released)
            defender_movement::Release(RE::TESForm::LookupByID<RE::Actor>(actor));
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
        combat_actions::taunt::Clear();
        defender_actions::Invalidate();
        std::lock_guard lock(mutex);
        controls.clear();
        defender_movement::Clear();
        precision::Clear();
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
