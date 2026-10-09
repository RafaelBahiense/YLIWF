#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include "ModInfo.h"
#include "Rules.h"
#include "sdk/PapyrusController.h"
#include <array>
#include <cstdio>

namespace {
    using namespace yliwf::sdk;
    const API* host = nullptr;
    AdapterID registration = 0;
    RE::TESQuest* mentalModel = nullptr;
    RE::Actor* serana = nullptr;
    constexpr char Script[] = "DLC1_NPCMentalModelScript";
    constexpr std::array Methods{"StopWaiting", "Wait", "Dismiss"};

    void Reason(char* output, std::uint32_t capacity, const char* text) {
        if (output && capacity)
            std::snprintf(output, capacity, "%s", text);
    }

    std::uint32_t Enumerate(void*, ActorID* actors, std::uint32_t capacity) {
        if (!serana || !actors || !capacity)
            return 0;
        actors[0] = serana->GetFormID();
        return 1;
    }

    std::uint32_t Inspect(void*, ActorID actor, FollowerState* output) {
        if (!serana || actor != serana->GetFormID() || !output || output->size < sizeof(*output))
            return 0;
        *output = {};
        // Yeah, this SHOULD never happen, but let's be safe.
        if (serana->IsDead()) {
            Reason(output->reason, sizeof(output->reason), "Serana is dead");
            return 1;
        }
        if (!mentalModel || !mentalModel->IsRunning()) {
            Reason(output->reason, sizeof(output->reason), "Dawnguard's follower quest is not running");
            return 1;
        }
        auto object = papyrus::Bound(mentalModel, Script);
        if (!object) {
            Reason(output->reason, sizeof(output->reason), "Dawnguard's follower controller is not initialized");
            return 1;
        }
        const auto following = papyrus::Boolean(object, "IsFollowing"), waiting = papyrus::Boolean(object, "IsWaiting"),
                   dismissed = papyrus::Boolean(object, "IsDismissed"), locked = papyrus::Boolean(object, "LockedIn"),
                   willing = papyrus::Boolean(object, "IsWillingToWait"),
                   dismissable = papyrus::Boolean(object, "CanBeDismissed"),
                   canFollow = papyrus::Boolean(object, "CanFollow");
        auto* actorBinding = papyrus::Field(object, "Serana");
        auto* aliasBinding = papyrus::Field(object, "RNPC");
        auto* alias = aliasBinding && aliasBinding->IsObject() ? aliasBinding->Unpack<RE::BGSRefAlias*>() : nullptr;
        if (!following || !waiting || !dismissed || !locked || !willing || !dismissable || !canFollow ||
            !actorBinding || !actorBinding->IsObject() || actorBinding->Unpack<RE::Actor*>() != serana || !alias ||
            alias->owningQuest != mentalModel || alias->GetActorReference() != serana) {
            Reason(output->reason, sizeof(output->reason),
                   "Dawnguard controller properties or actor bindings are incompatible");
            return 1;
        }
        for (const auto* method : Methods)
            if (!papyrus::HasMethod(object, method)) {
                Reason(output->reason, sizeof(output->reason), "Dawnguard controller methods are incompatible");
                return 1;
            }
        *output = mod::serana_rules::Describe(
            {*following, *waiting, *dismissed, *locked, *willing, *dismissable, *canFollow});
        if (output->state != State::Inactive &&
            (!serana->IsPlayerTeammate() || serana->AsActorValueOwner()->GetActorValue(
                                                RE::ActorValue::kWaitingForPlayer) != (*waiting ? 1.0f : 0.0f))) {
            output->state = State::Unavailable;
            output->commands = 0;
            Reason(output->reason, sizeof(output->reason),
                   "Dawnguard controller and actor state disagree; no automatic repair is applied");
        }
        return 1;
    }

    class Completion final : public RE::BSScript::IStackCallbackFunctor {
    public:
        Completion(RequestID request, ActorID actor, Command command)
            : request(request), actor(actor), command(command) {}

        void operator()(RE::BSScript::Variable) override {
            // VM callbacks may run on a worker. Observe engine state on the main thread.
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([request = request, actor = actor, command = command] {
                    FollowerState state;
                    const bool verified =
                        Inspect(nullptr, actor, &state) && Reached(state, command) &&
                        (command != Command::Dismiss ||
                         (!serana->IsPlayerTeammate() &&
                          papyrus::Boolean(papyrus::Bound(mentalModel, Script), "IsDismissed").value_or(false)));
                    if (host)
                        host->complete(registration, request, verified ? 1 : 0,
                                       verified ? "Dawnguard method returned and follower state matches."
                                                : "Dawnguard method returned without reaching the requested state.");
                });
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>& value) override {
            object = value;
        }

