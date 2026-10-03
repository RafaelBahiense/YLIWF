#include "ControllerStorage.h"
#include "StateCodec.h"
#include "Controller.h"
#include "ControllerExecutor.h"
#include "Settings.h"

#undef GetObject

namespace mod::controller::storage {
    namespace {
        constexpr std::uint32_t PluginID = 0x594C4957; // YLIW
        constexpr std::uint32_t RecordID = 0x4354524C; // CTRL
        constexpr std::uint32_t BlockedID = 0x424C434B; // BLCK: diagnostic bytes, never resumable handles
        State state;
        RE::TESQuest* quest = nullptr;
        RE::TESGlobal* partyCount = nullptr;
        RE::TESGlobal* recruitGate = nullptr;
        bool available = false, retained = false;
        std::string loadFailure;
        struct Record { std::uint32_t type, version; std::vector<std::uint8_t> bytes; };
        std::vector<Record> original;
        RE::BSScript::IObjectHandlePolicy* Policy() {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            return vm ? vm->GetObjectHandlePolicy() : nullptr;
        }
        void Retain(std::uint64_t handle) { if (handle) if (auto* policy = Policy()) policy->PersistHandle(handle); }
        void Release(std::uint64_t handle) { if (handle) if (auto* policy = Policy()) policy->ReleaseHandle(handle); }
        template <class F> void VisitActors(State& value, F visit) {
            visit(value.speaker);
            if (value.active) for (auto& step : value.active->steps) { visit(step.actor); visit(step.otherActor); }
            for (auto& command : value.queue) visit(command.actor);
        }
        std::uint64_t HandleForActor(RE::Actor* actor) {
            auto* policy = Policy();
            if (!actor || !policy) return 0;
            const auto handle = policy->GetHandleForObject(RE::FormType::ActorCharacter, actor);
            return handle == policy->EmptyHandle() ? 0 : handle;
        }
        void Save(SKSE::SerializationInterface* serialization) {
            std::scoped_lock lock(settings::Mutex);
            if (!loadFailure.empty()) {
                for (const auto& record : original) {
                    auto bytes = record.bytes;
                    if (record.type != BlockedID) {
                        StateWriter diagnostic; diagnostic.Number(record.type); diagnostic.Number(record.version);
                        diagnostic.bytes.insert(diagnostic.bytes.end(), bytes.begin(), bytes.end()); bytes = std::move(diagnostic.bytes);
                    }
                    // Raw unresolved handles belong to the original save's load-order
                    // table. Do not label them as a usable checkpoint in a new save.
                    if (!serialization->WriteRecord(BlockedID, 1, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                        logger::error("Could not preserve blocked controller save record");
                }
                return;
            }
            // Capture the remaining unpaused dismissal delay at the save boundary.
            executor::ObservePause();
            auto checkpoint = state;
            ClearUITickets(checkpoint); // Accepted commands survive loading; their UI feedback belongs to the old session.
            const auto bytes = EncodeState(checkpoint);
            if (bytes.empty() || !serialization->WriteRecord(RecordID, SaveVersion, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                logger::error("Could not save native controller state; no invalid record was written");
        }
        void Load(SKSE::SerializationInterface* serialization) {
            std::scoped_lock lock(settings::Mutex);
            Reset();
            std::uint32_t type, version, length;
            bool found = false;
            std::size_t retainedBytes = 0;
            while (serialization->GetNextRecordInfo(type, version, length)) {
                if (length > MaxSaveBytes + 8 || retainedBytes + length > MaxSaveBytes * 2 + 256 || original.size() >= 16) {
                    loadFailure = "Controller save records exceed the supported size; execution blocked."; continue;
                }
                Record record{type, version, std::vector<std::uint8_t>(length)};
                if (serialization->ReadRecordData(record.bytes.data(), length) != length) {
                    loadFailure = "Controller save record is truncated; execution blocked."; continue;
                }
                original.push_back(record);
                retainedBytes += length;
                if (type == BlockedID) { loadFailure = "Blocked native checkpoint retained for diagnosis; restore the original save pair with its matching build."; continue; }
                if (type != RecordID) { loadFailure = "Unknown controller save record; execution blocked."; continue; }
                if (found) { loadFailure = "Duplicate controller save records; execution blocked."; continue; }
                found = true;
                if (version != SaveVersion) { loadFailure = "Unsupported native controller save version; execution blocked."; continue; }
                if (!DecodeState(record.bytes, state)) loadFailure = "Invalid native controller save data; execution blocked.";
            }
            if (!loadFailure.empty()) { logger::error("{}", loadFailure); return; }
            if (!RemapState(state, [&](std::uint64_t& handle) {
                RE::VMHandle resolved = 0;
                if (!serialization->ResolveHandle(handle, resolved) || !resolved) return false;
                handle = resolved; return true;
            })) loadFailure = "A saved controller handle could not be resolved; execution blocked.";
            if (!loadFailure.empty()) logger::error("{}", loadFailure);
            logger::info("Loaded native controller: active={} phase={} queued={} callers={}", state.active.has_value(), state.active ? static_cast<int>(state.active->phase) : 0, state.queue.size(), state.callers.size());
        }
        void Revert(SKSE::SerializationInterface*) { controller::Invalidate(); }
    }
    State& Get() { return state; }
    void Configure(RE::TESQuest* value) {
        quest = value;
        auto* data = RE::TESDataHandler::GetSingleton();
        partyCount = data ? data->LookupForm<RE::TESGlobal>(0x002, mod::info::PluginFile) : nullptr;
        recruitGate = data ? data->LookupForm<RE::TESGlobal>(0x001, mod::info::PluginFile) : nullptr;
    }
    bool Available() { return available && loadFailure.empty(); }
    const std::string& LoadFailure() { return loadFailure; }
    void Reset() {
        if (retained) VisitActors(state, [](std::uint64_t& handle) { Release(handle); });
        retained = available = false;
        state = {}; original.clear(); loadFailure.clear();
    }
    void Loaded() {
        if (!loadFailure.empty()) return;
        if (!retained) VisitActors(state, [](std::uint64_t& handle) { Retain(handle); });
        retained = available = true;
    }
    void RegisterSerialization() {
        auto* serialization = SKSE::GetSerializationInterface();
        if (!serialization) { loadFailure = "SKSE serialization is unavailable."; return; }
        serialization->SetUniqueID(PluginID);
        serialization->SetSaveCallback(Save); serialization->SetLoadCallback(Load); serialization->SetRevertCallback(Revert);
    }
    RE::Actor* Actor(std::uint64_t handle) {
        auto* policy = Policy();
        if (!handle || !policy || !policy->HandleIsType(RE::FormType::ActorCharacter, handle) || !policy->IsHandleObjectAvailable(handle)) return nullptr;
        return static_cast<RE::Actor*>(policy->GetObjectForHandle(RE::FormType::ActorCharacter, handle));
    }
    Handle ActorHandle(RE::Actor* actor) { return HandleForActor(actor); }
    void SetQueue(std::vector<Command> queue) {
        for (const auto& command : queue) Retain(command.actor);
        for (const auto& command : state.queue) Release(command.actor);
        state.queue = std::move(queue);
    }
    void SetActive(std::optional<ActiveOperation> active) {
        if (active) for (const auto& step : active->steps) { Retain(step.actor); Retain(step.otherActor); }
        if (state.active) for (const auto& step : state.active->steps) { Release(step.actor); Release(step.otherActor); }
        state.active = std::move(active);
    }
    bool SetActor(std::uint64_t& destination, RE::Actor* actor) {
        const auto handle = HandleForActor(actor);
        if (actor && !handle) return false;
        Retain(handle); Release(destination); destination = handle; return true;
    }
    Caller CaptureCaller(RE::VMStackID id, std::int32_t request) {
        Caller caller{request, id};
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        RE::BSScript::Stack* stack = nullptr;
        if (!vm || !vm->GetStackByID(id, &stack) || !stack || !stack->top || !stack->top->owningFunction) return caller;
        caller.type = stack->top->owningFunction->GetObjectTypeName().c_str();
        caller.function = stack->top->owningFunction->GetName().c_str();
        if (stack->top->self.IsObject()) if (auto self = (stack->top->self.GetObject)()) caller.self = self->GetHandle();
        if (auto* policy = Policy(); policy && caller.self == policy->EmptyHandle()) caller.self = 0;
        return caller;
    }
    bool CurrentCaller(const Caller& caller) {
        const auto current = CaptureCaller(caller.stack, caller.request);
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        RE::BSScript::Stack* stack = nullptr;
        return vm && vm->GetStackByID(caller.stack, &stack) && stack &&
            stack->state == RE::BSScript::Stack::State::kWaitingOnLatentFunction && SameCaller(caller, current);
    }
    std::vector<RE::BGSRefAlias*> ExtraAliases() {
        std::vector<RE::BGSRefAlias*> aliases(7);
        if (quest) for (auto* base : quest->aliases) if (base && base->aliasID >= 2 && base->aliasID <= 8)
            aliases[base->aliasID - 2] = skyrim_cast<RE::BGSRefAlias*>(base);
        return aliases;
    }
    RE::TESGlobal* PartyCount() { return partyCount; }
    RE::TESGlobal* RecruitGate() { return recruitGate; }
}
