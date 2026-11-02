#include "diagnostics/Debug.h"
#include "CombatTestHost.h"
#include "followers/QuestPolicies.h"
#include "combat/CombatRoles.h"
#include "combat/CombatOrders.h"
#include "combat/ShieldDefender.h"
#include "combat/DefenderActions.h"
#include "combat/integrations/Precision.h"
#include "followers/ControllerRuntime.h"
#include "settings/Settings.h"
#include <cstdio>

namespace mod::debug {
    namespace {
        RE::BGSRefAlias* TestAlias(RE::Actor* actor) {
            auto* data = RE::TESDataHandler::GetSingleton();
            auto* quest = data ? data->LookupForm<RE::TESQuest>(0x750BA, "Skyrim.esm") : nullptr;
            if (!actor || !quest || !quest->IsRunning())
                return nullptr;
            for (auto* entry : quest->aliases) {
                if (!entry || !(entry->aliasID == 0 || (entry->aliasID >= 2 && entry->aliasID <= 8)))
                    continue;
                auto* alias = skyrim_cast<RE::BGSRefAlias*>(entry);
                if (alias && alias->GetActorReference() == actor)
                    return alias;
            }
            return nullptr;
        }

        std::uint32_t InspectTestSubject(std::uint32_t id, yliwf::testing::Subject* output) {
            if (!output || output->size < sizeof(*output))
                return 0;
            *output = {};
            if (!IsCurrentGame(Generation())) {
                std::snprintf(output->reason, sizeof(output->reason), "YLIWF is still loading follower state.");
                return 1;
            }
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            output->managed = TestAlias(actor) != nullptr;
            if (!output->managed) {
                std::snprintf(output->reason, sizeof(output->reason), "Select a core-managed follower.");
                return 1;
            }
            if (combat_orders::Enabled.load()) {
                std::snprintf(output->reason, sizeof(output->reason),
                              "Turn off Stand down before running combat tests.");
                return 1;
            }
            const auto role = combat_roles::Inspect(actor);
            output->role = static_cast<std::uint32_t>(role.assigned);
            std::snprintf(output->behavior, sizeof(output->behavior), "%s", role.behavior.c_str());
            for (std::size_t i = 0; i < role.options.size(); ++i)
                if (role.options[i].eligible)
                    output->eligibleRoles |= 1u << i;
            const auto policy = quest_policies::Inspect(actor);
            const auto required = yliwf::policy::Bit(yliwf::policy::Action::Follow) |
                                  yliwf::policy::Bit(yliwf::policy::Action::Movement) |
                                  yliwf::policy::Bit(yliwf::policy::Action::Combat);
            output->allowed = (role.available || role.needsLoad) && !(policy.blocked & required);
            if (!output->allowed)
                std::snprintf(output->reason, sizeof(output->reason), "%s",
                              (policy.blocked & required) ? policy.reason : role.unavailable.c_str());
            return 1;
        }

        std::uint32_t SetTestRole(std::uint32_t id, std::uint32_t role) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            auto* alias = TestAlias(actor);
            if (!alias || !combat_roles::Valid(static_cast<std::int32_t>(role)) ||
                !quest_policies::Allows(actor, yliwf::policy::Action::Combat))
                return 0;
            return combat_roles::Apply(id, alias->aliasID, static_cast<combat_roles::Role>(role), Generation());
        }

