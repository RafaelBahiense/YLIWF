#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include "ModInfo.h"
#include "FollowerPolicy.h"
#include "PapyrusController.h"
#include "Rules.h"
#include <array>
#include <cstring>
#include <utility>

namespace {
    constexpr char Plugin[] = "FDE Fura.esp";
    constexpr char Script[] = "aaFuraFollowerScript";
    RE::TESQuest* controller = nullptr;
    RE::Actor* fura = nullptr;
    RE::TESGlobal* cannotFollow = nullptr;
    RE::TESGlobal* atSawmill = nullptr;
    RE::TESGlobal* reveal = nullptr;
    RE::TESGlobal* introduction = nullptr;
    RE::TESPackage* ordinaryPackage = nullptr;
    bool registered = false, packagePatched = false, dialoguePatched = false;

    void ExcludeFura(RE::TESCondition& conditions) {
        auto* condition = new RE::TESConditionItem();
        condition->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetIsReference;
        condition->data.functionData.params[0] = fura;
        condition->data.comparisonValue.f = 0;
        condition->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        condition->data.object = RE::CONDITIONITEMOBJECT::kSelf;
        condition->next = conditions.head;
        conditions.head = condition;
    }

    void PatchDialogue(RE::TESDataHandler* data) {
        if (!registered || !fura || !controller || !cannotFollow || dialoguePatched)
            return;
        // Use the winning vanilla topics, including response records contributed
        // by other plugins. Only the original Fura loses the duplicate choices.
        constexpr std::array<std::pair<RE::FormID, RE::FormID>, 5> topics{{
            {0x75083, 0xA1E},  // Follow / recruitment
            {0x75084, 0xA22},  // Wait
            {0x5C80C, 0xA19},  // Dismiss
            {0x60020, 0xA26},  // Trade
            {0x26E0E, 0xA2A}   // Give an order
        }};
        auto* vanillaQuest = data->LookupForm<RE::TESQuest>(0x750BA, "Skyrim.esm");
        for (const auto& [vanillaID, fdeID] : topics) {
            auto* vanilla = data->LookupForm<RE::TESTopic>(vanillaID, "Skyrim.esm");
            auto* authored = data->LookupForm<RE::TESTopic>(fdeID, Plugin);
            if (!vanillaQuest || !vanilla || vanilla->ownerQuest != vanillaQuest || !authored ||
                authored->ownerQuest != controller) {
                SKSE::log::error("Fura FDE dialogue bindings changed; duplicate filtering was not applied");
                return;
            }
        }
        std::uint32_t filtered = 0, locked = 0;
        for (const auto& [vanillaID, fdeID] : topics) {
            auto* vanilla = data->LookupForm<RE::TESTopic>(vanillaID, "Skyrim.esm");
            for (std::uint32_t index = 0; index < vanilla->numTopicInfos; ++index) {
                if (auto* info = vanilla->topicInfos[index]) {
                    ExcludeFura(info->objConditions);
                    ++filtered;
                }
            }
            // FDE dismissal shares its controller entrypoint with story releases.
            // Gate player choices here so only explicit quest departures can bypass locks.
            if (fdeID != 0xA19 && fdeID != 0xA1E && fdeID != 0xA22)
                continue;
            auto* authored = data->LookupForm<RE::TESTopic>(fdeID, Plugin);
            for (std::uint32_t index = 0; index < authored->numTopicInfos; ++index) {
                if (auto* info = authored->topicInfos[index]) {
                    auto* condition = new RE::TESConditionItem();
                    condition->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
                    condition->data.functionData.params[0] = cannotFollow;
                    condition->data.comparisonValue.f = 0;
                    condition->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
                    condition->data.object = RE::CONDITIONITEMOBJECT::kSelf;
                    condition->next = info->objConditions.head;
                    info->objConditions.head = condition;
                    ++locked;
                }
            }
        }
        dialoguePatched = true;
        SKSE::log::info("Fura FDE dialogue: filtered {} vanilla responses; quest-locked {} FDE responses", filtered,
                        locked);
    }

