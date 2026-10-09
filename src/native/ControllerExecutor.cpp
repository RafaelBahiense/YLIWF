#include "ControllerExecutor.h"

#include "ControllerRules.h"
#include "CommandRules.h"
#include "Debug.h"
#include "PartyRules.h"
#include "Settings.h"
#include "Strings.h"
#include "NativeLatent.h"
#include "ExecutorRules.h"
#include "FollowDistance.h"

#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <thread>

namespace mod::controller::executor {
    namespace {
        using namespace storage;
        using namespace controller_rules;
        const Context& ContextData() { return detail::GetContext(); }
        bool pumping = false, countsDirty = false;
        using executor_rules::Phase;
        std::atomic<std::uint64_t> generation{0};
        std::atomic<bool> scheduled{false};
        bool loaded = false;
        auto lastTick = std::chrono::steady_clock::now();
        bool lastPaused = true;
        void Tick();
        bool PublishCounts(const Script& object);

        // This worker only schedules main-thread tasks. Engine/VM state is never
        // touched here, and it sleeps indefinitely when no deadline needs ticking.
        class Deadlines {
        public:
            Deadlines() : worker([this](std::stop_token stop) {
                std::unique_lock lock(mutex);
                while (!stop.stop_requested()) {
                    changed.wait(lock, stop, [this] { return active; });
                    if (stop.stop_requested()) break;
                    if (!changed.wait_for(lock, stop, interval, [this] { return !active; })) {
                        lock.unlock(); Wake(); lock.lock();
                    }
                }
            }) {}
            void Set(bool value, bool fast = false) {
                std::scoped_lock lock(mutex);
                active = value; interval = std::chrono::milliseconds(fast ? 50 : 1000); changed.notify_all();
            }
        private:
            std::mutex mutex;
            std::condition_variable_any changed;
            bool active = false;
            std::chrono::milliseconds interval{1000};
            std::jthread worker;
        };
        Deadlines& DeadlineScheduler() {
            // SKSE plugins live until process exit. Create outside DLL static
            // initialization, and avoid joining a thread under the loader lock.
            static auto* scheduler = new Deadlines;
            return *scheduler;
        }

        bool Paused() { auto* ui = RE::UI::GetSingleton(); return ui && ui->GameIsPaused(); }
        Script Object() { return detail::Object(); }
        RE::BGSRefAlias* Alias(RE::TESQuest* quest, std::int32_t id) {
            if (!quest || id < 0) return nullptr;
            for (auto* alias : quest->aliases) if (alias && alias->aliasID == static_cast<std::uint32_t>(id))
                return skyrim_cast<RE::BGSRefAlias*>(alias);
            return nullptr;
        }
        bool Timer(std::int32_t id, bool start) {
            if (!start) return executor_rules::SetDeadline(storage::Get().deadlines, id, std::nullopt);
            const auto* calendar = RE::Calendar::GetSingleton();
            return calendar && executor_rules::SetDeadline(storage::Get().deadlines, id, calendar->GetHoursPassed());
        }
        void RefreshClock() { lastTick = std::chrono::steady_clock::now(); lastPaused = Paused(); }

