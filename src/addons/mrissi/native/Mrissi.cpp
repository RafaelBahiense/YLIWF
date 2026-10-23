#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include "ModInfo.h"
#include "AddonInfo.h"
#include "PapyrusController.h"
#include "Rules.h"
#include "PapyrusCommands.h"
#include <array>
#include <cstdio>

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
    papyrus::Commands commands;

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
                              (!interrupt || (host && host->isMovementPackage(interrupt->GetFormID()) == 1));
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
             .pending = commands.Pending(actor)});
        return 1;
    }

    bool VerifyCommand(ActorID actor, Command command) {
        FollowerState state;
        return Inspect(nullptr, actor, &state) && Reached(state, command);
    }

    std::uint32_t MovementPermissions(void*, ActorID actor, std::uint32_t ownerPackage, const FollowerState* state) {
        if (!mrissi || actor != mrissi->GetFormID() || !state || !Supports(*state, Command::Follow) ||
            commands.Pending(actor))
            return 0;
        if (RestrictedLocation())
            return 0;
        if (ordinaryPackages[0] && ordinaryPackages[0]->GetFormID() == ownerPackage)
            return AllMovement;
        for (std::size_t i = 1; i < 3; ++i)
            if (ordinaryPackages[i] && ordinaryPackages[i]->GetFormID() == ownerPackage)
                return MovementBit(Movement::StandDown);
        return 0;
    }

    StartResult Start(void*, ActorID actor, Command command, RequestID request, char* reason, std::uint32_t capacity) {
        FollowerState state;
        if (!Inspect(nullptr, actor, &state) || !Supports(state, command)) {
            Reason(reason, capacity, state.reason[0] ? state.reason : "M'rissi cannot obey this command now.");
            return StartResult::Rejected;
        }
        if (Reached(state, command))
            return StartResult::Completed;
        return commands.Dispatch(papyrus::Bound(dialogue, Script), Methods[static_cast<std::size_t>(command)], host,
                                 registration, actor, command, request, VerifyCommand);
    }

    void NotifyState() {
        if (registration && CanNotifyState(host))
            host->stateChanged(registration);
    }

    std::uint32_t CanReceiveCombatProtection(void*, ActorID actor) {
        FollowerState state;
        return Inspect(nullptr, actor, &state) && Supports(state, Command::Follow) ? 1u : 0u;
    }

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPostLoad) {
            const auto module = GetModuleHandleA(mod::info::DllFile);
            const auto query = module ? reinterpret_cast<QueryAPI>(GetProcAddress(module, QueryExport)) : nullptr;
            host = query ? query(mod::addon::RequiredAdapterAPIVersion) : nullptr;
            if (!host || host->size < BaseAPISize || host->version != InterfaceVersion || !host->registerAdapter ||
                !host->complete || !HasCoreMovement(host)) {
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
                                  nullptr,
                                  nullptr,
                                  CanReceiveCombatProtection,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  MovementPermissions,
                                  AllMovement};
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
            if (!mrissi || !dialogue || !noFollower || !broken || !dismissed || !mountHandling || !restrictedLocations)
                SKSE::log::warn("Mrissi adapter: required MrissiTailOfTroubles.esp records are unavailable");
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame) {
            commands.Invalidate();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            NotifyState();
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    return messages && messages->RegisterListener(OnMessage);
}
