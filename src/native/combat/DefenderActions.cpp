#include "DefenderActions.h"
#include "actions/Evade.h"
#include "actions/Taunt.h"
#include "DefenderActionRules.h"
#include "DefensiveBashRules.h"
#include "DefenderCastingRules.h"
#include "actions/Samples.h"
#include "actions/Dispatch.h"
#include "ShieldDefender.h"
#include "DefenderMovement.h"
#include "DefenderTuning.h"
#include "CombatRoles.h"
#include "CombatOrders.h"
#include "followers/QuestPolicies.h"
#include "followers/ControllerRuntime.h"
#include "diagnostics/Debug.h"
#include <chrono>
#include <mutex>
#include <utility>

namespace mod::defender_actions {
    namespace {
        using namespace defender_action_rules;
        using namespace combat_actions::guard_rules;
        using combat_actions::Dispatch;
        using combat_actions::SelectionRejected;
        using Clock = std::chrono::steady_clock;
        combat_actions::DispatchEvidence evidence;
        std::mutex mutex;
        combat_actions::BashExecution bashExecution;
        combat_actions::RightAttackExecution attackExecution;

        struct Lease {
            std::uint32_t actor = 0, token = 0, readyEpoch = 0;
            combat_actions::GuardExecution guard;
            bool closing = false, armed = true, fault = false;
            bool direct = false, directGuard = false, directBash = false, directAttack = false;
            bool dodgeDeferred = false, dodgeInterrupted = false, dodgeCleanupCompleted = false;
            bool attackDeferred = false, attackInterrupted = false, attackCleanupCompleted = false;
            bool recoverySuppression = false, safeRecoveryGap = false;
            bool casterBash = false;
            float cooldown = 0, bashCooldown = 0;
            MovementHandoff movementHandoff;
            RE::NiPoint3 previousPosition{};
            bool positionKnown = false;
            Clock::time_point lastTick{};
            Observation observation;
            defensive_bash_rules::Kind bashKind = defensive_bash_rules::Kind::Off;
            defensive_bash_rules::Kind bashCapability = defensive_bash_rules::Kind::Off;
            std::uint32_t bashIdle = 0, bashGate = 0;
        } lease;

        combat_actions::Receipt Send(RE::Actor* actor, combat_actions::Intent intent) {
            const auto receipt = combat_actions::Send(actor, lease.token, intent, lease.bashIdle, lease.bashGate);
            evidence = receipt.evidence;
            if (receipt.attempted) {
                lease.observation.dispatch = static_cast<std::uint32_t>(receipt.outcome);
                lease.observation.validationMask = evidence.validationMask;
                lease.observation.processFlags = evidence.flags;
                lease.observation.processResult = evidence.result;
                lease.observation.processReturned = evidence.seen ? static_cast<std::int32_t>(evidence.returned) : -1;
                if (intent == combat_actions::Intent::Guard || intent == combat_actions::Intent::RightAttack)
                    lease.observation.guardEvent = evidence.event;
            }
            return receipt;
        }

        bool ObserveAttack(RE::Actor* actor, float delta) {
            namespace rules = combat_actions::attack_rules;
            if (attackExecution.Phase() == rules::Phase::Idle)
                return false;
            auto sample = combat_actions::ReadAttack(actor);
            sample.interrupted |= combat_actions::ExternalActive();
            const auto result = attackExecution.Observe(sample, delta);
            lease.observation.attackGraphStarts += result.graphStart;
            lease.observation.attackGraphEnds += result.graphEnd;
            lease.observation.attackNativeStarts += result.nativeStart;
            switch (result.step) {
                case rules::Step::Observed:
                    ++lease.observation.rightAttackObserved;
                    lease.observation.rightAttackStatus = "Owned right-hand normal attack observed; awaiting cleanup.";
                    break;
                case rules::Step::Finished:
                    ++lease.observation.rightAttackCompleted;
                    lease.observation.attackRecoveryCompletions += result.recoveryCompletion;
                    lease.observation.rightAttackStatus = "Right-hand normal attack completed; cleanup confirmed.";
                    return false;
                case rules::Step::Interrupted:
                    ++lease.observation.rightAttackInterrupted;
                    lease.observation.rightAttackStatus =
                        "Right-hand attack interrupted or superseded; cleanup confirmed.";
                    return false;
                case rules::Step::Fault:
                    ++lease.observation.rightAttackTimeouts;
                    lease.guard.Fail();
                    lease.fault = true;
                    lease.observation.status = lease.observation.rightAttackStatus =
                        "Right-hand attack evidence unresolved; ownership retained. No replay.";
                    break;
                case rules::Step::None:
                    break;
            }
            return true;
        }