        std::uint32_t SetFollowing(std::uint32_t id, bool following, std::uint32_t expected) {
            std::scoped_lock lock(settings::Mutex);
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!IsCurrentGame(Generation()) || !TestAlias(actor))
                return 0;
            if (expected) {
                const auto& state = controller::storage::Get();
                const auto newer = [expected](auto request, auto operation) {
                    return request > 0 && static_cast<std::uint32_t>(request) > expected &&
                           operation != controller_rules::Operation::Sync;
                };
                if (state.active && newer(state.active->request, state.active->operation))
                    return 0;
                for (const auto& command : state.queue)
                    if (newer(command.id, command.operation))
                        return 0;
                for (const auto& receipt : state.receipts)
                    if (newer(receipt.request, receipt.operation))
                        return 0;
            }
            return controller::detail::Enqueue(
                following ? controller_rules::Operation::Follow : controller_rules::Operation::Wait, actor, -1, 0, 0);
        }

        yliwf::testing::CommandState InspectTestCommand(std::uint32_t request) {
            using State = yliwf::testing::CommandState;
            std::scoped_lock lock(settings::Mutex);
            if (!IsCurrentGame(Generation()) || !request)
                return State::Unknown;
            const auto& state = controller::storage::Get();
            if (state.active && static_cast<std::uint32_t>(state.active->request) == request)
                return State::Pending;
            for (const auto& command : state.queue)
                if (static_cast<std::uint32_t>(command.id) == request)
                    return State::Pending;
            for (const auto& receipt : state.receipts)
                if (static_cast<std::uint32_t>(receipt.request) == request)
                    return receipt.success ? State::Succeeded : State::Failed;
            return State::Unknown;
        }

        std::uint32_t ObserveDefender(std::uint32_t id, yliwf::testing::DefenderObservation* output) {
            if (!output || output->size < sizeof(*output) || !IsCurrentGame(Generation()))
                return 0;
            *output = {};
            const auto state = shield_defender::Inspect(id);
            output->available = state.available;
            output->requested = state.requested;
            output->owned = state.owned;
            output->yielded = state.yielded;
            output->stance = state.stance;
            output->style = state.style;
            output->threat = state.threat;
            output->incoming = state.incoming;
            output->committed = state.committed;
            output->recovering = state.recovering;
            output->counter = state.counter;
            output->targeting = state.targeting;
            output->distance = state.distance;
            output->heading = state.heading;
            output->stamina = state.stamina;
            output->maximumStamina = state.maximumStamina;
            output->guardRemaining = state.guardRemaining;
            output->age = state.age;
            output->intent = static_cast<std::uint32_t>(state.tactic.intent);
            output->motion = static_cast<std::uint32_t>(state.tactic.motion);
            output->threatKind = state.threatKind;
            output->threatCount = state.threats;
            output->threatVisible = state.threatVisible;
            output->threatAttacking = state.threatAttacking;
            output->threatPreparing = state.threatPreparing;
            output->moving = state.moving;
            output->movementStalls = state.movement.stalls;
            output->movementDetours = state.movement.detours;
            output->movementStatus = static_cast<std::uint32_t>(state.movement.status);
            output->guardPreparing = state.guardPreparing;
            output->destinationX = state.movement.x;
            output->destinationY = state.movement.y;
            output->destinationZ = state.movement.z;
            output->movementProgress = state.movement.progress;
            output->movementWait = state.movement.waiting;
            output->movementRequests = state.movementRequests;
            output->movementFailures = state.movementFailures;
            output->precisionAvailable = state.precisionAvailable;
            output->precisionCallbacks = state.precisionCallbacks;
            output->weaponReach = state.weaponReach;
            output->precisionIncoming = state.precisionIncoming;
            output->precisionOutgoing = state.precisionOutgoing;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            const auto contacts = precision::Observe(actor);
            output->precisionAvailable = contacts.available;
            output->precisionCallbacks = contacts.callbacks;
            output->weaponReach = contacts.reach;
            output->precisionIncoming = contacts.incoming;
            output->precisionOutgoing = contacts.outgoing;
            const auto* controller = actor ? actor->GetActorRuntimeData().combatController : nullptr;
            output->liveStyle = controller && controller->combatStyle ? controller->combatStyle->GetFormID() : 0;
            const auto actions = defender_actions::Inspect(id);
            output->actionAvailable = actions.available;
            output->actionMode = actions.mode;
            output->guardPhase = actions.phase;
            output->actionFault = actions.fault;
            output->actionRequests = actions.actions;
            output->attackRequests = actions.attacks;
            output->attacksRejected = actions.rejected;
            output->engineRejected = actions.engineRejected;
            output->guardRequests = actions.guardRequests;
            output->guardAccepted = actions.guardAccepted;
            output->guardObserved = actions.guardObserved;
            output->releaseRequests = actions.releaseRequests;
            output->releaseAccepted = actions.releaseAccepted;
            output->releaseObserved = actions.releaseObserved;
            output->actionTimeouts = actions.timeouts;
            output->lastAction = actions.lastAction;
            output->guardRejected = actions.guardRejected;
            output->dispatch = actions.dispatch;
            output->validationMask = actions.validationMask;
            output->processFlags = actions.processFlags;
            output->processResult = actions.processResult;
            output->processReturned = actions.processReturned;
            output->blockRequests = actions.blockRequests;
            output->bashRequests = actions.bashRequests;
            output->unknownLeftRequests = actions.unknownLeftRequests;
            output->guardSuspended = actions.guardSuspended;
            output->blockReadable = actions.blockReadable;
            output->wantsBlock = actions.wantsBlock;
            output->startRecovering = actions.startRecovering;
            output->startStaminaFraction = actions.startStaminaFraction;
            output->guardLocomotionSpeed = actions.locomotionSpeed;
            output->guardLocomotionReady = actions.locomotionReady;
            output->releaseRejected = actions.releaseRejected;
            output->dodgeDeferrals = actions.dodgeDeferrals;
            output->dodgeInterruptions = actions.dodgeInterruptions;
            output->dodgeResumes = actions.dodgeResumes;
            output->attackDeferrals = actions.attackDeferrals;
            output->attackInterruptions = actions.attackInterruptions;
            output->attackResumes = actions.attackResumes;
            output->releaseDeferrals = actions.releaseDeferrals;
            output->blockHitReadable = actions.blockHitReadable;
            output->blockHit = actions.blockHit;
            output->graphWantReadable = actions.graphWantReadable;
            output->graphWantBlock = actions.graphWantBlock;
            output->releaseWait = actions.releaseWait;
            output->releaseWaitPeak = actions.releaseWaitPeak;
            output->recoveredInterruptTimeouts = actions.recoveredInterruptTimeouts;
            output->nativeBashRequests = actions.nativeBashRequests;
            output->powerBashRequests = actions.powerBashRequests;
            output->nativeBashAccepted = actions.nativeBashAccepted;
            output->powerBashAccepted = actions.powerBashAccepted;
            output->nativeBashObserved = actions.nativeBashObserved;
            output->powerBashObserved = actions.powerBashObserved;
            output->nativeBashCompleted = actions.nativeBashCompleted;
            output->powerBashCompleted = actions.powerBashCompleted;
            output->nativeBashRejected = actions.nativeBashRejected;
            output->powerBashRejected = actions.powerBashRejected;
            output->nativeBashTimeouts = actions.nativeBashTimeouts;
            output->powerBashTimeouts = actions.powerBashTimeouts;
            output->handoffAttackRejections = actions.handoffAttackRejections;
            output->nativeBashInterrupted = actions.nativeBashInterrupted;
            output->powerBashInterrupted = actions.powerBashInterrupted;
            output->bashGraphStarts = actions.bashGraphStarts;
            output->bashGraphEnds = actions.bashGraphEnds;
            output->bashNativeStarts = actions.bashNativeStarts;
            output->bashPhase = actions.bashPhase;
            output->rightAttackRequests = actions.rightAttackRequests;
            output->rightAttackAccepted = actions.rightAttackAccepted;
            output->rightAttackObserved = actions.rightAttackObserved;
            output->rightAttackCompleted = actions.rightAttackCompleted;
            output->rightAttackInterrupted = actions.rightAttackInterrupted;
            output->rightAttackRejected = actions.rightAttackRejected;
            output->rightAttackTimeouts = actions.rightAttackTimeouts;
            output->rightAttackPhase = actions.rightAttackPhase;
            output->attackGraphStarts = actions.attackGraphStarts;
            output->attackGraphEnds = actions.attackGraphEnds;
            output->attackNativeStarts = actions.attackNativeStarts;
            output->attackRecoveryCompletions = actions.attackRecoveryCompletions;
            std::snprintf(output->rightAttackStatus, sizeof(output->rightAttackStatus), "%s",
                          actions.rightAttackStatus.c_str());
            std::snprintf(output->bashStatus, sizeof(output->bashStatus), "%s", actions.bashStatus.c_str());
            std::snprintf(output->guardEvent, sizeof(output->guardEvent), "%s", actions.guardEvent.c_str());
            std::snprintf(output->actionStatus, sizeof(output->actionStatus), "%s", actions.status.c_str());
            std::snprintf(output->reason, sizeof(output->reason), "%s", state.reason.c_str());
            return 1;
        }
    }

    namespace {
        std::uint32_t BeginDefense(std::uint32_t actor, std::uint32_t mode, std::uint32_t token) {
            if (!IsCurrentGame(Generation()) || !TestAlias(RE::TESForm::LookupByID<RE::Actor>(actor)))
                return 0;
            return defender_actions::Begin(actor, mode, token);
        }

        std::uint32_t BeginDirect(std::uint32_t actor, std::uint32_t token) {
            if (!IsCurrentGame(Generation()) || !TestAlias(RE::TESForm::LookupByID<RE::Actor>(actor)))
                return 0;
            return defender_actions::BeginDirect(actor, token);
        }

        std::uint32_t EndDefense(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase) {
            return IsCurrentGame(Generation()) ? defender_actions::End(actor, token, savedPhase) : 0;
        }
    }

    const yliwf::testing::Host* CombatTestHost() {
        static const yliwf::testing::Host host{sizeof(host),
                                               yliwf::testing::Version,
                                               InspectTestSubject,
                                               SetTestRole,
                                               ObserveDefender,
                                               BeginDefense,
                                               EndDefense,
                                               defender_actions::SavedPhase,
                                               SetFollowing,
                                               InspectTestCommand,
                                               defender_actions::SetAttackCommitment,
                                               defender_actions::SetDefensiveBash,
                                               defender_actions::BashRequest,
                                               defender_actions::BashAnimation,
                                               BeginDirect,
                                               defender_actions::RequestDirect,
                                               defender_actions::AttackRequest,
                                               defender_actions::AttackAnimation,
                                               sizeof(yliwf::testing::DefenderObservation),
                                               defender_actions::SetMovementBounds};
        return &host;
    }
}

extern "C" __declspec(dllexport) const yliwf::testing::Host* YLIWF_GetCombatTestHost(std::uint32_t version) {
    return version == yliwf::testing::Version ? mod::debug::CombatTestHost() : nullptr;
}