    bool Compatible() {
        const auto object = yliwf::sdk::papyrus::Bound(controller, Script);
        const auto* binding = yliwf::sdk::papyrus::Field(object, "DLC1FuraRef");
        const auto* faction = yliwf::sdk::papyrus::Field(object, "CurrentFollowerFaction");
        if (!binding || !binding->IsObject() || binding->Unpack<RE::Actor*>() != fura || !faction ||
            !faction->IsObject())
            return false;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (faction->Unpack<RE::TESFaction*>() != data->LookupForm<RE::TESFaction>(0x5C84E, "Skyrim.esm"))
            return false;
        for (const auto* alias : controller->aliases) {
            if (alias && alias->aliasID == 0) {
                const auto* ref = skyrim_cast<const RE::BGSRefAlias*>(alias);
                return ref && ref->GetActorReference() == fura;
            }
        }
        return false;
    }

    std::uint32_t Inspect(void*, std::uint32_t actor, yliwf::policy::State* output) {
        if (!fura || fura->GetFormID() != actor || !output || output->size < sizeof(*output))
            return 0;
        *output = {};
        if (!controller || !controller->IsRunning() || !cannotFollow || !atSawmill || !reveal || !introduction ||
            !packagePatched || !Compatible()) {
            std::strcpy(output->reason, "FDE Fura compatibility bindings are unavailable; control is suspended.");
            return 1;
        }
        const auto* package = fura->GetCurrentPackage();
        const auto* file = package ? package->GetFile(0) : nullptr;
        const bool specialPackage =
            package && package != ordinaryPackage && file && std::strcmp(file->fileName, Plugin) == 0;
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto flags = mod::fura_rules::Restrictions(
            {fura->IsDead() || fura->IsDisabled(), cannotFollow->value != 0, atSawmill->value != 0,
             reveal->value == 3 || (player && fura->IsHostileToActor(player)), fura->GetCurrentScene() != nullptr,
             specialPackage, introduction->value == 1});
        output->ownerQuest = controller->GetFormID();
        output->blocked = flags;
        if (flags)
            std::strcpy(output->reason,
                        "FDE Fura's quest, scene or recruitment conditions currently restrict YLIWF control.");
        return 1;
    }

    void YieldOrdinaryPackage() {
        if (!registered || !ordinaryPackage || !fura || packagePatched)
            return;
        // Keep FDE's quest packages. Its ordinary follower package outranks
        // the core alias and must yield to YLIWF's distance/wait packages.
        // This runtime condition is not written to the ESP or a save.
        ExcludeFura(ordinaryPackage->packConditions);
        packagePatched = true;
    }

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPostLoad) {
            const auto module = GetModuleHandleA(mod::info::DllFile);
            const auto query =
                module ? reinterpret_cast<yliwf::policy::QueryAPI>(GetProcAddress(module, yliwf::policy::QueryExport))
                       : nullptr;
            const auto* api = query ? query(yliwf::policy::Version) : nullptr;
            if (!api || api->size < sizeof(*api) || api->version != yliwf::policy::Version || !api->registerProvider) {
                SKSE::log::error("Fura FDE compatibility requires the matching YLIWF core policy API");
                return;
            }
            const yliwf::policy::Provider provider{sizeof(provider), yliwf::policy::Version, "yliwf.fde.fura", nullptr,
                                                   Inspect};
            registered = api->registerProvider(&provider) != 0;
            SKSE::log::info("Fura FDE quest policy registered: {}", registered);
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                controller = data->LookupForm<RE::TESQuest>(0x810, Plugin);
                if (!controller)
                    return;
                fura = data->LookupForm<RE::Actor>(0x33B2, "Dawnguard.esm");
                cannotFollow = data->LookupForm<RE::TESGlobal>(0xB7D, Plugin);
                atSawmill = data->LookupForm<RE::TESGlobal>(0x9C6, Plugin);
                reveal = data->LookupForm<RE::TESGlobal>(0x991, Plugin);
                introduction = data->LookupForm<RE::TESGlobal>(0xA12, Plugin);
                ordinaryPackage = data->LookupForm<RE::TESPackage>(0x9C4, Plugin);
                YieldOrdinaryPackage();
                PatchDialogue(data);
            }
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    return messages && messages->RegisterListener(OnMessage);
}
