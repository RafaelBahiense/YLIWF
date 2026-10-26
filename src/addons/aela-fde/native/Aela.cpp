#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include "ModInfo.h"
#include "AddonInfo.h"
#include "FollowerPolicy.h"
#include "PapyrusController.h"
#include "Rules.h"
#include <cstring>

namespace {
    static_assert(mod::addon::RequiredAdapterAPIVersion == yliwf::policy::Version);
    constexpr char BasePlugin[] = "FDE Aela.esp";
    constexpr char PartTwoPlugin[] = "FDE Aela Part 2.esp";
    RE::Actor* aela = nullptr;
    RE::TESQuest* owner = nullptr;
    RE::TESQuest* transformation = nullptr;
    RE::TESFaction* potentialFollowers = nullptr;
    RE::TESRace* werewolfRace = nullptr;

    bool ActorBinding(const yliwf::sdk::papyrus::Object& object, const char* name) {
        const auto* field = yliwf::sdk::papyrus::Field(object, name);
        return field && field->IsObject() && field->Unpack<RE::Actor*>() == aela;
    }

    bool AuthoredPackage(RE::TESPackage* package) {
        const auto* file = package ? package->GetFile(0) : nullptr;
        if (!file)
            return false;
        // Include FDE banter modules, whose packages also retain scene ownership.
        return std::strncmp(file->fileName, "FDE Aela", 8) == 0;
    }

    std::uint32_t Inspect(void*, std::uint32_t actor, yliwf::policy::State* output) {
        if (!aela || actor != aela->GetFormID() || !output || output->size < sizeof(*output))
            return 0;

        const auto approval = yliwf::sdk::papyrus::Bound(owner, "aaAelaApprove");
        const auto transform = yliwf::sdk::papyrus::Bound(transformation, "aaAelaTransformScript");
        const auto transforming = yliwf::sdk::papyrus::Boolean(transform, "YLIWFTransforming");
        const bool compatible = owner && transformation && potentialFollowers && werewolfRace &&
                                ActorBinding(approval, "AelaTheHuntressRef") && ActorBinding(transform, "Aela") &&
                                transforming.has_value();
        auto* player = RE::PlayerCharacter::GetSingleton();
        *output = {};
        output->blocked =
            mod::aela_rules::Restrictions({.compatible = compatible,
                                           .unavailable = aela->IsDead() || aela->IsDisabled(),
                                           .hostile = player && aela->IsHostileToActor(player),
                                           .eligible = potentialFollowers && aela->IsInFaction(potentialFollowers),
                                           .transforming = transforming.value_or(false),
                                           .beastForm = werewolfRace && aela->GetRace() == werewolfRace,
                                           .inScene = aela->GetCurrentScene() != nullptr,
                                           .questPackage = AuthoredPackage(aela->GetCurrentPackage())});
        // Only the fixed base quest authorizes explicit authored departures.
        output->ownerQuest = compatible ? owner->GetFormID() : 0;
        if (!compatible)
            std::strcpy(output->reason, "FDE Aela requires the matching replacement scripts and both FDE plugins.");
        else if (output->blocked)
            std::strcpy(output->reason, "FDE Aela's transformation, scene or story conditions restrict YLIWF control.");
        return 1;
    }

    void OnMessage(SKSE::MessagingInterface::Message* message) {
        if (message->type == SKSE::MessagingInterface::kPostLoad) {
            const auto module = GetModuleHandleA(mod::info::DllFile);
            const auto query =
                module ? reinterpret_cast<yliwf::policy::QueryAPI>(GetProcAddress(module, yliwf::policy::QueryExport))
                       : nullptr;
            const auto* api = query ? query(yliwf::policy::Version) : nullptr;
            if (!api || api->size < sizeof(*api) || api->version != yliwf::policy::Version || !api->registerProvider) {
                SKSE::log::error("Aela FDE requires the matching YLIWF follower policy API");
                return;
            }
            const yliwf::policy::Provider provider{sizeof(provider), yliwf::policy::Version, "yliwf.fde.aela", nullptr,
                                                   Inspect};
            SKSE::log::info("Aela FDE quest policy registered: {}", api->registerProvider(&provider));
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* data = RE::TESDataHandler::GetSingleton()) {
                aela = data->LookupForm<RE::Actor>(0x1A697, "Skyrim.esm");
                owner = data->LookupForm<RE::TESQuest>(0x8D4, BasePlugin);
                transformation = data->LookupForm<RE::TESQuest>(0x800, PartTwoPlugin);
                potentialFollowers = data->LookupForm<RE::TESFaction>(0x5C84D, "Skyrim.esm");
                werewolfRace = data->LookupForm<RE::TESRace>(0xCDD84, "Skyrim.esm");
            }
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    auto* messages = SKSE::GetMessagingInterface();
    return messages && messages->RegisterListener(OnMessage);
}