    private:
        RequestID request;
        ActorID actor;
        Command command;
        papyrus::Object object;
    };

    StartResult Start(void*, ActorID actor, Command command, RequestID request, char* reason, std::uint32_t capacity) {
        FollowerState state;
        if (!Inspect(nullptr, actor, &state) || !Supports(state, command)) {
            Reason(reason, capacity, state.reason[0] ? state.reason : "Serana cannot obey this command now");
            return StartResult::Rejected;
        }
        if (Reached(state, command))
            return StartResult::Completed;
        auto object = papyrus::Bound(mentalModel, Script);
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !object)
            return StartResult::Rejected;
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> completion{new Completion(request, actor, command)};
        return vm->DispatchMethodCall(object, Methods[static_cast<std::size_t>(command)], RE::MakeFunctionArguments(),
                                      completion)
                   ? StartResult::Pending
                   : StartResult::Rejected;
    }

    void NotifyState() {
        if (registration && CanNotifyState(host))
            host->stateChanged(registration);
    }

    std::uint32_t SetFollowDistance(void*, ActorID actor, FollowDistance distance) {
        FollowerState state;
        if (!Inspect(nullptr, actor, &state) || !Supports(state, Command::Follow) ||
            static_cast<unsigned>(distance) > 2)
            return 0;
        auto object = papyrus::Bound(mentalModel, Script);
        constexpr std::array names{"FollowDistanceFar", "FollowDistanceMedium", "FollowDistanceClose"};
        std::array<RE::BSScript::Variable*, 3> fields{};
        for (std::size_t i = 0; i < names.size(); ++i) {
            fields[i] = papyrus::Field(object, names[i]);
            if (!fields[i] || !fields[i]->IsBool())
                return 0;
        }
        const auto values = mod::serana_rules::DistanceFlags(distance);
        bool changed = false;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (fields[i]->GetBool() == (*values)[i])
                continue;
            fields[i]->SetBool((*values)[i]);
            changed = true;
        }
        if (changed) {
            serana->EvaluatePackage();
            NotifyState();
        }
        return 1;
    }

    class StateEvents final : public RE::BSTEventSink<RE::TESQuestStageEvent>,
                              public RE::BSTEventSink<RE::TESQuestStartStopEvent>,
                              public RE::BSTEventSink<RE::TESActorLocationChangeEvent> {
        static bool DawnguardQuest(RE::FormID id) {
            return id && mentalModel && (id & 0xFF000000) == (mentalModel->GetFormID() & 0xFF000000);
        }

    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStageEvent* event,
                                              RE::BSTEventSource<RE::TESQuestStageEvent>*) override {
            if (event && DawnguardQuest(event->formID))
                NotifyState();
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESQuestStartStopEvent* event,
                                              RE::BSTEventSource<RE::TESQuestStartStopEvent>*) override {
            if (event && DawnguardQuest(event->formID))
                NotifyState();
            return RE::BSEventNotifyControl::kContinue;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event,
                                              RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override {
            if (event && event->actor.get() == serana)
                NotifyState();
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPostLoad) {
            const auto module = GetModuleHandleA(mod::info::DllFile);
            const auto query = module ? reinterpret_cast<QueryAPI>(GetProcAddress(module, QueryExport)) : nullptr;
            host = query ? query(InterfaceVersion) : nullptr;
            if (!host || host->size < BaseAPISize || host->version != InterfaceVersion || !host->registerAdapter ||
                !host->complete) {
                SKSE::log::warn("Serana adapter: compatible YLIWF host API is unavailable");
                host = nullptr;
                return;
            }
            const Adapter adapter{sizeof(Adapter),
                                  InterfaceVersion,
                                  "yliwf.dawnguard.serana",
                                  "Dawnguard / Serana",
                                  nullptr,
                                  Enumerate,
                                  Inspect,
                                  Start,
                                  SetFollowDistance};
            registration = host->registerAdapter(&adapter);
            if (!registration)
                SKSE::log::warn("Serana adapter registration rejected");
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                mentalModel = data->LookupForm<RE::TESQuest>(0x2B6E, "Dawnguard.esm");
                serana = data->LookupForm<RE::Actor>(0x2B74, "Dawnguard.esm");
            }
            if (!mentalModel || !serana)
                SKSE::log::warn("Serana adapter: Dawnguard forms unavailable");
            if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
                static StateEvents stateEvents;
                events->AddEventSink<RE::TESQuestStageEvent>(&stateEvents);
                events->AddEventSink<RE::TESQuestStartStopEvent>(&stateEvents);
                events->AddEventSink<RE::TESActorLocationChangeEvent>(&stateEvents);
            }
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    return messages && messages->RegisterListener(OnMessage);
}
