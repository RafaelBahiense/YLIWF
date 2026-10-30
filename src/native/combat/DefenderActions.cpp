#include "DefenderActions.h"
#include "DefenderActionRules.h"
#include "ShieldDefender.h"
#include "CombatRoles.h"
#include "CombatOrders.h"
#include "followers/QuestPolicies.h"
#include "followers/ControllerRuntime.h"
#include "diagnostics/Debug.h"
#include <atomic>
#include <array>
#include <chrono>
#include <mutex>

namespace mod::defender_actions {
    namespace {
        using namespace defender_action_rules;
        using Clock = std::chrono::steady_clock;
        using Action = RE::BGSDefaultObjectManager::DefaultObject;
        std::atomic_bool installed{false};
        std::atomic_uint32_t leasedActor{0}, published{0};
        std::atomic_uint64_t expires{0};
        std::atomic_uint32_t actions{0}, attacks{0}, rejected{0}, engineRejected{0}, lastAction{0}, foreignReady{0};
        std::atomic_uint32_t blockRequests{0}, bashRequests{0}, unknownLeftRequests{0};
        std::array<RE::BGSAction*, 6> offensive{};
        RE::BGSAction* leftAttack = nullptr;
        thread_local bool issuing = false;
        thread_local RE::TESActionData* issuedData = nullptr;

        struct DispatchEvidence {
            bool seen = false, returned = false;
            bool rejectedBeforeDispatch = false;
            std::int32_t result = -1;
            std::uint32_t flags = 0, validationMask = 0;
            std::string event;
        };

        thread_local DispatchEvidence evidence;
        SKSE::Trampoline trampoline{"YLIWF Defender actions"};
        REL::Relocation<bool (*)(void*, RE::BGSActionData*)> previous;
        REL::Relocation<bool (*)(RE::TESActionData*)> previousProcess;
        REL::Relocation<bool (*)(void*, RE::BGSActionData*)> previousResolved;
        REL::Relocation<bool (*)(void*, RE::BGSActionData*)> previousSelection;
        bool processProtocol = false;
        bool selectionProtocol = false, resolvedProtocol = false;
        std::mutex mutex;

        struct Lease {
            std::uint32_t actor = 0, token = 0, readyEpoch = 0;
            Mode mode = Mode::Styles;
            Phase phase = Phase::Idle;
            bool closing = false, armed = true, fault = false;
            bool releaseRejected = false, missedStart = false;
            bool releaseCompleted = false;
            float elapsed = 0, cooldown = 0;
            Clock::time_point lastTick{};
            Observation observation;
        } lease;

        std::uint64_t Milliseconds() {
            return static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());
        }

        bool Validate(void* manager, RE::BGSActionData* data) {
            const bool allowed = previous(manager, data);
            if (!data || !data->source || data->source->GetFormID() != leasedActor.load() || !data->action)
                return allowed;
            ++actions;
            if (data == issuedData)
                evidence.validationMask |= allowed ? InitialAllowed : InitialRejected;
            lastAction = data->action->GetFormID();
            if (!allowed)
                ++engineRejected;
            const bool attack = std::ranges::find(offensive, data->action) != offensive.end();
            if (attack)
                ++attacks;
            const auto decision = published.load();
            if (allowed && attack && decision && Milliseconds() <= expires.load()) {
                ++rejected;
                return false;
            }
            return allowed;
        }

        bool SelectAction(void* manager, RE::BGSActionData* data) {
            const bool allowed = previousSelection(manager, data);
            if (data && data == issuedData) {
                evidence.validationMask |= allowed ? SelectionAllowed : SelectionRejected;
                evidence.rejectedBeforeDispatch |= !allowed && selectionProtocol;
            }
            return allowed;
        }

        bool ValidateResolved(void* manager, RE::BGSActionData* data) {
            const bool allowed = previousResolved(manager, data);
            if (data && data == issuedData) {
                evidence.validationMask |= allowed ? ResolvedAllowed : ResolvedRejected;
                evidence.rejectedBeforeDispatch |= !allowed && resolvedProtocol;
            }
            if (!data || !data->source || data->source->GetFormID() != leasedActor.load() || data->action != leftAttack)
                return allowed;

            // Selection has populated ActionOutput, but deferred work and graph
            // dispatch have not started. Reject through normal validation cleanup.
            // Read the passed event, not actor state or gameplay policy on a worker.
            const auto* event = data->animEvent.c_str();
            const auto action = ClassifyLeftAction(event ? event : "");
            const bool ownGuard = data == issuedData;
            if (ownGuard)
                evidence.event = event ? event : "";
            if (action == LeftAction::Block)
                ++blockRequests;
            else if (action == LeftAction::Bash) {
                ++bashRequests;
                ++attacks;
            } else
                ++unknownLeftRequests;

            const bool suppress = published.load() && Milliseconds() <= expires.load();
            if (allowed && RejectLeftAction(action, ownGuard, suppress)) {
                if (action == LeftAction::Bash)
                    ++rejected;
                return false;
            }
            if (allowed && action == LeftAction::Block && !ownGuard && (data->flags & 3) == 0)
                ++foreignReady;
            return allowed;
        }