        bool ObserveBash(RE::Actor* actor, float delta) {
            namespace rules = combat_actions::bash_rules;
            if (bashExecution.Phase() == rules::Phase::Idle)
                return false;
            const auto result = bashExecution.Observe(combat_actions::ReadBash(actor), delta);
            if (result.graphStart)
                ++lease.observation.bashGraphStarts;
            if (result.graphEnd)
                ++lease.observation.bashGraphEnds;
            if (result.nativeStart)
                ++lease.observation.bashNativeStarts;
            switch (result.step) {
                case rules::Step::Observed:
                    ++lease.observation.nativeBashObserved;
                    if (lease.bashKind == defensive_bash_rules::Kind::Power)
                        ++lease.observation.powerBashObserved;
                    lease.observation.bashStatus = "Native defensive bash observed; awaiting completion.";
                    break;
                case rules::Step::Finished:
                case rules::Step::Interrupted:
                    if (result.step == rules::Step::Interrupted) {
                        ++lease.observation.nativeBashInterrupted;
                        if (lease.bashKind == defensive_bash_rules::Kind::Power)
                            ++lease.observation.powerBashInterrupted;
                        lease.observation.bashStatus =
                            "Native bash interrupted; cleanup confirmed and control returned.";
                    } else {
                        ++lease.observation.nativeBashCompleted;
                        if (lease.casterBash)
                            ++lease.observation.casterBashCompleted;
                        if (lease.bashKind == defensive_bash_rules::Kind::Power)
                            ++lease.observation.powerBashCompleted;
                        lease.observation.bashStatus = "Native defensive bash completed; control returned.";
                    }
                    return false;
                case rules::Step::Fault:
                    lease.guard.Fail();
                    lease.fault = true;
                    ++lease.observation.nativeBashTimeouts;
                    if (lease.bashKind == defensive_bash_rules::Kind::Power)
                        ++lease.observation.powerBashTimeouts;
                    lease.observation.status =
                        "Native bash acknowledgement/completion unconfirmed; no request replayed.";
                    lease.observation.bashStatus = lease.observation.status;
                    break;
                case rules::Step::None:
                    break;
            }
            return true;
        }

        void Publish(bool value) {
            combat_actions::PublishSuppression(value, lease.recoverySuppression && !lease.closing && !lease.fault,
                                               lease.safeRecoveryGap && !lease.closing && !lease.fault);
        }

        bool Managed(RE::Actor* actor) {
            const auto& context = controller::detail::GetContext();
            if (!actor || !context.quest || !context.quest->IsRunning() || !context.currentFaction ||
                !actor->IsInFaction(context.currentFaction))
                return false;
            for (auto* entry : context.quest->aliases) {
                if (!entry || !(entry->aliasID == 0 || (entry->aliasID >= 2 && entry->aliasID <= 8)))
                    continue;
                const auto* alias = skyrim_cast<const RE::BGSRefAlias*>(entry);
                if (alias && alias->GetActorReference() == actor)
                    return true;
            }
            return false;
        }