        void ReturnWaiters(const Script&, std::int32_t request, bool success) {
            auto& callers = storage::Get().callers;
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            for (auto& caller : callers) if (caller.request == request && caller.result < 0) caller.result = success ? 1 : 0;
            for (std::size_t i = 0; i < callers.size();) {
                const auto caller = callers[i];
                if (caller.result < 0 || !vm || !storage::CurrentCaller(caller)) { ++i; continue; }
                // Remove before returning to the VM. Never resume a reused stack ID.
                callers.erase(callers.begin() + i);
                vm->ReturnLatentResult(caller.stack, caller.result == 1);
            }
        }
        void Complete(const Script& object, bool succeeded) {
            if (!storage::Get().active) return;
            const auto request = storage::Active().request;
            Write(object, "iFollowerDismiss", 0);
            const bool verified = detail::Complete(succeeded);
            ReturnWaiters(object, request, verified); Wake();
        }
        void Finalize(const Script& object, bool succeeded) {
            if (!storage::Get().active) return;
            storage::Active().succeeded = succeeded;
            if (!succeeded) Write(object, "iFollowerDismiss", 0);
            PublishCounts(object);
        }
        bool Current(RE::StaticFunctionTag*, RE::TESQuest* owner, std::int32_t ticket, std::int32_t cursor) {
            std::scoped_lock lock(settings::Mutex);
            if (owner != ContextData().quest || !Object() || !storage::Get().active) return false;
            const auto& active = storage::Active();
            if (active.ticket != ticket || active.Cursor() != cursor) return false;
            if (active.phase == Phase::Counts) return !active.succeeded || active.Done();
            return active.phase == Phase::Engine && !active.Done() && detail::Validate(active.steps[active.next]);
        }
        void Returned(RE::StaticFunctionTag*, RE::TESQuest* owner, std::int32_t ticket, std::int32_t cursor, bool succeeded) {
            std::scoped_lock lock(settings::Mutex);
            if (owner != ContextData().quest || !Object() || !storage::Get().active) return;
            auto& active = storage::Active();
            if (!executor_rules::CanAcknowledge(true, active.ticket, active.Cursor(), active.phase, ticket, cursor)) return;
            if (active.phase == Phase::Counts) { Complete(Object(), succeeded && active.succeeded); return; }
            if (active.phase != Phase::Engine || active.Done()) return;
            const auto step = active.steps[active.next];
            if (step.effect == Effect::SwapPrimary) {
                auto* primary = Alias(ContextData().quest, 0);
                auto* extra = Alias(ContextData().quest, step.alias);
                auto* previous = storage::Actor(step.otherActor);
                succeeded = succeeded && primary && extra && (!step.otherActor || previous) &&
                    primary->GetReference() == storage::Actor(step.actor) && extra->GetReference() == previous;
                if (primary) primary->owningQuest->AddChange(RE::TESQuest::ChangeFlags::kQuestRuntimeData);
                if (succeeded) succeeded = executor_rules::SwapPrimaryDeadlines(storage::Get().deadlines, step.alias, step.otherActor != 0);
            }
            if (step.effect == Effect::Clear || step.effect == Effect::HomeClear || step.effect == Effect::Assign || step.effect == Effect::HomeAssign) {
                auto* slot = Alias(IsHomeEffect(step.effect) ? ContextData().homeQuest : ContextData().quest, step.alias);
                const bool assign = step.effect == Effect::Assign || step.effect == Effect::HomeAssign;
                succeeded = succeeded && slot && slot->GetReference() == (assign ? storage::Actor(step.actor) : nullptr);
                if (slot) slot->owningQuest->AddChange(RE::TESQuest::ChangeFlags::kQuestRuntimeData);
            }
            if (!detail::Acknowledge(ticket, cursor, succeeded)) { Finalize(Object(), false); return; }
            active.phase = Phase::Ready; Wake();
        }
        template <class... Args> bool Bridge(const Script&, Phase phase, const char* function, Args... args) {
            storage::Active().phase = phase;
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
            const auto script = std::string(mod::info::ScriptPrefix) + "_Engine";
            // Completion is acknowledged from the saved adapter stack, never from
            // an unsaved C++ callback. Accepted calls must not be replayed on load.
            const bool accepted = vm && vm->DispatchStaticCall(script.c_str(), function,
                RE::MakeFunctionArguments(static_cast<RE::TESQuest*>(ContextData().quest), std::int32_t{storage::Get().ticket},
                    std::int32_t{storage::Active().Cursor()}, std::move(args)...), callback);
            if (!accepted) storage::Get().failure = "Engine adapter dispatch rejected";
            return accepted;
        }
        bool PublishCounts(const Script& object) {
            storage::Active().phase = Phase::Counts;
            const auto count = detail::Count();
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            auto* vanilla = Read<RE::TESGlobal*>(object, "pPlayerFollowerCount");
            auto* party = storage::PartyCount();
            auto* gate = storage::RecruitGate();
            if (!vanilla || !party || !gate || !vm) { Complete(object, false); return false; }
            storage::Get().followerCount = count;
            // Use the same effective-cap provider as the planner (perk/Speech modes).
            const auto values = party_rules::CountGlobals(count, detail::Cap());
            const bool accepted = Bridge(object, Phase::Counts, "Counts", vanilla, party, gate, values.vanillaCount, values.partyCount, values.recruitGate);
            if (!accepted) Complete(object, false);
            return accepted;
        }
        bool Execute(const Script& object, const storage::Step& step) {
            const auto effect = step.effect; const auto id = step.alias, argument = step.argument;
            auto* actor = storage::Actor(step.actor);
            auto* slot = Alias(IsHomeEffect(effect) ? ContextData().homeQuest : ContextData().quest, id);
            switch (effect) {
            case Effect::Prepare: {
                if (auto* dismissed = Read<RE::TESFaction*>(object, "pDismissedFollower")) actor->RemoveFromFaction(dismissed);
                const auto& context = ContextData();
                if (context.currentFaction && (argument == 1 || (context.potentialFaction && actor->IsInFaction(context.potentialFaction))) && !actor->IsInFaction(context.currentFaction)) actor->AddToFaction(context.currentFaction, 0);
                if (context.applyProtection) context.applyProtection(actor);
                follow_distance::ApplyActor(actor);
                const auto waiting = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer);
                if (argument == 0 || (waiting != 0 && waiting != 1)) actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kWaitingForPlayer, 0);
                actor->EvaluatePackage(); return true;
            }
            case Effect::Release:
                if (ContextData().restoreProtection) ContextData().restoreProtection(actor);
                if (ContextData().currentFaction) actor->RemoveFromFaction(ContextData().currentFaction);
                if (actor->IsDead()) if (auto* hireling = Read<RE::TESFaction*>(object, "pCurrentHireling")) actor->RemoveFromFaction(hireling);
                actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kWaitingForPlayer, 0);
                actor->EvaluatePackage(); return true;
            case Effect::Protection:
                if (argument && ContextData().applyProtection) ContextData().applyProtection(actor);
                else if (!argument && ContextData().restoreProtection) ContextData().restoreProtection(actor);
                return true;
            case Effect::Waiting:
                actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kWaitingForPlayer, argument ? 1.0f : 0.0f);
                actor->EvaluatePackage(); return true;
            case Effect::Evaluate: case Effect::HomeEvaluate: actor->EvaluatePackage(); return true;
            case Effect::ReservedUpdate: return true; // Resume saved ID 10; new plans publish counts only at completion.
            case Effect::HomeFaction:
                if (!ContextData().homeFaction) return false;
                if (argument) actor->AddToFaction(ContextData().homeFaction, 0); else actor->RemoveFromFaction(ContextData().homeFaction);
                return true;
            case Effect::CancelTimer: return Timer(id, false);
            case Effect::StartTimer: return Timer(id, true);
            case Effect::SwapPrimary:
                return Bridge(object, Phase::Engine, "SwapPrimary", Alias(ContextData().quest, 0), slot,
                    static_cast<RE::TESObjectREFR*>(actor), static_cast<RE::TESObjectREFR*>(storage::Actor(step.otherActor)));
            case Effect::Clear: case Effect::HomeClear: return Bridge(object, Phase::Engine, "Clear", slot);
            case Effect::Assign: case Effect::HomeAssign: return Bridge(object, Phase::Engine, "Assign", slot, static_cast<RE::TESObjectREFR*>(actor));
            case Effect::Teammate: return Bridge(object, Phase::Engine, "Teammate", actor, argument == 1, true);
            case Effect::Relationship: return Bridge(object, Phase::Engine, "Relationship", actor);
            case Effect::StopCombat:
                if (auto* processes = RE::ProcessLists::GetSingleton()) { processes->StopCombatAndAlarmOnActor(actor, false); return true; }
                return false;
            case Effect::Cleanup:
                if (auto* dismissed = Read<RE::TESFaction*>(object, "pDismissedFollower")) actor->AddToFaction(dismissed, 0);
                if (auto* hireling = Read<RE::TESFaction*>(object, "pCurrentHireling")) actor->RemoveFromFaction(hireling);
                actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kWaitingForPlayer, 0);
                for (auto* item : std::array<RE::TESBoundObject*, 2>{Read<RE::TESObjectWEAP*>(object, "FollowerHuntingBow"), Read<RE::TESAmmo*>(object, "FollowerIronArrow")})
                    if (item) actor->RemoveItem(item, 999, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                return true;
            case Effect::Message: {
                constexpr std::array names{"FollowerDismissMessage", "FollowerDismissMessageWedding", "FollowerDismissMessageCompanions",
                    "FollowerDismissMessageCompanionsMale", "FollowerDismissMessageCompanionsFemale", "FollowerDismissMessageWait"};
                const auto* name = argument == -2 ? "AnimalDismissMessage" : names[argument >= 0 && argument < 6 ? argument : 0];
                return Bridge(object, Phase::Engine, "Message", Read<RE::BGSMessage*>(object, name));
            }
            case Effect::Hireling:
                if (auto* quest = Read<RE::TESQuest*>(object, "HirelingRehireScript"))
                    return Bridge(object, Phase::Engine, "Hireling", quest, actor->GetActorBase());
                return true;
            case Effect::DismissLine:
                if (!Write(object, "iFollowerDismiss", 1)) return false;
                storage::Active().delay = 2.0f; storage::Active().phase = Phase::Delay;
                actor->EvaluatePackage(); RefreshClock(); return true;
            case Effect::EndDismissLine: return Write(object, "iFollowerDismiss", 0);
            case Effect::HideObjective: return Bridge(object, Phase::Engine, "Objective", ContextData().quest, argument);
            case Effect::Speaker: return storage::SetActor(storage::Get().speaker, actor);
            case Effect::ClearSpeaker:
                return storage::Actor(storage::Get().speaker) != actor || storage::SetActor(storage::Get().speaker, static_cast<RE::Actor*>(nullptr));
            case Effect::AnimalPrepare:
                actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kLockpicking, 0);
                return Bridge(object, Phase::Engine, "Animal", actor);
            case Effect::AnimalCount:
                if (!argument) actor->AsActorValueOwner()->SetActorValue(RE::ActorValue::kVariable04, 0);
                return Bridge(object, Phase::Engine, "SetGlobal", Read<RE::TESGlobal*>(object, "pPlayerAnimalCount"), static_cast<float>(argument));
            case Effect::HomeMove: {
                auto* marker = Alias(ContextData().homeQuest, id + 8);
                if (!marker || !marker->GetReference()) return false;
                marker->GetReference()->MoveTo(actor); return true;
            }
            default: return false;
            }
        }
        void TimedOut() {
            // During an alias exchange, deadlines still belong to the pre-swap slots until acknowledgement.
            if (storage::Get().active && storage::Active().operation == Operation::PromotePrimary) return;
            auto timers = storage::Get().deadlines;
            auto* calendar = RE::Calendar::GetSingleton();
            if (!calendar || timers.size() != 9) return;
            for (int id = 0; id < 9; ++id) if (timers[id] > 0 && timers[id] <= calendar->GetHoursPassed()) {
                auto* slot = Alias(ContextData().quest, id);
                auto* actor = slot ? slot->GetActorReference() : nullptr;
                if (!actor || actor->IsDead() || actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) != 1 ||
                    detail::Enqueue(Operation::Timeout, actor, id, 5, 1, 0)) timers[id] = 0;
            }
            storage::Get().deadlines = std::move(timers);
        }
        void Tick() {
            std::scoped_lock lock(settings::Mutex);
            if (pumping) return;
            pumping = true;
            struct Exit { ~Exit() { pumping = false; } } exit;
            auto object = Object();
            if (!loaded || !object) { DeadlineScheduler().Set(false); return; }
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm || vm->IsCompletelyFrozen()) { DeadlineScheduler().Set(true); RefreshClock(); return; }
            const bool paused = Paused();
            const float elapsed = !paused && !lastPaused ? std::chrono::duration<float>(std::chrono::steady_clock::now() - lastTick).count() : 0;
            RefreshClock(); ReturnWaiters(object, 0, false);
            if (!paused) {
                TimedOut();
                if (!storage::Get().active) {
                    detail::StartNext();
                    if (!storage::Get().active && countsDirty && detail::StartCounts()) countsDirty = false;
                    ReturnWaiters(object, 0, false);
                }
                if (storage::Get().active) {
                    if (storage::Active().phase == Phase::Delay) {
                        auto& active = storage::Active();
                        active.delay = executor_rules::Remaining(active.delay, elapsed, false, false);
                        if (active.delay == 0) {
                            if (!detail::Acknowledge(active.ticket, active.Cursor(), true)) Finalize(object, false);
                            else active.phase = Phase::Ready;
                        }
                    }
                    while (storage::Get().active && storage::Active().phase == Phase::Ready) {
                        const auto& active = storage::Active();
                        if (active.Done()) { Finalize(object, true); break; }
                        const auto step = active.steps[active.next];
                        const auto ticket = active.ticket, cursor = active.Cursor();
                        if (!detail::Validate(step) || !Execute(object, step)) { Finalize(object, false); break; }
                        if (storage::Get().active && storage::Active().phase == Phase::Ready && storage::Active().Cursor() == cursor &&
                            !detail::Acknowledge(ticket, cursor, true)) { Finalize(object, false); break; }
                    }
                }
            }
            const bool delay = storage::Get().active && storage::Active().phase == Phase::Delay;
            const bool work = delay || countsDirty || !storage::Get().queue.empty() || !storage::Get().callers.empty() ||
                std::ranges::any_of(storage::Get().deadlines, [](float time) { return time > 0; });
            DeadlineScheduler().Set(work, delay && !paused);
        }
        RE::BSScript::LatentStatus Run(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID stack, RE::StaticFunctionTag*,
            RE::TESQuest* owner, int operation, RE::Actor* actor, int selected, int message, int sayLine, int debugTicket) {
            std::scoped_lock lock(settings::Mutex);
            auto object = Object();
            if (!object || owner != ContextData().quest || storage::Get().callers.size() >= 32) return RE::BSScript::kFailed;
            auto caller = storage::CaptureCaller(stack, 0);
            if (caller.type.empty() || caller.function.empty()) {
                logger::warn("Native operation cannot identify Papyrus caller stack {}", stack);
                return RE::BSScript::kFailed;
            }
            const int request = detail::Enqueue(static_cast<Operation>(operation), actor, selected, message, sayLine, debugTicket, std::move(caller));
            if (!request) return RE::BSScript::kFailed;
            Wake(); return RE::BSScript::kStarted;
        }
        bool Restore(RE::StaticFunctionTag*, RE::TESQuest* owner, int ticket) {
            std::scoped_lock lock(settings::Mutex); auto object = Object();
            if (!object || owner != ContextData().quest || !debug::IsCommandCurrent(ticket)) return false;
            std::vector<RE::BGSRefAlias*> slots;
            for (int i = 0; i < 8; ++i) {
                auto* slot = Alias(owner, i ? i + 1 : 0);
                if (!slot || slot->owningQuest != owner || !party_rules::ValidBinding(true, slot->aliasID, slot->aliasName.c_str(), i == 0)) return false;
                slots.push_back(slot);
            }
            auto* data = RE::TESDataHandler::GetSingleton();
            auto* gate = data ? data->LookupForm<RE::TESGlobal>(0x001, mod::info::PluginFile) : nullptr;
            auto* count = data ? data->LookupForm<RE::TESGlobal>(0x002, mod::info::PluginFile) : nullptr;
            auto* vanilla = data ? data->LookupForm<RE::TESGlobal>(0xBCC98, "Skyrim.esm") : nullptr;
            if (!gate || !count || !vanilla) return false;
            for (const auto* name : {"pFollowerAlias", "pPlayerFollowerCount"})
                if (!Field(object, name)) return false;
            if (!Write(object, "pFollowerAlias", slots[0]) || !Write(object, "pPlayerFollowerCount", vanilla)) return false;
            debug::NotifyStateChanged(); return true;
        }
        bool DebugCommand(RE::StaticFunctionTag* tag, RE::TESQuest* owner, int action, RE::Actor* actor, int alias, int ticket) {
            if (!debug::IsCommandCurrent(ticket)) return false;
            constexpr std::array operations{Operation::Sync, Operation::Repair, Operation::Follow, Operation::Wait, Operation::Dismiss,
                Operation::ClearSlot, Operation::Adopt, Operation::Release, Operation::PromotePrimary};
            if (action < 0 || action >= static_cast<int>(operations.size()) || (action == 0 && !Restore(tag, owner, ticket))) return false;
            return detail::Enqueue(operations[action], actor, alias, action == 4 ? -1 : 0, action == 4 ? 0 : 1, ticket) > 0;
        }
        void Speaker(RE::StaticFunctionTag*, RE::TESQuest* owner, RE::Actor* actor, bool clear) {
            std::scoped_lock lock(settings::Mutex); auto object = Object();
            if (!object || owner != ContextData().quest) return;
            if (clear) { if (storage::Actor(storage::Get().speaker) == actor) storage::SetActor(storage::Get().speaker, static_cast<RE::Actor*>(nullptr)); }
            else if (actor && actor != RE::PlayerCharacter::GetSingleton()) storage::SetActor(storage::Get().speaker, actor);
        }
        bool Befriend(RE::StaticFunctionTag*, int rank) { return rank >= 0 && rank < 3; }
        RE::Actor* Target(RE::StaticFunctionTag*, RE::TESQuest* owner, RE::Actor* lastSpeaker) {
            std::scoped_lock lock(settings::Mutex); auto object = Object();
            if (!object || owner != ContextData().quest) return nullptr;
            auto* topics = RE::MenuTopicManager::GetSingleton();
            auto reference = topics ? topics->speaker.get() : RE::NiPointer<RE::TESObjectREFR>{};
            auto* dialogue = reference ? reference->As<RE::Actor>() : nullptr;
            auto* speaker = lastSpeaker ? lastSpeaker : storage::Actor(storage::Get().speaker);
            auto slots = storage::ExtraAliases();
            auto* primary = Read<RE::BGSRefAlias*>(object, "pFollowerAlias"); slots.insert(slots.begin(), primary);
            for (auto* actor : {dialogue, speaker}) if (actor) for (auto* slot : slots) if (slot && slot->GetActorReference() == actor) return actor;
            return primary ? primary->GetActorReference() : nullptr;
        }
        RE::BSScript::LatentStatus SetFollower(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag, RE::TESObjectREFR* follower) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::Recruit), follower ? follower->As<RE::Actor>() : nullptr, -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus SetAnimal(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag, RE::TESObjectREFR* animal) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::AnimalRecruit), animal ? animal->As<RE::Actor>() : nullptr, -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus FollowerWait(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::DialogueWait), Target(nullptr, ContextData().quest, nullptr), -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus FollowerFollow(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::DialogueFollow), Target(nullptr, ContextData().quest, nullptr), -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus WaitActor(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag, RE::Actor* actor) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::Wait), actor, -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus FollowActor(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag, RE::Actor* actor) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::Follow), actor, -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus SetHome(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag,
            RE::Actor* actor, int action, RE::TESQuest* home, RE::TESFaction* faction) {
            if ((action != 0 && action != 1) || home != ContextData().homeQuest || faction != ContextData().homeFaction) return RE::BSScript::kFailed;
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(action == 0 ? Operation::HomeAssign : Operation::HomeRemove), actor, -1, 0, 1, 0);
        }
        RE::Actor* Animal() { auto* alias = Read<RE::BGSRefAlias*>(Object(), "pAnimalAlias"); return alias ? alias->GetActorReference() : nullptr; }
        RE::BSScript::LatentStatus AnimalWait(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::AnimalWait), Animal(), -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus AnimalFollow(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::AnimalFollow), Animal(), -1, 0, 1, 0);
        }
        RE::BSScript::LatentStatus DismissFollower(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag, int message, int sayLine) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::DialogueDismiss), Target(nullptr, ContextData().quest, nullptr), -1, message, sayLine, 0);
        }
        RE::BSScript::LatentStatus DismissActor(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag, RE::Actor* actor, int message, int sayLine) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::Dismiss), actor, -1, message, sayLine, 0);
        }
        RE::BSScript::LatentStatus DismissAnimal(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, RE::StaticFunctionTag* tag) {
            return Run(vm, stack, tag, ContextData().quest, static_cast<int>(Operation::AnimalDismiss), Animal(), -1, 0, 1, 0);
        }
        bool Managed(RE::StaticFunctionTag*, RE::Actor* actor) {
            std::scoped_lock lock(settings::Mutex);
            if (!actor) return false;
            auto object = Object(); auto slots = storage::ExtraAliases();
            slots.push_back(Read<RE::BGSRefAlias*>(object, "pFollowerAlias"));
            return std::ranges::any_of(slots, [actor](auto* slot) { return slot && slot->GetActorReference() == actor; });
        }
    }
    bool SubmitDebug(std::int32_t action, RE::Actor* actor, std::int32_t alias, std::int32_t ticket) {
        return DebugCommand(nullptr, ContextData().quest, action, actor, alias, ticket);
    }
    namespace {
        bool CanRunNow() {
            auto* main = RE::Main::GetSingleton();
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            return main && main->threadID == GetCurrentThreadId() && loaded && !pumping && !Paused() &&
                vm && !vm->IsCompletelyFrozen() && Object();
        }
    }
    bool CanStartNow() {
        return command_rules::CanStartImmediately(CanRunNow(), storage::Available(), storage::Get().active.has_value(),
            Paused(), pumping, storage::Get().queue.size());
    }
    void Kick() {
        if (CanRunNow()) Tick(); else Wake();
    }
    void RequestCounts() {
        countsDirty = true; Kick();
    }
    void Wake() {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || scheduled.exchange(true)) return;
        const auto token = generation.load();
        tasks->AddTask([token] { if (token != generation.load()) return; scheduled = false; Tick(); });
    }
    void Invalidate() { loaded = false; countsDirty = false; ++generation; scheduled = false; DeadlineScheduler().Set(false); RefreshClock(); }
    void Loaded() { loaded = true; countsDirty = true; RefreshClock(); Wake(); }
    void ObservePause() {
        std::scoped_lock lock(settings::Mutex);
        const bool paused = Paused();
        if (loaded && storage::Get().active && storage::Active().phase == Phase::Delay) {
            const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - lastTick).count();
            storage::Active().delay = executor_rules::Remaining(storage::Active().delay, elapsed, lastPaused, paused);
        }
        lastTick = std::chrono::steady_clock::now(); lastPaused = paused;
        if (loaded && !paused) Wake();
    }
    void RegisterPapyrus(RE::BSScript::IVirtualMachine* vm) {
        RegisterLatent<bool>(vm, "SetFollower", SetFollower);
        RegisterLatent<bool>(vm, "SetAnimal", SetAnimal);
        RegisterLatent<bool>(vm, "FollowerWait", FollowerWait);
        RegisterLatent<bool>(vm, "AnimalWait", AnimalWait);
        RegisterLatent<bool>(vm, "FollowerFollow", FollowerFollow);
        RegisterLatent<bool>(vm, "AnimalFollow", AnimalFollow);
        RegisterLatent<bool>(vm, "DismissFollower", DismissFollower);
        RegisterLatent<bool>(vm, "DismissAnimal", DismissAnimal);
        RegisterLatent<bool>(vm, "DismissActor", DismissActor);
        RegisterLatent<bool>(vm, "WaitActor", WaitActor);
        RegisterLatent<bool>(vm, "SetHome", SetHome);
        RegisterLatent<bool>(vm, "FollowActor", FollowActor);
        vm->RegisterFunction("IsManagedFollower", mod::info::NativeScript, Managed);
        vm->RegisterFunction("GetNativeBefriendRank", mod::info::NativeScript, Befriend);
        vm->RegisterFunction("SetNativeEffectCurrent", mod::info::NativeScript, Current);
        vm->RegisterFunction("SetNativeEffectReturned", mod::info::NativeScript, Returned);
        vm->RegisterFunction("SetNativeSpeaker", mod::info::NativeScript, Speaker);
    }
}