        bool ProcessIssued(RE::TESActionData* data) {
            auto* previousData = issuedData;
            const bool capture =
                issuing && !issuedData && data && data->source && data->source->GetFormID() == leasedActor.load();
            if (capture)
                issuedData = data;
            const bool result = previousProcess(data);
            if (capture) {
                evidence.seen = true;
                evidence.returned = result;
                evidence.result = data->result;
                evidence.flags = data->flags;
                issuedData = previousData;
            }
            return result;
        }

        Dispatch Send(RE::Actor* actor, RE::DEFAULT_OBJECT action) {
            struct Scope {
                Scope() {
                    issuing = true;
                }

                ~Scope() {
                    issuing = false;
                }
            } scope;

            evidence = {};
            RE::SourceActionMap::DoAction(actor, action);
            const auto result = ClassifyDispatch(evidence.seen && processProtocol, evidence.returned, evidence.result,
                                                 evidence.flags, evidence.rejectedBeforeDispatch);
            lease.observation.dispatch = static_cast<std::uint32_t>(result);
            lease.observation.validationMask = evidence.validationMask;
            lease.observation.processFlags = evidence.flags;
            lease.observation.processResult = evidence.result;
            lease.observation.processReturned = evidence.seen ? static_cast<std::int32_t>(evidence.returned) : -1;
            if (action == RE::DEFAULT_OBJECT::kActionLeftAttack)
                lease.observation.guardEvent = evidence.event;
            return result;
        }