        void Pump() {
            lease.observation.prepareGuardMovement = false;
            lease.recoverySuppression = lease.safeRecoveryGap = false;
            if (!combat_actions::Active())
                return;
            if (lease.guard.Read().phase == Phase::Fault) {
                Publish(false);
                return;
            }
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(lease.actor);
            const auto now = Clock::now();
            const float delta = std::clamp(std::chrono::duration<float>(now - lease.lastTick).count(), 0.0f, 0.25f);
            lease.guard.Advance(delta);
            lease.cooldown = std::max(0.0f, lease.cooldown - delta);
            lease.bashCooldown = std::max(0.0f, lease.bashCooldown - delta);
            lease.lastTick = now;
            const auto defender = shield_defender::Inspect(lease.actor);
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            const auto* left = actor ? actor->GetEquippedObject(true) : nullptr;
            const auto* shield = left ? left->As<RE::TESObjectARMO>() : nullptr;
            const auto* right = actor ? actor->GetEquippedObject(false) : nullptr;
            const auto* weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
            const bool equipped = shield && shield->IsShield() && weapon &&
                                  (weapon->IsOneHandedSword() || weapon->IsOneHandedDagger() ||
                                   weapon->IsOneHandedAxe() || weapon->IsOneHandedMace());
            const auto* interrupt =
                process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
            const bool foreignPackage = interrupt && !defender_movement::Owned(interrupt);
            const bool valid =
                actor && !actor->IsDead() && !actor->IsDisabled() && actor->Is3DLoaded() && actor->IsInCombat() &&
                equipped && Managed(actor) && actor->IsPlayerTeammate() && !actor->GetCurrentScene() &&
                !foreignPackage && !actor->AsActorState()->IsBleedingOut() && !actor->AsActorState()->IsUnconscious() &&
                !actor->AsActorState()->actorState2.staggered && !actor->IsOnMount() &&
                actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) == 0 &&
                !combat_orders::Enabled && quest_policies::Allows(actor, quest_policies::Action::Combat) &&
                (lease.direct ? combat_roles::Read(actor) == combat_roles::Role::Automatic
                              : combat_roles::Read(actor) == combat_roles::Role::ShieldDefender && defender.requested &&
                                    defender.owned && !defender.yielded && defender.age >= 0 && defender.age <= 0.25f);
            const bool live = valid && !lease.closing && !lease.fault;
            const bool challengeEnabled = defender_tuning::TestScope.Read(lease.actor).challengeEnabled != 0;
            combat_actions::taunt::Update(actor, defender, delta, live && !lease.direct && challengeEnabled);
            lease.recoverySuppression = live && !lease.direct && defender.recovering;
            lease.safeRecoveryGap = lease.recoverySuppression && !defender.committed && defender.threatVisible &&
                                    !defender.threatAttacking && !defender.threatPreparing;
            if (!live)
                Publish(false);
            bool dodgeValue = false;
            const bool dodgeReadable = actor && actor->GetGraphVariableBool("bIsDodging", dodgeValue);
            const bool external = combat_actions::ExternalActive();
            const bool evading = combat_actions::evade::Update(
                actor, defender, delta, live && !lease.direct,
                lease.guard.Read().phase == Phase::Idle && !external &&
                    bashExecution.Phase() == combat_actions::bash_rules::Phase::Idle &&
                    attackExecution.Phase() == combat_actions::attack_rules::Phase::Idle && !lease.dodgeInterrupted);
            const bool dodging = ActiveDodge(dodgeReadable, dodgeValue);
            if (GuardInterrupted(lease.guard.Read().phase, dodging) && !lease.dodgeInterrupted) {
                lease.dodgeInterrupted = true;
                lease.dodgeCleanupCompleted = false;
                ++lease.observation.dodgeInterruptions;
            }
            if (GuardInterrupted(lease.guard.Read().phase, external) && !lease.attackInterrupted) {
                lease.attackInterrupted = true;
                lease.attackCleanupCompleted = false;
                ++lease.observation.attackInterruptions;
            }
            const bool guardOpportunity =
                live && !lease.observation.guardSuspended &&
                (lease.direct ? lease.directGuard
                              : defender.committed && defender.targeting && defender.distance >= 0 &&
                                    std::abs(defender.heading) <= 80);
            const bool wanted = guardOpportunity && !dodging && !evading && !lease.dodgeInterrupted && !external &&
                                !lease.attackInterrupted;
            float speed = -1;
            if (actor && lease.positionKnown && delta > 0)
                speed = actor->GetPosition().GetDistance(lease.previousPosition) / delta;
            if (actor) {
                lease.previousPosition = actor->GetPosition();
                lease.positionKnown = true;
            }
            lease.observation.locomotionSpeed = speed;
            const auto tuning = defender_tuning::TestScope.Read(lease.actor);
            lease.observation.locomotionReady =
                actor && GuardLocomotionReady(speed, actor->AsActorState()->IsSprinting(),
                                              actor->AsActorState()->IsWeaponDrawn(), tuning.guardSpeed);
            const auto guardPhase = lease.guard.Read().phase;
            lease.movementHandoff.Update(delta, live, lease.direct, wanted, lease.armed, lease.cooldown == 0,
                                         guardPhase, !lease.observation.locomotionReady, tuning.guardSettle);
            lease.observation.prepareGuardMovement = lease.movementHandoff.Preparing();
            const bool deferred = guardOpportunity && dodging && lease.guard.Read().phase == Phase::Idle;
            if (deferred && !lease.dodgeDeferred)
                ++lease.observation.dodgeDeferrals;
            lease.dodgeDeferred = deferred;
            const bool attackDeferred = guardOpportunity && external && lease.guard.Read().phase == Phase::Idle;
            if (attackDeferred && !lease.attackDeferred)
                ++lease.observation.attackDeferrals;
            lease.attackDeferred = attackDeferred;
            if (!wanted)
                lease.armed = true;
            if (lease.guard.Read().phase != Phase::Idle && lease.guard.Read().phase != Phase::Fault &&
                combat_actions::ForeignGuardEpoch() != lease.readyEpoch) {
                lease.guard.YieldOwnership();
                lease.armed = false;
                lease.dodgeInterrupted = lease.dodgeCleanupCompleted = false;
                lease.attackInterrupted = lease.attackCleanupCompleted = false;
                lease.observation.status = "Another block-start request took ownership; no release was sent.";
            }
            if (!actor || actor->IsDead()) {
                Publish(false);
                lease.guard.ActorUnavailable();
                lease.fault = true;
                lease.observation.status = "Actor unavailable; guard release needs review.";
                return;
            }
            if (ObserveAttack(actor, delta)) {
                Publish(!lease.fault && !dodging && !external);
                return;
            }
            if (ObserveBash(actor, delta)) {
                Publish(!lease.fault && !dodging && !external);
                return;
            }
            const auto sample = combat_actions::ReadGuard(actor);
            const bool blocking = sample.blocking;
            const bool blockReadable = sample.blockReadable;
            const bool reacting = sample.reactionReadable && sample.reacting;
            const bool intent = sample.IntentActive();
            lease.observation.blockReadable = blockReadable;
            lease.observation.wantsBlock = sample.wantsBlock;
            lease.observation.blockHitReadable = sample.reactionReadable;
            lease.observation.blockHit = sample.reacting;
            lease.observation.graphWantReadable = sample.graphIntentReadable;
            lease.observation.graphWantBlock = sample.graphIntent;
            const auto reaction = lease.guard.ObserveReaction(sample, wanted, delta);
            const bool beforeReleaseReaction = reaction.beforeRelease;
            const bool afterReleaseReaction = reaction.afterRelease;
            if (reaction.newlyDeferred)
                ++lease.observation.releaseDeferrals;
            if (beforeReleaseReaction || afterReleaseReaction)
                lease.observation.status =
                    beforeReleaseReaction
                        ? "Waiting for block-hit reaction to finish before releasing the owned guard."
                        : "Release accepted and intent clear; waiting for the block-hit reaction without replay.";
            lease.observation.releaseWait = lease.guard.Read().releaseWait;
            lease.observation.releaseWaitPeak =
                std::max(lease.observation.releaseWaitPeak, lease.guard.Read().releaseWait);
            if (!lease.closing &&
                ResumeAfterYield(lease.guard.Read().phase, lease.dodgeInterrupted, lease.dodgeCleanupCompleted,
                                 dodgeReadable, dodging, blockReadable, blocking, intent)) {
                lease.dodgeInterrupted = lease.dodgeCleanupCompleted = false;
                lease.observation.guardSuspended = false;
                lease.armed = true;
                ++lease.observation.dodgeResumes;
                if (lease.guard.TakeMissedStart())
                    ++lease.observation.recoveredInterruptTimeouts;
                lease.observation.status = "Dodge ended and guard cleanup confirmed; reevaluating current threat.";
                // wanted remains false for this tick. A later fresh snapshot must justify a new request.
            } else if (lease.guard.Read().phase == Phase::Idle && (deferred || lease.dodgeInterrupted)) {
                lease.observation.status = "Yielding native guard control until dodge and guard cleanup are clear.";
            }
            if (!lease.closing &&
                ResumeAfterYield(lease.guard.Read().phase, lease.attackInterrupted, lease.attackCleanupCompleted, true,
                                 external, blockReadable, blocking, intent)) {
                lease.attackInterrupted = lease.attackCleanupCompleted = false;
                lease.observation.guardSuspended = false;
                lease.armed = true;
                ++lease.observation.attackResumes;
                if (lease.guard.TakeMissedStart())
                    ++lease.observation.recoveredInterruptTimeouts;
                lease.observation.status =
                    "External attack ended and guard cleanup confirmed; reevaluating current threat.";
            } else if (lease.guard.Read().phase == Phase::Idle && (attackDeferred || lease.attackInterrupted)) {
                lease.observation.status = "Yielding native guard control to an external attack commitment.";
            }
            if (lease.directAttack && live && lease.guard.Read().phase == Phase::Idle && !blocking && !intent &&
                !reacting && !dodging && !external && !actor->IsAttacking() &&
                actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone &&
                actor->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal) {
                lease.directAttack = false;
                ++lease.observation.rightAttackRequests;
                const auto receipt =
                    attackExecution.Start(lease.actor, lease.token, lease.observation.rightAttackRequests,
                                          [&](combat_actions::Intent action) { return Send(actor, action); });
                if (receipt.outcome == Dispatch::Accepted) {
                    ++lease.observation.rightAttackAccepted;
                    lease.observation.rightAttackStatus = "Right-hand normal attack accepted; awaiting evidence.";
                } else if (receipt.outcome == Dispatch::Rejected) {
                    ++lease.observation.rightAttackRejected;
                    lease.observation.rightAttackStatus =
                        "Right-hand normal attack rejected; resolved event: " +
                        (evidence.event.empty() ? std::string("<none>") : evidence.event) + "; validation mask " +
                        std::to_string(evidence.validationMask) + "; " + evidence.rejection;
                } else {
                    lease.guard.Fail();
                    lease.fault = true;
                    lease.observation.status = lease.observation.rightAttackStatus =
                        "Right-hand dispatch unconfirmed; no request replayed.";
                }
                Publish(!lease.fault);
                return;
            }
            auto target = RE::TESForm::LookupByID<RE::Actor>(defender.threat);
            bool castingWindup = false;
            if (target && target->GetActorRuntimeData().currentCombatTarget.get().get() == actor &&
                defender.threatPreparing && defender.tactic.interrupt &&
                defender_tactics::Magic(static_cast<defender_tactics::Kind>(defender.threatKind)))
                for (const auto hand :
                     {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                    if (const auto* caster = target->GetMagicCaster(hand);
                        caster && caster->currentSpell &&
                        (caster->currentSpell->IsHostile() ||
                         std::ranges::any_of(caster->currentSpell->effects,
                                             [](const auto* effect) {
                                                 return effect && effect->baseEffect && effect->baseEffect->IsHostile();
                                             })) &&
                        defender_casting_rules::Observe(true, static_cast<std::uint32_t>(caster->state.get())) ==
                            defender_casting_rules::Activity::Preparing)
                        castingWindup = true;
            const bool windingUp =
                target && (target->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kDraw || castingWindup);
            auto* values = actor->AsActorValueOwner();
            const float bashStamina = values->GetActorValue(RE::ActorValue::kStamina);
            const float bashMaximum =
                values->GetPermanentActorValue(RE::ActorValue::kStamina) +
                actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
            const defensive_bash_rules::Opportunity bash{
                lease.bashCapability != defensive_bash_rules::Kind::Off,
                live,
                lease.guard.Read().phase == Phase::Holding,
                blockReadable && blocking,
                dodging || external || lease.dodgeInterrupted || lease.attackInterrupted,
                reacting,
                actor->IsAttacking(),
                windingUp && defender.targeting,
                defender.distance,
                defender.heading,
                bashStamina,
                bashMaximum,
                lease.bashCooldown,
                lease.bashKind,
                defender.recovering};
            const bool directBashReady = lease.directBash && live && lease.guard.Read().phase == Phase::Holding &&
                                         blockReadable && blocking && !reacting && !actor->IsAttacking() && !dodging &&
                                         !external && !lease.dodgeInterrupted && !lease.attackInterrupted;
            auto selectedBash = lease.bashKind;
            if (!lease.direct) {
                const bool powerEnabled =
                    lease.bashCapability == defensive_bash_rules::Kind::Power &&
                    defender.threatKind == static_cast<std::uint32_t>(defender_tactics::Kind::HeavyMelee);
                auto powerOpportunity = bash;
                powerOpportunity.kind = defensive_bash_rules::Kind::Power;
                auto* powerIdle = RE::TESForm::LookupByID<RE::TESIdleForm>(lease.bashIdle);
                auto* powerGate = RE::TESForm::LookupByID<RE::TESIdleForm>(lease.bashGate);
                selectedBash = defensive_bash_rules::Select(
                    bash, powerEnabled,
                    powerEnabled && defensive_bash_rules::CanRequest(powerOpportunity) && powerIdle && powerGate &&
                        powerGate->conditions.IsTrue(actor, nullptr) &&
                        powerIdle->CheckConditions(actor, nullptr, false));
            }
            if (lease.direct ? directBashReady : selectedBash != defensive_bash_rules::Kind::Off) {
                lease.bashKind = selectedBash;
                lease.directBash = false;
                ++lease.observation.nativeBashRequests;
                lease.casterBash = !lease.direct && castingWindup;
                if (lease.casterBash)
                    ++lease.observation.casterBashRequests;
                if (lease.bashKind == defensive_bash_rules::Kind::Power)
                    ++lease.observation.powerBashRequests;
                lease.bashCooldown = defensive_bash_rules::Cooldown;
                // Preserve idle/perk selection; resolved validation accepts only the requested bash kind.
                const bool power = lease.bashKind == defensive_bash_rules::Kind::Power;
                const auto receipt =
                    bashExecution.Start(lease.actor, lease.token, lease.observation.nativeBashRequests,
                                        power ? combat_actions::Intent::PowerBash : combat_actions::Intent::Bash,
                                        [&](combat_actions::Intent action) { return Send(actor, action); });
                const auto result = receipt.outcome;
                if (result == Dispatch::Rejected) {
                    ++lease.observation.nativeBashRejected;
                    if (lease.bashKind == defensive_bash_rules::Kind::Power)
                        ++lease.observation.powerBashRejected;
                    lease.observation.bashStatus =
                        evidence.rejection.empty()
                            ? "Bash rejected before dispatch; selected " +
                                  (evidence.event.empty() ? std::string("<none>") : evidence.event) +
                                  "; validation mask " + std::to_string(evidence.validationMask) +
                                  ". Guard observation continues."
                            : evidence.rejection;
                } else {
                    lease.armed = false;
                    if (lease.direct)
                        lease.directGuard = false;
                    if (result == Dispatch::Accepted) {
                        ++lease.observation.nativeBashAccepted;
                        if (lease.bashKind == defensive_bash_rules::Kind::Power)
                            ++lease.observation.powerBashAccepted;
                        lease.guard.YieldOwnership();
                        lease.observation.bashStatus = "Defensive bash accepted; guard handed off to bash observation.";
                    } else {
                        lease.guard.Fail();
                        lease.fault = true;
                        lease.observation.status = "Native bash dispatch unconfirmed; no request replayed.";
                        lease.observation.bashStatus = lease.observation.status;
                    }
                    Publish(!lease.fault);
                    return;
                }
            }
            const bool canStart = valid && lease.movementHandoff.Ready() && lease.cooldown == 0 &&
                                  !actor->IsAttacking() && blockReadable && !intent && !reacting && !dodging &&
                                  !lease.dodgeInterrupted && !external && !lease.attackInterrupted &&
                                  !actor->AsActorState()->actorState2.recoil &&
                                  actor->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
                                  actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone;
            const auto transition = lease.guard.Update(sample, wanted, canStart, lease.armed, dodging, reaction,
                                                       [&](combat_actions::Intent action) {
                                                           if (action == combat_actions::Intent::Guard)
                                                               lease.readyEpoch = combat_actions::ForeignGuardEpoch();
                                                           else if (lease.guard.Read().missedStart)
                                                               Publish(false);
                                                           return Send(actor, action);
                                                       });
            switch (transition.step) {
                case Step::Start:
                    lease.cooldown = 1.5f;
                    lease.armed = false;
                    ++lease.observation.guardRequests;
                    lease.dodgeInterrupted = lease.dodgeCleanupCompleted = false;
                    lease.attackInterrupted = lease.attackCleanupCompleted = false;
                    lease.observation.startRecovering = defender.recovering;
                    lease.observation.startStaminaFraction =
                        defender.maximumStamina > 0 ? defender.stamina / defender.maximumStamina : -1;
                    if (const auto result = transition.receipt.outcome; result == Dispatch::Accepted) {
                        ++lease.observation.guardAccepted;
                        lease.observation.status = "Shield block-start accepted; awaiting blocking state.";
                    } else if (result == Dispatch::Rejected) {
                        ++lease.observation.guardRejected;
                        lease.observation.status =
                            evidence.externalBlocked
                                ? "External attack superseded guard selection; request rejected before dispatch."
                            : (evidence.validationMask & SelectionRejected)
                                ? "No eligible block idle selected; request rejected before dispatch."
                                : "Shield block-start rejected; no guard is owned.";
                    } else {
                        // The dispatcher's false result does not expose whether
                        // downstream action work remains. Do not assume rejection
                        // or retry this request on a later threat episode.
                        lease.fault = true;
                        Publish(false);
                        lease.observation.status =
                            "Shield block-start dispatch unconfirmed; manual review required. No retry.";
                    }
                    break;
                case Step::Acknowledge:
                    ++lease.observation.guardObserved;
                    lease.observation.status = "Requested guard observed.";
                    break;
                case Step::Release:
                    if (transition.missedStart) {
                        ++lease.observation.timeouts;
                        lease.observation.guardSuspended = true;
                        Publish(false);
                    }
                    ++lease.observation.releaseRequests;
                    if (const auto dispatch = transition.receipt.outcome; dispatch == Dispatch::Accepted) {
                        ++lease.observation.releaseAccepted;
                        lease.observation.status = "Shield-release accepted; awaiting non-blocking state.";
                    } else if (dispatch == Dispatch::Rejected) {
                        ++lease.observation.releaseRejected;
                        lease.observation.status =
                            "Shield-release rejected; observing block intent and graph without retry.";
                    } else {
                        lease.fault = true;
                        lease.observation.status = "Shield-release dispatch unconfirmed; no request was replayed.";
                    }
                    break;
                case Step::Finished:
                    if (lease.dodgeInterrupted)
                        lease.dodgeCleanupCompleted = transition.cleanupConfirmed;
                    if (lease.attackInterrupted)
                        lease.attackCleanupCompleted = transition.cleanupConfirmed;
                    if (transition.releaseObserved)
                        ++lease.observation.releaseObserved;
                    if (lease.attackInterrupted)
                        lease.observation.status =
                            "Interrupted guard cleanup confirmed; waiting for external attack ownership.";
                    else if (lease.dodgeInterrupted)
                        lease.observation.status =
                            "Interrupted guard cleanup confirmed; waiting for dodge state to clear.";
                    else if (lease.guard.Read().missedStart)
                        lease.observation.status =
                            lease.guard.Read().releaseRejected
                                ? "Guard start unobserved; release rejected, no guard intent remains. Native guards "
                                  "suspended."
                                : "Guard start unobserved; release confirmed. Native guards suspended.";
                    else
                        lease.observation.status = lease.guard.Read().releaseRejected
                                                       ? "Release rejected; no guard intent remains."
                                                       : "Guard ended.";
                    break;
                case Step::Fault:
                    if (transition.timeout)
                        ++lease.observation.timeouts;
                    lease.fault = true;
                    Publish(false);
                    lease.observation.status =
                        beforeReleaseReaction  ? "Block-hit reaction did not finish within its observation window; no "
                                                 "release was sent. Review required."
                        : afterReleaseReaction ? "Block-hit reaction exceeded the release observation bound; release "
                                                 "was already dispatched. No replay."
                        : lease.guard.Read().releaseRejected
                            ? "Release rejected; block intent or graph still unresolved. Manual "
                              "review required. No retry."
                            : "Guard release unconfirmed; manual review required. No replay.";
                    break;
                case Step::None:
                    break;
            }
            const bool guardActive = blocking || lease.guard.Read().phase == Phase::StartPending ||
                                     lease.guard.Read().phase == Phase::Holding;
            Publish(evading ||
                    (lease.direct
                         ? live && !dodging && !external
                         : Suppress(valid && !lease.closing && !lease.fault, defender.recovering,
                                    !lease.observation.guardSuspended && defender.committed && guardActive, true,
                                    dodging || lease.dodgeInterrupted || external || lease.attackInterrupted)));
        }
    }

