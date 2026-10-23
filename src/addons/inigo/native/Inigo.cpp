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
    papyrus::Commands commands;

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

    bool VerifyCommand(ActorID actor, Command command) {
        FollowerState state;
        return Inspect(nullptr, actor, &state) && Reached(state, command);
    }

    std::uint32_t MovementPermissions(void*, ActorID actor, std::uint32_t ownerPackage, const FollowerState* state) {
        if (!inigo || actor != inigo->GetFormID() || !state || !Supports(*state, Command::Follow) ||
            commands.Pending(actor))
            return 0;
        const auto riding = papyrus::Boolean(papyrus::Bound(statusQuest, "InigoStatusQuestScript"), "preferRiding");
        if (!horseFollowing || !rideWithoutPlayer || !riding || *riding ||
            inigo->GetFactionRank(horseFollowing, false) > 0 || inigo->GetFactionRank(rideWithoutPlayer, false) > 0)
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
            Reason(reason, capacity, state.reason[0] ? state.reason : "Inigo cannot obey this command now.");
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

    std::uint32_t SetFollowDistance(void*, ActorID actor, FollowDistance distance) {
        FollowerState state;
        const auto rank = mod::inigo_rules::DistanceRank(distance);
        if (!rank || !Inspect(nullptr, actor, &state) || !Supports(state, Command::Follow))
            return 0;
        // Inigo's SetFollowDistance(Int) performs exactly this faction write.
        // The host callback is synchronous; no Papyrus call is queued here.
        if (inigo->GetFactionRank(distanceFaction, false) != *rank) {
            inigo->AddToFaction(distanceFaction, *rank);
            inigo->EvaluatePackage();
            NotifyState();
        }
        return 1;
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
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  MovementPermissions,
                                  AllMovement};
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
            if (!inigo || !dialogue || !statusQuest || !badVibrations || !distanceFaction)
                SKSE::log::warn("Inigo adapter: required Inigo.esp records are unavailable");
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