        void Publish(bool value) {
            expires = Milliseconds() + 250;
            published = value ? 1 : 0;
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
            if (!leasedActor.load())
                return;
            if (lease.phase == Phase::Fault) {
                Publish(false);
                return;
            }
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(lease.actor);
            const auto now = Clock::now();
            const float delta = std::clamp(std::chrono::duration<float>(now - lease.lastTick).count(), 0.0f, 0.25f);
            lease.elapsed += delta;
            lease.cooldown = std::max(0.0f, lease.cooldown - delta);
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
            const bool foreignPackage = process && process->middleHigh && process->middleHigh->runOncePackage.package;
            const bool valid =
                actor && !actor->IsDead() && !actor->IsDisabled() && actor->Is3DLoaded() && actor->IsInCombat() &&
                equipped && Managed(actor) && actor->IsPlayerTeammate() && !actor->GetCurrentScene() &&
                !foreignPackage && !actor->AsActorState()->IsBleedingOut() && !actor->AsActorState()->IsUnconscious() &&
                !actor->AsActorState()->actorState2.staggered && !actor->IsOnMount() &&
                actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) == 0 &&
                combat_roles::Read(actor) == combat_roles::Role::ShieldDefender && !combat_orders::Enabled &&
                quest_policies::Allows(actor, quest_policies::Action::Combat) && defender.requested && defender.owned &&
                !defender.yielded && defender.age >= 0 && defender.age <= 0.25f;
            const bool live = valid && !lease.closing && !lease.fault;
            if (!live)
                Publish(false);
            const bool wanted = live && Guard(lease.mode) && !lease.observation.guardSuspended && defender.committed &&
                                defender.targeting && defender.distance >= 0 && defender.distance <= 352 &&
                                std::abs(defender.heading) <= 80;
            if (!wanted)
                lease.armed = true;
            if (lease.phase != Phase::Idle && lease.phase != Phase::Fault && foreignReady.load() != lease.readyEpoch) {
                lease.phase = Phase::Idle;
                lease.armed = false;
                lease.observation.status = "Another block-start request took ownership; no release was sent.";
            }
            if (!actor || actor->IsDead()) {
                Publish(false);
                if (lease.phase != Phase::Idle)
                    lease.phase = Phase::Fault;
                lease.fault = true;
                lease.observation.status = "Actor unavailable; guard release needs review.";
                return;
            }
            bool blocking = false;
            const bool blockReadable = actor->GetGraphVariableBool("IsBlocking", blocking);
            const bool wantsBlock = actor->AsActorState()->actorState2.wantBlocking;
            lease.observation.blockReadable = blockReadable;
            lease.observation.wantsBlock = wantsBlock;
            const bool canStart = valid && lease.cooldown == 0 && !actor->IsAttacking() && blockReadable &&
                                  !wantsBlock && !actor->AsActorState()->actorState2.recoil &&
                                  actor->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
                                  actor->AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone;
            switch (Decide(lease.phase, wanted, canStart, blocking, lease.armed, lease.elapsed, blockReadable,
                           wantsBlock)) {
                case Step::Start:
                    lease.cooldown = 1.5f;
                    lease.armed = false;
                    lease.readyEpoch = foreignReady.load();
                    ++lease.observation.guardRequests;
                    lease.elapsed = 0;
                    lease.phase = Phase::StartPending;
                    lease.releaseRejected = lease.missedStart = false;
                    lease.releaseCompleted = false;
                    lease.observation.startRecovering = defender.recovering;
                    lease.observation.startStaminaFraction =
                        defender.maximumStamina > 0 ? defender.stamina / defender.maximumStamina : -1;
                    if (const auto result = Send(actor, RE::DEFAULT_OBJECT::kActionLeftAttack);
                        result == Dispatch::Accepted) {
                        ++lease.observation.guardAccepted;
                        lease.observation.status = "Shield block-start accepted; awaiting blocking state.";
                    } else if (result == Dispatch::Rejected) {
                        ++lease.observation.guardRejected;
                        lease.phase = Phase::Idle;
                        lease.observation.status =
                            (evidence.validationMask & SelectionRejected)
                                ? "No eligible block idle selected; request rejected before dispatch."
                                : "Shield block-start rejected; no guard is owned.";
                    } else {
                        // The dispatcher's false result does not expose whether
                        // downstream action work remains. Do not assume rejection
                        // or retry this request on a later threat episode.
                        lease.phase = Phase::Fault;
                        lease.fault = true;
                        Publish(false);
                        lease.observation.status =
                            "Shield block-start dispatch unconfirmed; manual review required. No retry.";
                    }
                    break;
                case Step::Acknowledge:
                    ++lease.observation.guardObserved;
                    lease.phase = Phase::Holding;
                    lease.elapsed = 0;
                    lease.observation.status = "Requested guard observed.";
                    break;
                case Step::Release:
                    if (lease.phase == Phase::StartPending) {
                        ++lease.observation.timeouts;
                        lease.missedStart = true;
                        lease.observation.guardSuspended = true;
                        Publish(false);
                    }
                    ++lease.observation.releaseRequests;
                    lease.phase = Phase::ReleasePending;
                    lease.elapsed = 0;
                    if (const auto dispatch = Send(actor, RE::DEFAULT_OBJECT::kActionLeftRelease);
                        dispatch == Dispatch::Accepted) {
                        ++lease.observation.releaseAccepted;
                        lease.releaseCompleted = true;
                        lease.observation.status = "Shield-release accepted; awaiting non-blocking state.";
                    } else if (dispatch == Dispatch::Rejected) {
                        lease.releaseRejected = true;
                        lease.releaseCompleted = true;
                        ++lease.observation.releaseRejected;
                        lease.observation.status =
                            "Shield-release rejected; observing block intent and graph without retry.";
                    } else {
                        lease.phase = Phase::Fault;
                        lease.fault = true;
                        lease.observation.status = "Shield-release dispatch unconfirmed; no request was replayed.";
                    }
                    break;
                case Step::Finished:
                    if (lease.phase == Phase::ReleasePending && !lease.releaseRejected)
                        ++lease.observation.releaseObserved;
                    lease.phase = Phase::Idle;
                    lease.elapsed = 0;
                    if (lease.missedStart)
                        lease.observation.status =
                            lease.releaseRejected
                                ? "Guard start unobserved; release rejected, no guard intent remains. Native guards "
                                  "suspended."
                                : "Guard start unobserved; release confirmed. Native guards suspended.";
                    else
                        lease.observation.status =
                            lease.releaseRejected ? "Release rejected; no guard intent remains." : "Guard ended.";
                    break;
                case Step::Fault:
                    if (lease.phase != Phase::Fault)
                        ++lease.observation.timeouts;
                    lease.phase = Phase::Fault;
                    lease.fault = true;
                    Publish(false);
                    lease.observation.status = lease.releaseRejected
                                                   ? "Release rejected; block intent or graph still unresolved. Manual "
                                                     "review required. No retry."
                                                   : "Guard release unconfirmed; manual review required. No replay.";
                    break;
                case Step::None:
                    break;
            }
            const bool guardActive = blocking || lease.phase == Phase::StartPending || lease.phase == Phase::Holding;
            Publish(Suppress(lease.mode, valid && !lease.closing && !lease.fault, defender.recovering,
                             !lease.observation.guardSuspended && defender.committed && guardActive, true));
        }
    }

    void Install() {
        if (installed || REL::Module::get().version() != REL::Version(1, 6, 1170, 0))
            return;
        // Verified AE 1.6.1170 action-manager validation call. Rejection remains
        // inside the engine's normal completion path. Existing callees are chained.
        const auto entry = REL::ID(38949).address();
        const auto source = REL::ID(34202).address();
        const auto* sourceBytes = reinterpret_cast<const std::uint8_t*>(source);
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(entry);
        if (bytes[0x4F] != 0x48 || bytes[0x50] != 0x8B || bytes[0x51] != 0xD3 || bytes[0x52] != 0x48 ||
            bytes[0x53] != 0x8B || bytes[0x54] != 0x0D || bytes[0x59] != 0xE8 || sourceBytes[0x65] != 0x48 ||
            sourceBytes[0x66] != 0x8D || sourceBytes[0x67] != 0x4C || sourceBytes[0x68] != 0x24 ||
            sourceBytes[0x69] != 0x40 || sourceBytes[0x6A] != 0xE8 || bytes[0x86] != 0x48 || bytes[0x87] != 0x8B ||
            bytes[0x88] != 0xD3 || bytes[0x90] != 0xE8 || bytes[0x69] != 0x48 || bytes[0x6A] != 0x8B ||
            bytes[0x6B] != 0xD3 || bytes[0x73] != 0xE8) {
            SKSE::log::warn("Defender action validation site unsupported; native test modes disabled");
            return;
        }
        auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
        if (!defaults)
            return;
        constexpr std::array actionsToGate{Action::kActionRightAttack,      Action::kActionRightReady,
                                           Action::kActionRightPowerAttack, Action::kActionLeftPowerAttack,
                                           Action::kActionDualAttack,       Action::kActionDualPowerAttack};
        for (std::size_t i = 0; i < offensive.size(); ++i) {
            offensive[i] = defaults->GetObject<RE::BGSAction>(actionsToGate[i]);
            if (!offensive[i])
                return;
        }
        leftAttack = defaults->GetObject<RE::BGSAction>(Action::kActionLeftAttack);
        if (!leftAttack || !defaults->GetObject<RE::BGSAction>(Action::kActionLeftRelease))
            return;
        trampoline.create(128);
        previousProcess = trampoline.write_call<5>(source + 0x6A, ProcessIssued);
        const auto processor = REL::ID(41557).address();
        const auto* processorBytes = reinterpret_cast<const std::uint8_t*>(processor);
        processProtocol = previousProcess.address() == processor && processorBytes[0] == 0x48 &&
                          processorBytes[1] == 0x8B && processorBytes[2] == 0xD1 && processorBytes[3] == 0x48 &&
                          processorBytes[4] == 0x8B && processorBytes[5] == 0x0D && processorBytes[10] == 0xE9;
        previous = trampoline.write_call<5>(entry + 0x59, Validate);
        previousSelection = trampoline.write_call<5>(entry + 0x73, SelectAction);
        previousResolved = trampoline.write_call<5>(entry + 0x90, ValidateResolved);
        const auto selector = REL::ID(37147).address();
        const auto* selectorBytes = reinterpret_cast<const std::uint8_t*>(selector);
        selectionProtocol = previousSelection.address() == selector && selectorBytes[0] == 0x40 &&
                            selectorBytes[1] == 0x53 && selectorBytes[2] == 0x48 && selectorBytes[3] == 0x83 &&
                            selectorBytes[4] == 0xEC && selectorBytes[5] == 0x20;
        const auto resolved = REL::ID(39003).address();
        const auto* resolvedBytes = reinterpret_cast<const std::uint8_t*>(resolved);
        resolvedProtocol = previousResolved.address() == resolved && resolvedBytes[0] == 0x40 &&
                           resolvedBytes[1] == 0x56 && resolvedBytes[2] == 0x57 && resolvedBytes[3] == 0x41 &&
                           resolvedBytes[4] == 0x56 && resolvedBytes[5] == 0x48;
        installed = true;
        SKSE::log::info("Experimental defender action validation hook installed (AE 1.6.1170)");
    }

    bool Begin(std::uint32_t actor, std::uint32_t mode, std::uint32_t token) {
        std::lock_guard lock(mutex);
        if (!actor || !token || !ValidMode(mode) || leasedActor.load() || (mode && !installed) ||
            ((mode & 2) && !processProtocol))
            return false;
        lease = {};
        lease.actor = actor;
        lease.token = token;
        lease.mode = static_cast<Mode>(mode);
        lease.lastTick = Clock::now();
        actions = attacks = rejected = engineRejected = lastAction = foreignReady = 0;
        blockRequests = bashRequests = unknownLeftRequests = 0;
        published = 0;
        leasedActor = actor;
        return true;
    }

    std::uint32_t End(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase) {
        std::lock_guard lock(mutex);
        if (!leasedActor.load()) {
            if (!savedPhase)
                return 1;
            if (!installed || savedPhase > static_cast<std::uint32_t>(Phase::Fault))
                return 2;
            const auto* actorRef = RE::TESForm::LookupByID<RE::Actor>(actor);
            if (!actorRef)
                return 2;
            bool blocking = false;
            const bool readable = actorRef->GetGraphVariableBool("IsBlocking", blocking);
            const bool intent = actorRef->AsActorState()->actorState2.wantBlocking;
            const auto recovery =
                RestoreGuard(static_cast<Phase>(savedPhase), !ReleaseCleared(readable, blocking, intent));
            if (recovery == Recovery::Finished)
                return 1;
            lease = {};
            lease.actor = actor;
            lease.token = token;
            lease.phase = recovery == Recovery::Manual ? Phase::Fault : Phase::ReleasePending;
            lease.fault = lease.phase == Phase::Fault;
            lease.readyEpoch = foreignReady.load();
            lease.observation.status =
                recovery == Recovery::Manual
                    ? "Loaded guard ownership is unverified; no new release was sent. Manual review required."
                    : "Observing the saved release without replaying it.";
            lease.lastTick = Clock::now();
            leasedActor = actor;
        }
        if (lease.actor != actor || lease.token != token)
            return 2;
        lease.closing = true;
        Publish(false);
        if (lease.phase == Phase::Fault && lease.releaseCompleted) {
            auto* actorRef = RE::TESForm::LookupByID<RE::Actor>(actor);
            if (actorRef && actorRef->Is3DLoaded()) {
                bool blocking = false;
                const bool readable = actorRef->GetGraphVariableBool("IsBlocking", blocking);
                const bool intent = actorRef->AsActorState()->actorState2.wantBlocking;
                if (ReconcileRelease(lease.releaseCompleted, readable, blocking, intent)) {
                    lease.phase = Phase::Idle;
                    lease.fault = false;
                    lease.observation.blockReadable = true;
                    lease.observation.wantsBlock = false;
                    lease.observation.status =
                        "Completed release rechecked; graph and block intent are clear. No command replayed.";
                }
            }
        }
        Pump();
        if (lease.phase == Phase::Fault)
            return 2;
        if (lease.phase != Phase::Idle)
            return 0;
        leasedActor = 0;
        return 1;
    }

    std::uint32_t SavedPhase(std::uint32_t actor, std::uint32_t token) {
        std::lock_guard lock(mutex);
        return lease.actor == actor && lease.token == token ? static_cast<std::uint32_t>(lease.phase) : 0;
    }

    Observation Inspect(std::uint32_t actor) {
        std::lock_guard lock(mutex);
        Observation result;
        if (actor == lease.actor) {
            result = lease.observation;
            result.mode = static_cast<std::uint32_t>(lease.mode);
            result.phase = static_cast<std::uint32_t>(lease.phase);
            result.fault = lease.fault;
            result.actions = actions;
            result.attacks = attacks;
            result.rejected = rejected;
            result.engineRejected = engineRejected;
            result.lastAction = lastAction;
            result.blockRequests = blockRequests;
            result.bashRequests = bashRequests;
            result.unknownLeftRequests = unknownLeftRequests;
        }
        result.available = installed;
        return result;
    }

    void Tick() {
        std::lock_guard lock(mutex);
        Pump();
    }

    bool HasWork() {
        std::lock_guard lock(mutex);
        return leasedActor.load() != 0 && lease.phase != Phase::Fault;
    }

    void Invalidate() {
        std::lock_guard lock(mutex);
        published = 0;
        leasedActor = 0;
        lease = {};
    }
}