    void Install() {
        combat_actions::Install();
        combat_actions::taunt::Configure();
    }

    bool Begin(std::uint32_t actor, std::uint32_t token) {
        std::lock_guard lock(mutex);
        if (!actor || !token || combat_actions::Active() || !combat_actions::Available().installed ||
            !combat_actions::Available().process)
            return false;
        lease = {};
        defender_movement::ClearTestBounds();
        bashExecution.Reset();
        defender_tuning::TestScope.Begin(actor, token);
        combat_actions::evade::Reset();
        attackExecution.Reset();
        lease.actor = actor;
        lease.token = token;
        combat_actions::taunt::Begin(actor, token);
        lease.lastTick = Clock::now();
        combat_actions::Begin(actor, token);
        return true;
    }

    bool BeginDirect(std::uint32_t actor, std::uint32_t token) {
        if (!Begin(actor, token))
            return false;
        std::lock_guard lock(mutex);
        lease.direct = true;
        return true;
    }

    bool RequestDirect(std::uint32_t actor, std::uint32_t token, yliwf::testing::PrimitiveAction action) {
        std::lock_guard lock(mutex);
        if (!combat_actions::Active() || !lease.direct || lease.actor != actor || lease.token != token ||
            lease.closing || lease.fault || lease.directAttack || lease.directBash ||
            bashExecution.Phase() != combat_actions::bash_rules::Phase::Idle ||
            attackExecution.Phase() != combat_actions::attack_rules::Phase::Idle)
            return false;
        using Action = yliwf::testing::PrimitiveAction;
        const auto phase = lease.guard.Read().phase;
        switch (action) {
            case Action::RightAttack:
                if (phase != Phase::Idle || lease.directAttack)
                    return false;
                lease.directGuard = false;
                lease.directAttack = true;
                return true;
            case Action::Guard:
                if (phase != Phase::Idle || lease.observation.guardSuspended)
                    return false;
                lease.directGuard = lease.armed = true;
                return true;
            case Action::Release:
                if (phase != Phase::Holding && phase != Phase::StartPending)
                    return false;
                lease.directGuard = lease.directBash = false;
                return true;
            case Action::Bash:
            case Action::PowerBash:
                if (phase != Phase::Holding || lease.directBash ||
                    lease.bashKind != (action == Action::PowerBash ? defensive_bash_rules::Kind::Power
                                                                   : defensive_bash_rules::Kind::Normal))
                    return false;
                lease.directBash = true;
                return true;
        }
        return false;
    }

