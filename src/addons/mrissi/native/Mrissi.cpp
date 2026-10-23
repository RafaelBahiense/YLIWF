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
    constexpr char Plugin[] = "MrissiTailOfTroubles.esp";
    constexpr char Script[] = "MRTDialogueFollowerScript";
    constexpr std::array Methods{"FollowerFollow", "FollowerWait", "DismissFollower"};
    constexpr std::uint32_t ActorForm = 0x1AEF15;
    constexpr std::uint32_t DialogueForm = 0x033233;
    constexpr std::array OrdinaryPackages{0x033234u, 0x047663u, 0x047662u};
    const API* host = nullptr;
    AdapterID registration = 0;
    RE::Actor* mrissi = nullptr;
    RE::TESQuest* dialogue = nullptr;
    RE::TESFaction* noFollower = nullptr;
    RE::TESFaction* broken = nullptr;
    RE::TESFaction* dismissed = nullptr;
    RE::TESFaction* mountHandling = nullptr;
    RE::BGSListForm* restrictedLocations = nullptr;
    std::array<RE::TESPackage*, OrdinaryPackages.size()> ordinaryPackages{};
    std::uint64_t generation = 0;
    bool commandPending = false;
    std::atomic_bool observeMovement{false};
    std::atomic_bool observationQueued{false};

    void Reason(char* output, std::uint32_t capacity, const char* text) {
        if (output && capacity)
            std::snprintf(output, capacity, "%s", text);
    }

    RE::BGSRefAlias* FollowerAlias(const papyrus::Object& controller) {
        auto* binding = papyrus::Field(controller, "pFollowerAlias");
        auto* alias = binding && binding->IsObject() ? binding->Unpack<RE::BGSRefAlias*>() : nullptr;
        return alias && alias->owningQuest == dialogue && alias->aliasID == 0 ? alias : nullptr;
    }

    bool Compatible(const papyrus::Object& controller) {
        auto* faction = papyrus::Field(controller, "pDismissedFollower");
        if (!FollowerAlias(controller) || !faction || !faction->IsObject() ||
            faction->Unpack<RE::TESFaction*>() != dismissed)
            return false;
        for (const auto* method : Methods)
            if (!papyrus::HasMethod(controller, method))
                return false;
        return true;
    }

    bool RestrictedLocation() {
        if (!restrictedLocations)
            return true;
        auto* location = mrissi ? mrissi->GetCurrentLocation() : nullptr;
        for (unsigned depth = 0; location && depth < 128; ++depth, location = location->parentLoc)
            if (restrictedLocations->HasForm(location))
                return true;
        return false;
    }

    std::uint32_t Enumerate(void*, ActorID* actors, std::uint32_t capacity) {
        if (!mrissi || !actors || !capacity)
            return 0;
        actors[0] = mrissi->GetFormID();
        return 1;
    }

    std::uint32_t Inspect(void*, ActorID actor, FollowerState* output) {
        if (!mrissi || actor != mrissi->GetFormID() || !output || output->size < sizeof(*output))
            return 0;
        *output = {};
        if (mrissi->IsDead()) {
            *output = mod::mrissi_rules::Describe({.dead = true});
            return 1;
        }
        const auto controller = papyrus::Bound(dialogue, Script);
        if (!dialogue || !noFollower || !broken || !dismissed || !mountHandling || !restrictedLocations ||
            !dialogue->IsRunning() || !Compatible(controller)) {
            Reason(output->reason, sizeof(output->reason),
                   "M'rissi's controller records or bindings are unavailable or incompatible.");
            return 1;
        }
        auto* alias = FollowerAlias(controller);
        auto* owner = alias->GetReference();
        if (owner && owner != mrissi) {
            Reason(output->reason, sizeof(output->reason), "M'rissi's follower alias is occupied by another actor.");
            return 1;
        }
        const auto* process = mrissi->GetActorRuntimeData().currentProcess;
        const auto* package = process ? process->currentPackage.package : mrissi->GetCurrentPackage();
        const auto* interrupt = process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        const bool ordinary = package && std::ranges::find(ordinaryPackages, package) != ordinaryPackages.end() &&
                              (!interrupt || mod::mrissi_movement::Owns(interrupt));
        *output = mod::mrissi_rules::Describe(
            {.initialized = true,
             .dead = false,
             .recruited = owner == mrissi,
             .teammate = mrissi->IsPlayerTeammate(),
             .dismissed = mrissi->IsInFaction(dismissed),
             .waiting = mrissi->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer),
             .questLocked = mrissi->IsInFaction(noFollower),
             .broken = mrissi->IsInFaction(broken),
             .restrictedLocation = RestrictedLocation(),
             .scene = mrissi->GetCurrentScene() != nullptr,
             .ordinaryPackage = ordinary,
             .incapacitated = mrissi->IsDisabled() || mrissi->AsActorState()->IsBleedingOut() ||
                              mrissi->AsActorState()->IsUnconscious(),
             .mounting = mrissi->GetFactionRank(mountHandling, false) > 0,
             .pending = commandPending});
        return 1;
    }

    mod::mrissi_movement_rules::Snapshot MovementState() {
        auto snapshot = mod::mrissi_movement::Read(mrissi);
        FollowerState state;
        if (mrissi && Inspect(nullptr, mrissi->GetFormID(), &state)) {
            snapshot.following = state.state == State::Following;
            snapshot.waiting = state.state == State::Waiting;
            snapshot.controllerAllows = Supports(state, Command::Follow) && !RestrictedLocation();
        }
        snapshot.pending = commandPending;
        snapshot.ridingRequested = !mrissi || !mountHandling || mrissi->GetFactionRank(mountHandling, false) > 0;
        return snapshot;
    }

    void RefreshMovement() {
        auto* ui = RE::UI::GetSingleton();
        const auto* main = RE::Main::GetSingleton();
        if (!observeMovement || !ui || ui->GameIsPaused() || !main || main->freezeTime)
            return;
        mod::mrissi_movement::Refresh(mrissi, MovementState());
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
                                       verified ? "M'rissi's method returned and follower state matches."
                                                : "M'rissi's method returned without reaching the requested state.");
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
            Reason(reason, capacity, state.reason[0] ? state.reason : "M'rissi cannot obey this command now.");
            return StartResult::Rejected;
        }
        if (Reached(state, command))
            return StartResult::Completed;
        auto object = papyrus::Bound(dialogue, Script);
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !object)
            return StartResult::Rejected;
        mod::mrissi_movement::SetFormation(mrissi, 0);
        mod::mrissi_movement::SetStandDown(mrissi, 0);
        mod::mrissi_movement::Release(mrissi);
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
        if (!Inspect(nullptr, actor, &state) || !Supports(state, Command::Follow) || !mod::mrissi_movement::Ready() ||
            !mod::mrissi_movement::SetDistance(distance))
            return 0;
        mod::mrissi_movement::Refresh(mrissi, MovementState());
        return 1;
    }

    std::uint32_t CanTravelInFormation(void*, ActorID actor) {
        return mrissi && actor == mrissi->GetFormID() && mod::mrissi_movement::Ready() &&
                       mod::mrissi_movement_rules::Permitted(MovementState())
                   ? 1u
                   : 0u;
    }

    std::uint32_t SetFormationPackage(void*, ActorID actor, std::uint32_t package) {
        if (!mrissi || actor != mrissi->GetFormID() || (package && !CanTravelInFormation(nullptr, actor)))
            return 0;
        if (!mod::mrissi_movement::SetFormation(mrissi, package))
            return 0;
        mod::mrissi_movement::Refresh(mrissi, MovementState());
        if (package && !mod::mrissi_movement::FormationActive(mrissi, package)) {
            mod::mrissi_movement::SetFormation(mrissi, 0);
            return 0;
        }
        return 1;
    }

    std::uint32_t CanReceiveCombatProtection(void*, ActorID actor) {
        FollowerState state;
        return Inspect(nullptr, actor, &state) && Supports(state, Command::Follow) ? 1u : 0u;
    }

    std::uint32_t SetStandDownPackage(void*, ActorID actor, std::uint32_t package) {
        if (!mrissi || actor != mrissi->GetFormID() ||
            (package && !mod::mrissi_movement_rules::StandDownPermitted(MovementState())))
            return 0;
        if (!mod::mrissi_movement::SetStandDown(mrissi, package))
            return 0;
        mod::mrissi_movement::Refresh(mrissi, MovementState());
        if (package && !mod::mrissi_movement::StandDownActive(mrissi, package)) {
            mod::mrissi_movement::SetStandDown(mrissi, 0);
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
                SKSE::log::warn("Mrissi adapter: compatible YLIWF host API is unavailable");
                return;
            }
            const Adapter adapter{sizeof(Adapter),
                                  InterfaceVersion,
                                  "yliwf.mrissi",
                                  "M'rissi",
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
                SKSE::log::warn("Mrissi adapter registration rejected");
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (!registration)
                return;
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                mrissi = data->LookupForm<RE::Actor>(ActorForm, Plugin);
                dialogue = data->LookupForm<RE::TESQuest>(DialogueForm, Plugin);
                noFollower = data->LookupForm<RE::TESFaction>(0x033236, Plugin);
                broken = data->LookupForm<RE::TESFaction>(0x2AD67D, Plugin);
                dismissed = data->LookupForm<RE::TESFaction>(0x05C84C, "Skyrim.esm");
                mountHandling = data->LookupForm<RE::TESFaction>(0x047657, Plugin);
                restrictedLocations = data->LookupForm<RE::BGSListForm>(0x2AD680, Plugin);
                for (std::size_t i = 0; i < ordinaryPackages.size(); ++i)
                    ordinaryPackages[i] = data->LookupForm<RE::TESPackage>(OrdinaryPackages[i], Plugin);
            }
            mod::mrissi_movement::Configure(ordinaryPackages[0], ordinaryPackages[1], ordinaryPackages[2]);
            if (mrissi)
                StartMovementObservation();
            if (!mrissi || !dialogue || !noFollower || !broken || !dismissed || !mountHandling || !restrictedLocations)
                SKSE::log::warn("Mrissi adapter: required MrissiTailOfTroubles.esp records are unavailable");
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            ++generation;
            observeMovement = false;
            commandPending = false;
            mod::mrissi_movement::Reset(mrissi);
            if (message->type == SKSE::MessagingInterface::kNewGame)
                observeMovement = mrissi != nullptr;
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            mod::mrissi_movement::SetFormation(mrissi, 0);
            mod::mrissi_movement::Release(mrissi);
            observeMovement = mrissi != nullptr;
            NotifyState();
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    return messages && messages->RegisterListener(OnMessage);
}
