#include "Dispatch.h"
#include "ExternalAttackReservation.h"
#include <atomic>
#include <array>
#include <chrono>
#include <memory>

namespace mod::combat_actions {
    namespace {
        using Clock = std::chrono::steady_clock;
        using Action = RE::BGSDefaultObjectManager::DefaultObject;
        std::atomic_bool installed{false};
        std::atomic_uint32_t leasedActor{0}, scopeToken{0}, published{0};
        std::atomic_uint64_t expires{0};
        std::atomic_uint32_t actions{0}, attacks{0}, rejected{0}, engineRejected{0}, lastAction{0}, foreignReady{0};
        std::atomic_uint32_t blockRequests{0}, bashRequests{0}, unknownLeftRequests{0};
        std::atomic_uint32_t handoffAttackRejections{0};
        std::array<RE::BGSAction*, 6> offensive{};
        RE::BGSAction* leftAttack = nullptr;
        thread_local bool issuing = false;
        thread_local bool issuingBash = false;
        thread_local auto issuingIntent = Intent::Guard;
        thread_local RE::TESActionData* issuedData = nullptr;

        thread_local DispatchEvidence evidence;
        SKSE::Trampoline trampoline{"YLIWF Defender actions"};
        REL::Relocation<bool (*)(void*, RE::BGSActionData*)> previous;
        REL::Relocation<bool (*)(RE::TESActionData*)> previousProcess;
        REL::Relocation<bool (*)(void*, RE::BGSActionData*)> previousResolved;
        REL::Relocation<bool (*)(void*, RE::BGSActionData*)> previousSelection;
        bool processProtocol = false;
        bool selectionProtocol = false, resolvedProtocol = false;
        ExternalAttackReservation externalAttack;

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
            const bool handoff = externalAttack.BlocksNewAttacks();
            const bool suppress =
                handoff || RejectNewOffense(decision, Milliseconds() <= expires.load(), externalAttack.Active());
            if (allowed && attack && !(data == issuedData && (issuingBash || issuingIntent == Intent::RightAttack)) &&
                suppress) {
                ++rejected;
                if (handoff)
                    ++handoffAttackRejections;
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
            if (!data || !data->source || data->source->GetFormID() != leasedActor.load())
                return allowed;
            const bool ownBash = data == issuedData && issuingBash;
            const bool ownAttack = data == issuedData && issuingIntent == Intent::RightAttack;
            const auto policy = published.load();
            const bool fresh = Milliseconds() <= expires.load();
            const bool handoff = externalAttack.BlocksNewAttacks();
            const bool suppress = handoff || RejectNewOffense(policy, fresh, externalAttack.Active());
            if (allowed && !ownBash && !ownAttack && suppress &&
                std::ranges::find(offensive, data->action) != offensive.end()) {
                ++rejected;
                if (handoff)
                    ++handoffAttackRejections;
                return false;
            }
            if (data->action != leftAttack && !ownBash && !ownAttack)
                return allowed;

            // Selection has populated ActionOutput, but deferred work and graph
            // dispatch have not started. Reject through normal validation cleanup.
            // Read the passed event, not actor state or gameplay policy on a worker.
            const auto* event = data->animEvent.c_str();
            const auto action = ClassifyLeftAction(event ? event : "");
            const bool ownGuard = data == issuedData && issuingIntent == Intent::Guard;
            if (ownGuard || ownBash || ownAttack)
                evidence.event = event ? event : "";
            if (!ownBash && !ownAttack && action == LeftAction::Block)
                ++blockRequests;
            else if (!ownBash && !ownAttack && action == LeftAction::Bash) {
                ++bashRequests;
                ++attacks;
            } else if (!ownBash && !ownAttack)
                ++unknownLeftRequests;

            if (allowed && (ownGuard || ownBash || ownAttack) && externalAttack.Active()) {
                evidence.externalBlocked = true;
                evidence.rejectedBeforeDispatch = resolvedProtocol;
                return false;
            }
            if (allowed && ((ownBash && !AllowedBashEvent(event ? event : "", issuingIntent)) ||
                            (ownAttack && !AllowedRightAttackEvent(event ? event : "")) ||
                            (!ownBash && !ownAttack && RejectLeftAction(action, ownGuard, suppress)) ||
                            RejectDiscretionaryGuard(policy, fresh, ownGuard || ownBash || ownAttack, action))) {
                if (ownGuard || ownBash || ownAttack)
                    evidence.rejectedBeforeDispatch = resolvedProtocol;
                if (action == LeftAction::Bash)
                    ++rejected;
                if (!ownBash && !ownAttack && !ownGuard && action == LeftAction::Bash && handoff)
                    ++handoffAttackRejections;
                return false;
            }
            if (allowed && action == LeftAction::Block && !ownGuard && !ownBash && !ownAttack && (data->flags & 3) == 0)
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

    }