    std::uint32_t End(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase) {
        std::lock_guard lock(mutex);
        if (!combat_actions::Active()) {
            if (!savedPhase)
                return 1;
            if (!combat_actions::Available().installed || savedPhase > static_cast<std::uint32_t>(Phase::Fault))
                return 2;
            auto* actorRef = RE::TESForm::LookupByID<RE::Actor>(actor);
            if (!actorRef)
                return 2;
            const auto sample = combat_actions::ReadGuard(actorRef);
            const auto recovery =
                RestoreGuard(static_cast<Phase>(savedPhase),
                             !ReleaseCleared(sample.blockReadable, sample.blocking, sample.IntentActive()));
            if (recovery == Recovery::Finished)
                return 1;
            lease = {};
            lease.actor = actor;
            lease.token = token;
            lease.guard.Restore(static_cast<Phase>(savedPhase),
                                !ReleaseCleared(sample.blockReadable, sample.blocking, sample.IntentActive()));
            lease.fault = lease.guard.Read().phase == Phase::Fault;
            lease.readyEpoch = combat_actions::ForeignGuardEpoch();
            lease.observation.status =
                recovery == Recovery::Manual
                    ? "Loaded guard ownership is unverified; no new release was sent. Manual review required."
                    : "Observing the saved release without replaying it.";
            lease.lastTick = Clock::now();
            combat_actions::RestoreScope(actor, token);
        }
        if (lease.actor != actor || lease.token != token)
            return 2;
        lease.closing = true;
        combat_actions::taunt::End(actor, token);
        combat_actions::evade::End(actor, token);
        defender_tuning::TestScope.End(actor, token);
        defender_movement::ClearTestBounds();
        lease.directBash = lease.directGuard = lease.directAttack = false;
        if (bashExecution.Phase() != combat_actions::bash_rules::Phase::Idle)
            bashExecution.Teardown();
        attackExecution.Teardown();
        combat_actions::CloseExternal();
        Publish(false);
        if (lease.guard.Read().phase == Phase::Fault &&
            (lease.guard.Read().releaseCompleted || lease.guard.Read().releaseNotIssued)) {
            auto* actorRef = RE::TESForm::LookupByID<RE::Actor>(actor);
            if (actorRef && actorRef->Is3DLoaded()) {
                const auto sample = combat_actions::ReadGuard(actorRef);
                if (lease.guard.Reconcile(sample)) {
                    lease.fault = false;
                    lease.observation.blockReadable = true;
                    lease.observation.wantsBlock = false;
                    lease.observation.status =
                        lease.guard.Read().releaseNotIssued
                            ? "Observed guard rechecked after reaction wait; graph and block intent are clear. No "
                              "release was sent."
                            : "Completed release rechecked; graph and block intent are clear. No command replayed.";
                }
            }
        }
        Pump();
        if (lease.guard.Read().phase == Phase::Fault)
            return 2;
        if (lease.guard.Read().phase != Phase::Idle || combat_actions::evade::Busy(actor) ||
            bashExecution.Phase() != combat_actions::bash_rules::Phase::Idle ||
            attackExecution.Phase() != combat_actions::attack_rules::Phase::Idle)
            return 0;
        combat_actions::Clear();
        bashExecution.Reset();
        attackExecution.Reset();
        return 1;
    }

