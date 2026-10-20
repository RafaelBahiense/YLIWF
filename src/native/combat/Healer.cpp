#include "followers/QuestPolicies.h"
#include "combat/Healer.h"
#include "combat/CombatOrders.h"
#include "combat/HealingRules.h"
#include "combat/HealingSpells.h"
#include "combat/HealingGeometry.h"
#include "combat/HealerPackages.h"
#include "combat/HealerMovement.h"
#include "combat/HealerMovementRules.h"
#include "combat/CombatRoles.h"
#include "adapters/Adapters.h"
#include "followers/ControllerRuntime.h"
#include "diagnostics/Debug.h"
#include "settings/Settings.h"

#include <unordered_map>
#include <unordered_set>

namespace mod::healer {
    namespace {
        struct Observation {
            RE::ActorHandle actor;
            std::size_t slot = healer_packages::Slots;
            float retrySeconds = 0;
            float diagnosticCooldown = 0;
            float castSeconds = 0, stalledSeconds = 0;
            float offensiveSeconds = 0;
            bool released = false;
            float releasedAt = 0, recoveryEstimate = 0.5f;
            healing_rules::EscapeAttempt escape;
            std::uint32_t spell = 0, target = 0;
            float cost = 0, magickaBefore = 0;
            std::optional<RE::NiPoint3> lastPosition;
            bool moving = false;
            bool combat = false;
            bool retreating = false;
            std::optional<RE::NiPoint3> retreatGoal;
            std::string status = "Waiting for combat";
        };

        std::unordered_map<std::uint32_t, Observation> observations;

        struct Reservation {
            std::uint32_t healer = 0;
            float seconds = 0;
        };

        std::unordered_map<std::uint32_t, Reservation> reservations;
        float sinceObservation = 0;

        std::vector<RE::Actor*> NativeParty() {
            std::vector<RE::Actor*> result;
            auto* quest = controller::detail::GetContext().quest;
            if (!quest)
                return result;
            for (auto* alias : quest->aliases) {
                auto* reference = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
                auto* actor = reference ? reference->GetActorReference() : nullptr;
                if (actor && !adapters::Owns(actor->GetFormID()) && std::ranges::find(result, actor) == result.end())
                    result.push_back(actor);
            }
            return result;
        }

        bool Available(RE::Actor* actor) {
            return actor && !actor->IsDead() && !actor->IsDisabled() && actor->Is3DLoaded();
        }

