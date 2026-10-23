#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include "ModInfo.h"
#include "AddonInfo.h"
#include "PapyrusController.h"
#include "Rules.h"
#include "Movement.h"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
    using namespace yliwf::sdk;
    static_assert(mod::addon::RequiredAdapterAPIVersion == InterfaceVersion);
    constexpr char Plugin[] = "Inigo.esp";
    constexpr char Script[] = "InigofollowerDialogue";
    constexpr std::array Methods{"FollowMe", "WaitHere", "Dismiss"};
    constexpr std::uint32_t ActorForm = 0x008AE9;
    constexpr std::uint32_t DialogueForm = 0x0012DF;
    constexpr std::uint32_t StatusForm = 0x09D37D;
    constexpr std::uint32_t BadVibrationsForm = 0x0695C5;
    constexpr std::uint32_t DistanceForm = 0x04EA5E;
    constexpr std::array OrdinaryPackages{0x050AEDu, 0x056698u, 0x075B89u, 0x05669Au};
    const API* host = nullptr;
    AdapterID registration = 0;
    RE::Actor* inigo = nullptr;
    RE::TESQuest* dialogue = nullptr;
    RE::TESQuest* statusQuest = nullptr;
    RE::TESQuest* badVibrations = nullptr;
    RE::TESFaction* distanceFaction = nullptr;
    RE::TESFaction* horseFollowing = nullptr;
    RE::TESFaction* rideWithoutPlayer = nullptr;
    std::array<RE::TESPackage*, OrdinaryPackages.size()> ordinaryPackages{};
    std::uint64_t generation = 0;
    bool commandPending = false;
    std::atomic_bool observeMovement{false};
    std::atomic_bool observationQueued{false};

    void Reason(char* output, std::uint32_t capacity, const char* text) {
        if (output && capacity)
            std::snprintf(output, capacity, "%s", text);
    }

    bool Compatible(const papyrus::Object& controller) {
        auto* actor = papyrus::Field(controller, "InigoRef");
        auto* distance = papyrus::Field(controller, "InigoFollowDistanceFaction");
        RE::BGSBaseAlias* alias = nullptr;
        if (statusQuest)
            for (auto* candidate : statusQuest->aliases)
                if (candidate && candidate->aliasID == 1) {
                    alias = candidate;
                    break;
                }
        auto* reference = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
        if (!actor || !actor->IsObject() || actor->Unpack<RE::Actor*>() != inigo || !distance ||
            !distance->IsObject() || distance->Unpack<RE::TESFaction*>() != distanceFaction || !reference ||
            reference->GetActorReference() != inigo)
            return false;
        for (const auto* method : Methods)
            if (!papyrus::HasMethod(controller, method))
                return false;
        return true;
    }

    std::uint32_t Enumerate(void*, ActorID* actors, std::uint32_t capacity) {
        if (!inigo || !actors || !capacity)
            return 0;
        actors[0] = inigo->GetFormID();
        return 1;
    }

    std::uint32_t Inspect(void*, ActorID actor, FollowerState* output) {
        if (!inigo || actor != inigo->GetFormID() || !output || output->size < sizeof(*output))
            return 0;
        *output = {};
        if (!dialogue || !statusQuest || !badVibrations || !distanceFaction) {
            Reason(output->reason, sizeof(output->reason), "Inigo's controller records are unavailable.");
            return 1;
        }
        if (inigo->IsDead() || dialogue->currentStage < 10) {
            *output = mod::inigo_rules::Describe({.dead = inigo->IsDead()});
            return 1;
        }
        if (!dialogue->IsRunning() || !statusQuest->IsRunning() || !Compatible(papyrus::Bound(dialogue, Script))) {
            Reason(output->reason, sizeof(output->reason), "Inigo's controller or actor bindings are incompatible.");
            return 1;
        }
        const auto* process = inigo->GetActorRuntimeData().currentProcess;
        const auto* package = process ? process->currentPackage.package : inigo->GetCurrentPackage();
        const bool ordinary = package && std::ranges::find(ordinaryPackages, package) != ordinaryPackages.end();
        *output = mod::inigo_rules::Describe(
            {.initialized = true,
             .teammate = inigo->IsPlayerTeammate(),
             .waiting = inigo->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer),
             .questLocked = badVibrations->currentStage == 140,
             .scene = inigo->GetCurrentScene() != nullptr,
             .ordinaryPackage = ordinary,
             .incapacitated = inigo->IsDisabled() || inigo->AsActorState()->IsBleedingOut() ||
                              inigo->AsActorState()->IsUnconscious()});
        return 1;
    }

    mod::inigo_movement_rules::Snapshot MovementState() {
        auto snapshot = mod::inigo_movement::Read(inigo);
        FollowerState state;
        if (inigo && Inspect(nullptr, inigo->GetFormID(), &state)) {
            snapshot.following = state.state == State::Following;
            snapshot.waiting = state.state == State::Waiting;
            snapshot.controllerAllows = Supports(state, Command::Follow);
        }
        snapshot.pending = commandPending;
        const auto riding = papyrus::Boolean(papyrus::Bound(statusQuest, "InigoStatusQuestScript"), "preferRiding");
        snapshot.ridingRequested = !inigo || !horseFollowing || !rideWithoutPlayer || !riding || *riding ||
                                   inigo->GetFactionRank(horseFollowing, false) > 0 ||
                                   inigo->GetFactionRank(rideWithoutPlayer, false) > 0;
        return snapshot;
    }

    void RefreshMovement() {
        auto* ui = RE::UI::GetSingleton();
        const auto* main = RE::Main::GetSingleton();
        if (!observeMovement || !ui || ui->GameIsPaused() || !main || main->freezeTime)
            return;
        mod::inigo_movement::Refresh(inigo, MovementState());
    }

    void StartMovementObservation() {
        // The worker only schedules a bounded main-thread task. Engine and VM
        // snapshots, package writes and session state stay on the game thread.
        static std::jthread worker([](std::stop_token stop) {
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks)
                return;
            std::mutex mutex;
            std::condition_variable_any changed;
            std::unique_lock lock(mutex);
            while (!stop.stop_requested()) {
                changed.wait_for(lock, stop, std::chrono::milliseconds(250), [] { return false; });
                if (stop.stop_requested())
                    break;
                if (observeMovement && !observationQueued.exchange(true))
                    tasks->AddTask([] {
                        observationQueued = false;
                        RefreshMovement();
                    });
            }
        });
    }

    class Completion final : public RE::BSScript::IStackCallbackFunctor {
    public:
        Completion(RequestID request, ActorID actor, Command command)
            : request(request), actor(actor), command(command), epoch(generation) {}

        void operator()(RE::BSScript::Variable) override {
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([request = request, actor = actor, command = command, epoch = epoch] {
                    if (epoch != generation)
                        return;
                    commandPending = false;
                    FollowerState state;
                    const bool verified = Inspect(nullptr, actor, &state) && Reached(state, command);
                    if (host)
                        host->complete(registration, request, verified ? 1 : 0,
                                       verified ? "Inigo's method returned and follower state matches."
                                                : "Inigo's method returned without reaching the requested state.");
                });
        }

        void SetObject(const papyrus::Object& value) override {
            object = value;
        }

    private:
        RequestID request;
        ActorID actor;
        Command command;
        std::uint64_t epoch;
        papyrus::Object object;
    };

    StartResult Start(void*, ActorID actor, Command command, RequestID request, char* reason, std::uint32_t capacity) {
        FollowerState state;
        if (!Inspect(nullptr, actor, &state) || !Supports(state, command)) {
            Reason(reason, capacity, state.reason[0] ? state.reason : "Inigo cannot obey this command now.");
            return StartResult::Rejected;
        }
        if (Reached(state, command))
            return StartResult::Completed;
        auto object = papyrus::Bound(dialogue, Script);
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !object)
            return StartResult::Rejected;
        mod::inigo_movement::SetFormation(inigo, 0);
        mod::inigo_movement::SetStandDown(inigo, 0);
        mod::inigo_movement::Release(inigo);
        commandPending = true;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> completion{new Completion(request, actor, command)};
        if (vm->DispatchMethodCall(object, Methods[static_cast<std::size_t>(command)], RE::MakeFunctionArguments(),
                                   completion))
            return StartResult::Pending;
        commandPending = false;
        return StartResult::Rejected;
    }

    void NotifyState() {
        if (registration && CanNotifyState(host))
            host->stateChanged(registration);
    }

    std::uint32_t SetFollowDistance(void*, ActorID actor, FollowDistance distance) {
        FollowerState state;
        const auto rank = mod::inigo_rules::DistanceRank(distance);
        if (!rank || !Inspect(nullptr, actor, &state) || !Supports(state, Command::Follow) ||
            !mod::inigo_movement::Ready() || !mod::inigo_movement::SetDistance(distance))
            return 0;
        // Inigo's SetFollowDistance(Int) performs exactly this faction write.
        // The host callback is synchronous; no Papyrus call is queued here.
        if (inigo->GetFactionRank(distanceFaction, false) != *rank) {
            inigo->AddToFaction(distanceFaction, *rank);
            inigo->EvaluatePackage();
            NotifyState();
        }
        mod::inigo_movement::Refresh(inigo, MovementState());
        return 1;
    }

    std::uint32_t CanTravelInFormation(void*, ActorID actor) {
        return inigo && actor == inigo->GetFormID() && mod::inigo_movement::Ready() &&
                       mod::inigo_movement_rules::Permitted(MovementState())
                   ? 1u
                   : 0u;
    }

    std::uint32_t SetFormationPackage(void*, ActorID actor, std::uint32_t package) {
        if (!inigo || actor != inigo->GetFormID() || (package && !CanTravelInFormation(nullptr, actor)))
            return 0;
        if (!mod::inigo_movement::SetFormation(inigo, package))
            return 0;
        mod::inigo_movement::Refresh(inigo, MovementState());
        if (package && !mod::inigo_movement::FormationActive(inigo, package)) {
            mod::inigo_movement::SetFormation(inigo, 0);
            return 0;
        }
        return 1;
    }

    std::uint32_t CanReceiveCombatProtection(void*, ActorID actor) {
        FollowerState state;
        return Inspect(nullptr, actor, &state) && Supports(state, Command::Follow) ? 1u : 0u;
    }

    std::uint32_t SetStandDownPackage(void*, ActorID actor, std::uint32_t package) {
        if (!inigo || actor != inigo->GetFormID() ||
            (package && !mod::inigo_movement_rules::StandDownPermitted(MovementState())))
            return 0;
        if (!mod::inigo_movement::SetStandDown(inigo, package))
            return 0;
        mod::inigo_movement::Refresh(inigo, MovementState());
        if (package && !mod::inigo_movement::StandDownActive(inigo, package)) {
            mod::inigo_movement::SetStandDown(inigo, 0);
            return 0;
        }
        return 1;
    }

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPostLoad) {
            const auto module = GetModuleHandleA(mod::info::DllFile);
            const auto query = module ? reinterpret_cast<QueryAPI>(GetProcAddress(module, QueryExport)) : nullptr;
            host = query ? query(mod::addon::RequiredAdapterAPIVersion) : nullptr;
            if (!host || host->size < BaseAPISize || host->version != InterfaceVersion || !host->registerAdapter ||
                !host->complete) {
                SKSE::log::warn("Inigo adapter: compatible YLIWF host API is unavailable");
                return;
            }
            const Adapter adapter{sizeof(Adapter),
                                  InterfaceVersion,
                                  "yliwf.inigo",
                                  "Inigo",
                                  nullptr,
                                  Enumerate,
                                  Inspect,
                                  Start,
                                  SetFollowDistance,
                                  nullptr,
                                  CanReceiveCombatProtection,
                                  CanTravelInFormation,
                                  SetFormationPackage,
                                  SetStandDownPackage};
            registration = host->registerAdapter(&adapter);
            if (!registration)
                SKSE::log::warn("Inigo adapter registration rejected");
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (!registration)
                return;
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                inigo = data->LookupForm<RE::Actor>(ActorForm, Plugin);
                dialogue = data->LookupForm<RE::TESQuest>(DialogueForm, Plugin);
                statusQuest = data->LookupForm<RE::TESQuest>(StatusForm, Plugin);
                badVibrations = data->LookupForm<RE::TESQuest>(BadVibrationsForm, Plugin);
                distanceFaction = data->LookupForm<RE::TESFaction>(DistanceForm, Plugin);
                horseFollowing = data->LookupForm<RE::TESFaction>(0x04EA5F, Plugin);
                rideWithoutPlayer = data->LookupForm<RE::TESFaction>(0x075793, Plugin);
                for (std::size_t i = 0; i < ordinaryPackages.size(); ++i)
                    ordinaryPackages[i] = data->LookupForm<RE::TESPackage>(OrdinaryPackages[i], Plugin);
            }
            mod::inigo_movement::Configure(ordinaryPackages[0], ordinaryPackages[1], ordinaryPackages[2]);
            if (inigo)
                StartMovementObservation();
            if (!inigo || !dialogue || !statusQuest || !badVibrations || !distanceFaction)
                SKSE::log::warn("Inigo adapter: required Inigo.esp records are unavailable");
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            ++generation;
            observeMovement = false;
            commandPending = false;
            mod::inigo_movement::Reset(inigo);
            if (message->type == SKSE::MessagingInterface::kNewGame)
                observeMovement = inigo != nullptr;
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            mod::inigo_movement::SetFormation(inigo, 0);
            mod::inigo_movement::Release(inigo);
            observeMovement = inigo != nullptr;
            NotifyState();
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    return messages && messages->RegisterListener(OnMessage);
}