    std::uint32_t SavedPhase(std::uint32_t actor, std::uint32_t token) {
        std::lock_guard lock(mutex);
        return lease.actor == actor && lease.token == token
                   ? static_cast<std::uint32_t>((combat_actions::evade::Busy(actor) ||
                                                 bashExecution.Phase() != combat_actions::bash_rules::Phase::Idle ||
                                                 attackExecution.Phase() != combat_actions::attack_rules::Phase::Idle)
                                                    ? Phase::Fault
                                                    : lease.guard.Read().phase)
                   : 0;
    }

    bool SetDefensiveBash(std::uint32_t actor, std::uint32_t token, std::uint32_t kind, std::uint32_t idle,
                          std::uint32_t gate) {
        std::lock_guard lock(mutex);
        if (!combat_actions::Active() || lease.actor != actor || lease.token != token || lease.closing ||
            lease.guard.Read().phase != Phase::Idle ||
            bashExecution.Phase() != combat_actions::bash_rules::Phase::Idle ||
            attackExecution.Phase() != combat_actions::attack_rules::Phase::Idle ||
            kind > static_cast<std::uint32_t>(defensive_bash_rules::Kind::Power))
            return false;
        if (kind == static_cast<std::uint32_t>(defensive_bash_rules::Kind::Power)) {
            auto* selected = RE::TESForm::LookupByID<RE::TESIdleForm>(idle);
            auto* conditions = RE::TESForm::LookupByID<RE::TESIdleForm>(gate);
            if (!combat_actions::Available().process || !combat_actions::Available().selection ||
                !combat_actions::Available().resolved || !selected || !conditions || selected->parentIdle ||
                !defensive_bash_rules::AllowedEvent(selected->animEventName.c_str(), defensive_bash_rules::Kind::Power))
                return false;
        } else if (idle || gate)
            return false;
        lease.bashKind = static_cast<defensive_bash_rules::Kind>(kind);
        lease.bashCapability = lease.bashKind;
        lease.bashIdle = idle;
        lease.bashGate = gate;
        return true;
    }