        bool CanControl(RE::Actor* actor) {
            if (!quest_policies::Allows(actor, quest_policies::Action::Combat))
                return false;
            auto* currentFaction = controller::detail::GetContext().currentFaction;
            auto* ui = RE::UI::GetSingleton();
            if (!Available(actor) || !actor->IsPlayerTeammate() || actor->GetCurrentScene() || actor->IsOnMount() ||
                (ui && ui->IsMenuOpen("Dialogue Menu")) || healer_packages::ForeignInterrupt(actor) ||
                !currentFaction || !actor->IsInFaction(currentFaction))
                return false;
            const auto* state = actor->AsActorState();
            if (state->IsBleedingOut() || state->IsUnconscious() || state->IsSwimming() ||
                state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
                actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kParalysis) > 0 ||
                actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) > 0)
                return false;
            return true;
        }

        bool CanAct(RE::Actor* actor) {
            if (!CanControl(actor) || actor->IsAttacking() || actor->IsBlocking())
                return false;
            const auto* state = actor->AsActorState();
            if (state->actorState2.staggered || state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal)
                return false;
            using Source = RE::MagicSystem::CastingSource;
            for (const auto source : {Source::kLeftHand, Source::kRightHand, Source::kOther, Source::kInstant}) {
                auto* caster = actor->GetMagicCaster(source);
                if (caster && caster->state != RE::MagicCaster::State::kNone)
                    return false;
            }
            return true;
        }

        std::vector<RE::Actor*> Allies(const std::vector<RE::Actor*>& party) {
            auto result = party;
            if (auto* player = RE::PlayerCharacter::GetSingleton())
                result.push_back(player);
            for (const auto& follower : adapters::Followers(false)) {
                using yliwf::sdk::State;
                if (follower.state.state != State::Following && follower.state.state != State::Waiting)
                    continue;
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(follower.actor);
                if (actor && std::ranges::find(result, actor) == result.end())
                    result.push_back(actor);
            }
            std::erase_if(result, [](const auto* actor) {
                return !healing_rules::AllowedTarget(combat_orders::Enabled.load(), actor->IsPlayerRef());
            });
            return result;
        }

        bool SameArea(RE::Actor* caster, RE::Actor* target) {
            return (caster->GetParentCell() && caster->GetParentCell() == target->GetParentCell()) ||
                   (caster->GetWorldspace() && caster->GetWorldspace() == target->GetWorldspace());
        }

        healing_rules::Target DescribeTarget(RE::Actor* caster, RE::Actor* target) {
            healing_rules::Target result;
            if (!target)
                return result;
            result.id = target->GetFormID();
            result.alive = !target->IsDead() && !target->IsDisabled();
            result.loaded = target->Is3DLoaded();
            result.sameArea = SameArea(caster, target);
            const auto reservation = reservations.find(result.id);
            result.reserved = reservation != reservations.end() && reservation->second.healer != caster->GetFormID();
            result.health = target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
            result.bleedingOut = target->AsActorState()->IsBleedingOut();
            result.maximumHealth = result.health - target->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kDamage,
                                                                                 RE::ActorValue::kHealth);
            result.distance = caster->GetPosition().GetDistance(target->GetPosition());
            if (result.alive && result.loaded && result.sameArea && result.distance <= healing_rules::SearchRange &&
                result.id != caster->GetFormID()) {
                result.friendly = (target->IsPlayerRef() || target->IsPlayerTeammate()) &&
                                  !caster->IsHostileToActor(target) && !target->IsHostileToActor(caster);
                bool unused = false;
                if (result.friendly) {
                    const auto sight = healing_geometry::Check(caster, target);
                    result.actorSight = caster->HasLineOfSight(target, unused);
                    result.geometryAvailable = sight.available;
                    result.visible = sight.available && sight.clear;
                    result.obstruction = sight.blocker;
                }
            }
            return result;
        }

        using healing_spells::Range;

        struct Choice {
            combat_roles::HealingSpell spell;
            std::size_t target = 0;
        };

        bool TraceTargets(RE::Actor* actor, const std::vector<healing_rules::Target>& targets,
                          const std::vector<combat_roles::HealingSpell>& spells, Observation& observation) {
            if (!debug::Logging.load() || observation.diagnosticCooldown > 0)
                return false;
            observation.diagnosticCooldown = 5;
            for (const auto& target : targets)
                debug::Trace(
                    "Healer.Target", actor,
                    "target={:08X} alive={} loaded={} friendly={} sameArea={} visible={} health={} maximum={} "
                    "bleedingOut={} distance={} reserved={} actorSight={} geometryAvailable={} obstruction={:08X}",
                    target.id, target.alive, target.loaded, target.friendly, target.sameArea, target.visible,
                    target.health, target.maximumHealth, target.bleedingOut, target.distance, target.reserved,
                    target.actorSight, target.geometryAvailable, target.obstruction);
            for (const auto& spell : spells) {
                debug::Trace("Healer.Spell", actor,
                             "spell={:08X} name={} cost={} rangeUnits={} authoredRangeFeet={} delivery={}",
                             spell.spell->GetFormID(), spell.spell->GetName(), spell.cost, Range(spell.spell),
                             spell.spell->GetRange(), static_cast<std::uint32_t>(spell.spell->GetDelivery()));
            }
            return true;
        }

        std::optional<Choice> ChooseCast(RE::Actor* actor, RE::MagicCaster* caster,
                                         const std::vector<RE::Actor*>& allies,
                                         const std::vector<healing_rules::Target>& targets,
                                         const std::vector<combat_roles::HealingSpell>& spells,
                                         Observation& observation, bool traceTargets) {
            const auto budget = healing_spells::ReadBudget(actor);
            const auto options = healing_spells::ReadOptions(actor, caster, allies, spells, traceTargets);
            const auto selected = healing_rules::SelectHealing(options, targets, actor->GetFormID(), observation.combat,
                                                               budget, observation.recoveryEstimate);
            if (!selected) {
                const bool affordable = std::ranges::any_of(spells, [&](const auto& spell) {
                    return std::isfinite(budget.magicka) && budget.magicka >= spell.cost;
                });
                observation.status = affordable ? "No injured ally in range with a usable healing spell"
                                                : "Insufficient magicka for an ally-healing spell";
                return std::nullopt;
            }
            const auto& spell = spells[selected->spell];
            if (traceTargets)
                debug::Trace("Healer.Selection", actor,
                             "target={:08X} spell={:08X} estimatedHPPerSecond={} window={} magicka={} "
                             "regeneration={} regenerationDelay={} recoveryEstimate={}",
                             targets[selected->target].id, spell.spell->GetFormID(), selected->throughput,
                             healing_rules::HealingWindow, budget.magicka, budget.regeneration,
                             budget.regenerationDelay, observation.recoveryEstimate);
            return Choice{spell, selected->target};
        }

        void Cast(RE::Actor* actor, RE::Actor* target, const combat_roles::HealingSpell& candidate,
                  Observation& observation) {
            observation.spell = candidate.spell->GetFormID();
            observation.target = target->GetFormID();
            observation.cost = candidate.cost;
            observation.magickaBefore = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka);
            observation.castSeconds = 0;
            observation.released = false;
            if (!healer_packages::Cast(actor, observation.slot, target, candidate.spell)) {
                observation.spell = 0;
                observation.retrySeconds = healing_rules::RetryBackoff;
                observation.status = "Animated casting package unavailable or blocked";
                return;
            }
            reservations[target->GetFormID()] = {actor->GetFormID(), healing_rules::CastTimeout};
            observation.moving = false;
            observation.status = "Casting healing spell";
            debug::Trace("Healer.CastRequested", actor, "target={:08X} spell={:08X} cost={}", observation.target,
                         observation.spell, observation.cost);
        }

        void CancelCast(RE::Actor* actor, Observation& observation) {
            if (observation.spell && !observation.released && healer_packages::Casting(actor, observation.slot))
                for (const auto source :
                     {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                    if (auto* caster = actor->GetMagicCaster(source);
                        caster && caster->currentSpell && caster->currentSpell->GetFormID() == observation.spell)
                        caster->InterruptCast(true);
            observation.spell = 0;
            observation.released = false;
            const auto reservation = reservations.find(observation.target);
            if (reservation != reservations.end() && reservation->second.healer == actor->GetFormID())
                reservations.erase(reservation);
            healer_packages::Release(actor);
        }

        bool MaintainCast(RE::Actor* actor, Observation& observation) {
            if (!observation.spell)
                return false;
            if (observation.released) {
                const bool ready = CanAct(actor);
                if (!ready && observation.castSeconds < healing_rules::CastTimeout) {
                    observation.status = "Waiting for casting animation to finish";
                    return true;
                }
                if (ready) {
                    observation.recoveryEstimate = std::max(0.0f, observation.castSeconds - observation.releasedAt);
                    observation.spell = 0;
                    observation.released = false;
                    healer_packages::Release(actor);
                } else {
                    observation.retrySeconds = healing_rules::RetryBackoff;
                    debug::Trace("Healer.RecoveryTimeout", actor, "spell={:08X} elapsed={}", observation.spell,
                                 observation.castSeconds);
                    CancelCast(actor, observation);
                }
                return false;
            }
            auto* target = RE::TESForm::LookupByID<RE::Actor>(observation.target);
            const auto position = actor->GetPosition();
            const auto threats = observation.combat ? healer_movement::Threats(actor, combat_orders::Enabled.load())
                                                    : std::vector<healer_movement::Point>{};
            const float enemyDistance = healer_movement::ThreatDistance({position.x, position.y, position.z}, threats);
            // A single-shot UseMagic procedure can finish before the hand
            // animation emits its release event. Keep that in-flight cast's
            // receipt rather than cancelling just because the package ended.
            bool handCast = false;
            for (const auto source :
                 {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                if (auto* caster = actor->GetMagicCaster(source);
                    caster && caster->currentSpell && caster->currentSpell->GetFormID() == observation.spell &&
                    caster->state != RE::MagicCaster::State::kNone)
                    handCast = true;
            std::string_view reason;
            if (!Available(target))
                reason = "target unavailable";
            else if (!SameArea(actor, target))
                reason = "target left area";
            else if (target->IsHostileToActor(actor) || actor->IsHostileToActor(target))
                reason = "target hostile";
            else if (!healer_packages::Casting(actor, observation.slot) && !handCast)
                reason = "casting package lost";
            else if (target != actor && healing_rules::InterruptForThreat(enemyDistance, observation.castSeconds))
                reason = "enemy too close";
            else if (observation.castSeconds >= healing_rules::CastTimeout)
                reason = "cast timeout";
            if (!reason.empty()) {
                const auto* package = actor->GetCurrentPackage();
                debug::Trace(
                    "Healer.CastCancelled", actor,
                    "target={:08X} spell={:08X} elapsed={} reason={} enemyDistance={} package={:08X} handCast={}",
                    observation.target, observation.spell, observation.castSeconds, reason, enemyDistance,
                    package ? package->GetFormID() : 0, handCast);
                CancelCast(actor, observation);
                observation.retrySeconds = healing_rules::RetryBackoff;
                observation.status = "Healing cast interrupted or timed out";
                return false;
            }
            return true;
        }

        bool Move(RE::Actor* actor, const RE::NiPoint3& destination, Observation& observation) {
            const auto position = actor->GetPosition();
            if (observation.moving && observation.lastPosition &&
                position.GetDistance(*observation.lastPosition) < 16 && position.GetDistance(destination) > 96)
                observation.stalledSeconds += 1;
            else
                observation.stalledSeconds = 0;
            observation.lastPosition = position;
            observation.moving = true;
            if (observation.stalledSeconds >= healing_rules::TravelTimeout) {
                observation.retrySeconds = healing_rules::RetryBackoff;
                observation.stalledSeconds = 0;
                healer_packages::Move(actor, observation.slot, position);
                observation.status = "Healing route stalled; holding position before retry";
                debug::Trace("Healer.TravelStalled", actor, "target={:08X}", observation.target);
                return false;
            }
            return healer_packages::Move(actor, observation.slot, destination);
        }

        void ResumeCombat(RE::Actor* actor, Observation& observation) {
            healer_packages::Release(actor);
            observation.moving = false;
            observation.retreating = false;
            observation.retreatGoal.reset();
            observation.escape.Reset();
            observation.lastPosition.reset();
            observation.stalledSeconds = 0;
        }

        bool Retreat(RE::Actor* actor, Observation& observation, float elapsed) {
            const auto threats = observation.combat ? healer_movement::Threats(actor, combat_orders::Enabled.load())
                                                    : std::vector<healer_movement::Point>{};
            const auto position = actor->GetPosition();
            const auto distance = healer_movement::ThreatDistance({position.x, position.y, position.z}, threats);
            observation.retreating = observation.combat && healing_rules::Retreating(observation.retreating, distance);
            if (!observation.retreating) {
                observation.retreatGoal.reset();
                observation.escape.Reset();
                return false;
            }
            if (healing_rules::AllowOffense(combat_orders::Enabled.load()) &&
                observation.escape.Failed(distance, elapsed)) {
                ResumeCombat(actor, observation);
                observation.offensiveSeconds = healing_rules::OffensiveInterval;
                observation.status = "Unable to escape pursuers; fighting back";
                debug::Trace("Healer.FightBack", actor, "enemyDistance={} duration={}", distance,
                             observation.offensiveSeconds);
                return true;
            }
            if (observation.retreatGoal) {
                const auto goal = *observation.retreatGoal;
                if (position.GetDistance(goal) < 96 ||
                    healer_movement::ThreatDistance({goal.x, goal.y, goal.z}, threats) < healing_rules::SafeDistance ||
                    !healing_rules::EscapeDirection({position.x, position.y, position.z}, {goal.x, goal.y, goal.z},
                                                    threats))
                    observation.retreatGoal.reset();
            }
            if (!observation.retreatGoal)
                if (const auto goal = healer_movement::Destination(actor, actor, healing_rules::MaximumRange, threats,
                                                                   healer_movement::Purpose::Retreat))
                    observation.retreatGoal = RE::NiPoint3{goal->x, goal->y, goal->z};
            observation.status = "Retreating until clear of close combat";
            if (observation.retreatGoal) {
                Move(actor, *observation.retreatGoal, observation);
                const auto* package = actor->GetCurrentPackage();
                debug::Trace("Healer.Retreat", actor,
                             "enemyDistance={} goalDistance={} owned={} package={:08X} x={} y={} z={} goalX={} "
                             "goalY={} goalZ={} staggered={} knockState={}",
                             distance, position.GetDistance(*observation.retreatGoal), healer_packages::Owned(actor),
                             package ? package->GetFormID() : 0, position.x, position.y, position.z,
                             observation.retreatGoal->x, observation.retreatGoal->y, observation.retreatGoal->z,
                             actor->AsActorState()->actorState2.staggered != 0,
                             static_cast<int>(actor->AsActorState()->GetKnockState()));
            } else {
                healer_packages::Move(actor, observation.slot, position);
                observation.status = "No safe retreat position on loaded navmesh";
                debug::Trace("Healer.RetreatBlocked", actor, "enemyDistance={}", distance);
            }
            return true;
        }

        void Heal(RE::Actor* actor, const std::vector<RE::Actor*>& allies, Observation& observation, float elapsed) {
            if (MaintainCast(actor, observation))
                return;
            auto spells = combat_roles::HealingSpells(actor);
            if (combat_orders::Enabled)
                // Area healing could also heal the player during a duel.
                std::erase_if(spells, [](const auto& spell) {
                    return std::ranges::any_of(
                        spell.spell->effects, [](const auto* effect) { return effect && effect->effectItem.area > 0; });
                });
            if (spells.empty()) {
                ResumeCombat(actor, observation);
                observation.status = "Needs a known fire-and-forget healing spell";
                return;
            }
            auto* caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
            if (!caster) {
                ResumeCombat(actor, observation);
                observation.status = "Spell caster unavailable";
                return;
            }
            std::vector<healing_rules::Target> targets;
            for (auto* ally : allies)
                targets.push_back(DescribeTarget(actor, ally));
            const bool traceTargets = TraceTargets(actor, targets, spells, observation);
            const auto choice = ChooseCast(actor, caster, allies, targets, spells, observation, traceTargets);
            if (!choice) {
                ResumeCombat(actor, observation);
                return;
            }
            // Self-healing is a stationary Self-target cast and does not need
            // separation from enemies. Preserve ongoing attacks/casts until ready.
            if (choice->spell.self && observation.retrySeconds <= 0 && CanAct(actor)) {
                Cast(actor, actor, choice->spell, observation);
                return;
            }
            if (observation.offensiveSeconds > 0) {
                ResumeCombat(actor, observation);
                observation.status = "Fighting pursuers before retrying healing";
                return;
            }
            // Retreat must not wait for an attack animation to finish, but only
            // runs while a usable healing action is needed.
            if (Retreat(actor, observation, elapsed))
                return;
            if (observation.retrySeconds > 0 || !CanAct(actor) || choice->spell.self) {
                ResumeCombat(actor, observation);
                observation.status = "Waiting for an uninterrupted casting opportunity or retry backoff";
                return;
            }
            const auto threats = observation.combat ? healer_movement::Threats(actor, combat_orders::Enabled.load())
                                                    : std::vector<healer_movement::Point>{};
            const auto position = actor->GetPosition();
            const float enemyDistance = healer_movement::ThreatDistance({position.x, position.y, position.z}, threats);
            if (healing_rules::SafeToStartCast(enemyDistance) &&
                healing_rules::Eligible(targets[choice->target], actor->GetFormID(), Range(choice->spell.spell),
                                        observation.combat)) {
                Cast(actor, allies[choice->target], choice->spell, observation);
                return;
            }
            auto* target = allies[choice->target];
            if (const auto destination = healer_movement::Destination(
                    actor, target, Range(choice->spell.spell), threats,
                    observation.combat ? healer_movement::Purpose::CombatSeek : healer_movement::Purpose::Seek)) {
                if (observation.target != target->GetFormID())
                    observation.stalledSeconds = 0;
                observation.target = target->GetFormID();
                observation.status = "Seeking an injured ally";
                if (Move(actor, {destination->x, destination->y, destination->z}, observation))
                    debug::Trace("Healer.Move", actor, "target={:08X} x={} y={} z={}", observation.target,
                                 destination->x, destination->y, destination->z);
                return;
            }
            observation.status = "No safe healing position on loaded navmesh";
            if (observation.combat)
                healer_packages::Move(actor, observation.slot, position);
            else
                healer_packages::Release(actor);
            observation.moving = false;
        }
    }

    void Configure() {
        healer_packages::Configure();
    }

    void Loaded() {
        // A saved interrupt package can outlive its process-local request.
        // Reconcile it before choosing a new target or rewriting package inputs.
        for (auto* actor : NativeParty())
            healer_packages::Release(actor);
        if (auto* processes = RE::ProcessLists::GetSingleton())
            for (const auto& handle : processes->highActorHandles)
                if (auto actor = handle.get())
                    healer_packages::Release(actor.get());
    }

    void Observe(std::uint32_t actorID) {
        std::scoped_lock lock(settings::Mutex);
        if (observations.contains(actorID))
            return;
        // A reference loaded later can carry an activity rank saved during
        // an earlier session. It is not a live native request.
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID))
            healer_packages::Release(actor);
    }

    void SpellReleased(std::uint32_t actorID, std::uint32_t spell) {
        std::scoped_lock lock(settings::Mutex);
        const auto found = observations.find(actorID);
        if (found == observations.end() || found->second.spell != spell || found->second.released)
            return;
        auto actor = found->second.actor.get();
        auto& observation = found->second;
        if (!actor || healer_packages::ForeignInterrupt(actor.get()))
            return;
        debug::Trace("Healer.Cast", actor.get(),
                     "target={:08X} spell={:08X} cost={} magickaBefore={} magickaAfter={} mode=UseMagic",
                     observation.target, spell, observation.cost, observation.magickaBefore,
                     actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka));
        observation.released = true;
        observation.releasedAt = observation.castSeconds;
        reservations[observation.target] = {actorID, healing_rules::TargetReservation};
        observation.status = "Healing spell released; waiting for casting animation";
        observation.moving = false;
    }

    void Suspend() {
        for (auto& [id, observation] : observations)
            if (auto actor = observation.actor.get())
                CancelCast(actor.get(), observation);
        Invalidate();
    }

    bool NeedsObservation() {
        if (!controller::storage::Available())
            return false;
        // Keep ticking until old role assignments have released their packages.
        if (!observations.empty())
            return true;
        const auto party = NativeParty();
        return std::ranges::any_of(party, [](RE::Actor* actor) {
            return actor && !actor->IsDead() && combat_roles::Read(actor) == combat_roles::Role::Healer;
        });
    }

    void Tick(float elapsed) {
        if (!controller::storage::Available() || !std::isfinite(elapsed) || elapsed <= 0)
            return;
        for (auto& [actor, observation] : observations) {
            observation.retrySeconds = healing_rules::Remaining(observation.retrySeconds, elapsed);
            observation.diagnosticCooldown = healing_rules::Remaining(observation.diagnosticCooldown, elapsed);
            observation.offensiveSeconds = healing_rules::Remaining(observation.offensiveSeconds, elapsed);
            if (observation.spell)
                observation.castSeconds += elapsed;
        }
        for (auto& [actor, reservation] : reservations)
            reservation.seconds = healing_rules::Remaining(reservation.seconds, elapsed);
        std::erase_if(reservations, [](const auto& entry) { return entry.second.seconds <= 0; });
        sinceObservation += elapsed;
        if (sinceObservation < 1)
            return;
        const float observationElapsed = sinceObservation;
        sinceObservation = 0;

        const auto party = NativeParty();
        const auto allies = Allies(party);
        const bool combat =
            std::ranges::any_of(allies, [](RE::Actor* actor) { return Available(actor) && actor->IsInCombat(); }) ||
            (RE::PlayerCharacter::GetSingleton() && RE::PlayerCharacter::GetSingleton()->IsInCombat());
        std::unordered_set<std::uint32_t> healers;
        for (auto* actor : party) {
            if (!actor || actor->IsDead() || combat_roles::Read(actor) != combat_roles::Role::Healer)
                continue;
            const auto id = actor->GetFormID();
            healers.insert(id);
            auto& observation = observations[id];
            observation.actor = actor->GetHandle();
            observation.combat = combat;
            if (!combat || !healing_rules::AllowOffense(combat_orders::Enabled.load()))
                observation.offensiveSeconds = 0;
            if (observation.slot == healer_packages::Slots)
                for (std::size_t slot = 0; slot < healer_packages::Slots; ++slot)
                    if (std::ranges::none_of(observations, [slot, id](const auto& entry) {
                            return entry.first != id && entry.second.slot == slot;
                        })) {
                        observation.slot = slot;
                        break;
                    }
            const auto previous = observation.status;
            if (controller::storage::Get().active || !controller::storage::Get().queue.empty()) {
                CancelCast(actor, observation);
                observation.moving = false;
                observation.status = "Waiting for the follower command to finish";
            } else if (!CanControl(actor)) {
                if (observation.spell)
                    debug::Trace("Healer.CastCancelled", actor,
                                 "target={:08X} spell={:08X} reason=controller safety guard elapsed={}",
                                 observation.target, observation.spell, observation.castSeconds);
                CancelCast(actor, observation);
                observation.moving = false;
                observation.status = "Healer control suspended by actor state, dialogue or another package";
            } else if (!healer_packages::Ready(observation.slot))
                observation.status = "Healer records unavailable; install the matching ESP";
            else
                Heal(actor, allies, observation, observationElapsed);
            if (observation.status != previous)
                debug::Trace("Healer.Status", actor, observation.status);
        }
        std::erase_if(observations, [&](auto& entry) {
            if (healers.contains(entry.first))
                return false;
            if (auto actor = entry.second.actor.get())
                CancelCast(actor.get(), entry.second);
            return true;
        });
    }

    void Invalidate() {
        // Serialization/menu callbacks only invalidate transient state. Saved
        // interrupt packages are reconciled on the main thread by Loaded().
        observations.clear();
        reservations.clear();
        sinceObservation = 0;
        healer_movement::Clear();
    }

    std::string Status(std::uint32_t actor) {
        const auto found = observations.find(actor);
        return found != observations.end() ? found->second.status : "Waiting for healer observation";
    }
}