    Receipt Send(RE::Actor* actor, std::uint32_t token, Intent intent, std::uint32_t idleID, std::uint32_t gateID) {
        if (!actor || actor->GetFormID() != leasedActor.load() || !token || token != scopeToken.load())
            return {Dispatch::Rejected, {}, false};
        if (intent == Intent::RightAttack &&
            (!installed || !processProtocol || !selectionProtocol || !resolvedProtocol))
            return {Dispatch::Rejected, {.rejection = "Normal-attack dispatch protocol is unavailable."}, false};
        const bool bash = intent == Intent::Bash || intent == Intent::PowerBash;
        RE::DEFAULT_OBJECT action;
        switch (intent) {
            case Intent::Guard:
                action = RE::DEFAULT_OBJECT::kActionLeftAttack;
                break;
            case Intent::ReleaseGuard:
                action = RE::DEFAULT_OBJECT::kActionLeftRelease;
                break;
            case Intent::RightAttack:
            case Intent::Bash:
                action = RE::DEFAULT_OBJECT::kActionRightAttack;
                break;
            case Intent::PowerBash:
                action = RE::DEFAULT_OBJECT::kActionRightPowerAttack;
                break;
            default:
                return {Dispatch::Rejected, {}, false};
        }

        struct Scope {
            Scope(bool bash, Intent intent) {
                issuing = true;
                issuingBash = bash;
                issuingIntent = intent;
            }

            ~Scope() {
                issuing = false;
                issuingBash = false;
                issuingIntent = Intent::Guard;
            }
        } scope{bash, intent};

        evidence = {};
        RE::TESIdleForm* explicitBashIdle = nullptr;
        if (intent == Intent::PowerBash) {
            auto* idle = RE::TESForm::LookupByID<RE::TESIdleForm>(idleID);
            auto* gate = RE::TESForm::LookupByID<RE::TESIdleForm>(gateID);
            if (!idle || !gate) {
                evidence.rejection = "Explicit power-bash idle unavailable.";
                return {Dispatch::Rejected, evidence, false};
            }
            if (!gate->conditions.IsTrue(actor, nullptr)) {
                evidence.rejection = "Winning power-attack root conditions failed.";
                return {Dispatch::Rejected, evidence, false};
            }
            if (!idle->CheckConditions(actor, nullptr, false)) {
                evidence.rejection =
                    "Power bash ineligible: perk, blocking, block intent or idle attack state condition failed.";
                return {Dispatch::Rejected, evidence, false};
            }
            explicitBashIdle = idle;
        }
        if (bash || intent == Intent::RightAttack) {
            auto* defaults = RE::BGSDefaultObjectManager::GetSingleton();
            auto* selectedAction = defaults ? defaults->GetObject<RE::BGSAction>(action) : nullptr;
            if (!selectedAction)
                return {Dispatch::Rejected, evidence, false};
            // Match NPC input mode/source/action while retaining our capture scope.
            // The NPC helper calls Process virtually and bypasses the source-wrapper hook.
            std::unique_ptr<RE::TESActionData> request{RE::TESActionData::Create()};
            if (!request)
                return {Dispatch::Rejected, evidence, false};
            request->source.reset(actor);
            request->target = nullptr;
            request->action = selectedAction;
            request->unk20 = 2;
            if (explicitBashIdle) {
                // Match the engine's explicit-idle ActionOutput after checking
                // its conditions. Use the captured native combat dispatch path.
                request->animEvent = explicitBashIdle->animEventName;
                request->animObjIdle = explicitBashIdle;
            }
            ProcessIssued(request.get());
        } else
            RE::SourceActionMap::DoAction(actor, action);
        const auto result = ClassifyDispatch(evidence.seen && processProtocol, evidence.returned, evidence.result,
                                             evidence.flags, evidence.rejectedBeforeDispatch);
        return {result, evidence, true};
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
            SKSE::log::warn("Combat action validation site unsupported; native test modes disabled");
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
        SKSE::log::info("Experimental combat action validation hook installed (AE 1.6.1170)");
    }

    void PublishSuppression(bool value, bool recovering, bool safeRecoveryGap) {
        expires = Milliseconds() + 250;
        published =
            (value ? SuppressOffense : 0) | (recovering ? RecoveryOffense : 0) | (safeRecoveryGap ? RecoveryGuard : 0);
    }

    Capabilities Available() {
        return {installed.load(), processProtocol, selectionProtocol, resolvedProtocol};
    }

    bool Active() {
        return leasedActor.load() != 0;
    }

    void Begin(std::uint32_t actor, std::uint32_t token) {
        actions = attacks = rejected = engineRejected = lastAction = foreignReady = 0;
        blockRequests = bashRequests = unknownLeftRequests = handoffAttackRejections = 0;
        published = 0;
        externalAttack.Begin(actor, token);
        RestoreScope(actor, token);
    }

    void RestoreScope(std::uint32_t actor, std::uint32_t token) {
        scopeToken = token;
        leasedActor = actor;
    }

    void Clear() {
        published = 0;
        leasedActor = 0;
        scopeToken = 0;
        externalAttack.Clear();
    }

    Counters Inspect() {
        return {actions.load(),
                attacks.load(),
                rejected.load(),
                engineRejected.load(),
                lastAction.load(),
                blockRequests.load(),
                bashRequests.load(),
                unknownLeftRequests.load(),
                handoffAttackRejections.load()};
    }

    std::uint32_t ForeignGuardEpoch() {
        return foreignReady.load();
    }

    bool ExternalActive() {
        return externalAttack.Active();
    }

    void CloseExternal() {
        externalAttack.Close();
    }

    bool SetExternal(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence, bool active) {
        return externalAttack.Set(actor, token, sequence, active);
    }

    bool FinishExternal(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence) {
        return externalAttack.Finish(actor, token, sequence);
    }
}