    Observation Inspect(std::uint32_t actor) {
        std::lock_guard lock(mutex);
        Observation result;
        if (actor == lease.actor) {
            const auto counters = combat_actions::Inspect();
            result = lease.observation;
            result.mode = yliwf::testing::DefenderControl;
            result.phase = static_cast<std::uint32_t>(lease.guard.Read().phase);
            result.bashPhase = static_cast<std::uint32_t>(bashExecution.Phase());
            result.rightAttackPhase = static_cast<std::uint32_t>(attackExecution.Phase());
            result.fault = lease.fault;
            result.actions = counters.actions;
            result.attacks = counters.attacks;
            result.rejected = counters.rejected;
            result.engineRejected = counters.engineRejected;
            result.lastAction = counters.lastAction;
            result.blockRequests = counters.blockRequests;
            result.bashRequests = counters.bashRequests;
            result.handoffAttackRejections = counters.handoffAttackRejections;
            result.unknownLeftRequests = counters.unknownLeftRequests;
        }
        result.available = combat_actions::Available().installed;
        return result;
    }

    void Tick() {
        std::lock_guard lock(mutex);
        Pump();
    }

    bool HasWork() {
        std::lock_guard lock(mutex);
        return combat_actions::Active() && lease.guard.Read().phase != Phase::Fault;
    }

    void Invalidate() {
        std::lock_guard lock(mutex);
        defender_tuning::TestScope = {};
        combat_actions::evade::Reset();
        defender_movement::ClearTestBounds();
        combat_actions::Clear();
        bashExecution.Reset();
        attackExecution.Reset();
        lease = {};
    }

    bool SetMovementBounds(std::uint32_t actor, std::uint32_t token, std::uint32_t anchor, float radius) {
        std::lock_guard lock(mutex);
        if (!combat_actions::Active() || lease.actor != actor || lease.token != token || lease.closing || lease.fault)
            return false;
        return defender_movement::SetTestBounds(RE::TESForm::LookupByID<RE::Actor>(actor),
                                                RE::TESForm::LookupByID<RE::TESObjectREFR>(anchor), radius);
    }

    bool SetTuning(std::uint32_t actor, std::uint32_t token, const yliwf::testing::DefenderTuning* value) {
        std::lock_guard lock(mutex);
        auto* subject = RE::TESForm::LookupByID<RE::Actor>(actor);
        if (!value || !combat_actions::Active() || lease.actor != actor || lease.token != token || lease.closing ||
            lease.fault || !subject || subject->IsInCombat())
            return false;
        return defender_tuning::TestScope.Configure(actor, token, *value);
    }

    bool SetEvadeBackend(std::uint32_t actor, std::uint32_t token, const yliwf::testing::EvadeBackend* value) {
        std::lock_guard lock(mutex);
        auto* subject = RE::TESForm::LookupByID<RE::Actor>(actor);
        return value && combat_actions::Active() && lease.actor == actor && lease.token == token && !lease.closing &&
               !lease.fault && subject && !subject->IsInCombat() &&
               combat_actions::evade::Configure(actor, token, *value);
    }

    bool SetAttackCommitment(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence,
                             yliwf::testing::AttackCommitment phase) {
        using Commitment = yliwf::testing::AttackCommitment;
        switch (phase) {
            case Commitment::Clear:
                return combat_actions::SetExternal(actor, token, sequence, false);
            case Commitment::Active:
                if (combat_actions::SetExternal(actor, token, sequence, true)) {
                    Publish(false);
                    return true;
                }
                return false;
            case Commitment::Finishing:
                return combat_actions::FinishExternal(actor, token, sequence);
        }
        return false;
    }

    std::uint32_t AttackRequest(std::uint32_t actor, std::uint32_t token) {
        return attackExecution.Request(actor, token);
    }

    bool AttackAnimation(std::uint32_t actor, std::uint32_t token, std::uint32_t request,
                         yliwf::testing::AttackAnimationEvent event) {
        using Event = yliwf::testing::AttackAnimationEvent;
        using Target = combat_actions::AttackEvent;
        switch (event) {
            case Event::Started:
                return attackExecution.Record(actor, token, request, Target::Started);
            case Event::Ended:
                return attackExecution.Record(actor, token, request, Target::Ended);
            case Event::Interrupted:
                return attackExecution.Record(actor, token, request, Target::Interrupted);
            case Event::ConflictingStarted:
                return attackExecution.Record(actor, token, request, Target::ConflictingStarted);
            case Event::ConflictingEnded:
                return attackExecution.Record(actor, token, request, Target::ConflictingEnded);
        }
        return false;
    }

    std::uint32_t BashRequest(std::uint32_t actor, std::uint32_t token) {
        return bashExecution.Request(actor, token);
    }

    bool BashAnimation(std::uint32_t actor, std::uint32_t token, std::uint32_t request,
                       yliwf::testing::BashAnimationEvent event) {
        using Event = yliwf::testing::BashAnimationEvent;
        switch (event) {
            case Event::Started:
                return bashExecution.Record(actor, token, request, combat_actions::AnimationEvent::Started);
            case Event::Ended:
                return bashExecution.Record(actor, token, request, combat_actions::AnimationEvent::Ended);
            case Event::Interrupted:
                return bashExecution.Record(actor, token, request, combat_actions::AnimationEvent::Interrupted);
        }
        return false;
    }
}
