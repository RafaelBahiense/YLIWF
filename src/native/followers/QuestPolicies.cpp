#include "followers/QuestPolicies.h"
#include "settings/Settings.h"
#include <vector>
#include <string>
#include <cstring>
#include <mutex>

namespace mod::quest_policies {
    namespace {
        struct Entry {
            std::string id;
            yliwf::policy::Provider provider;
        };

        std::vector<Entry> providers;

        std::uint32_t Register(const yliwf::policy::Provider* provider) {
            std::scoped_lock lock(settings::Mutex);
            if (!provider || provider->size < sizeof(*provider) || provider->version != yliwf::policy::Version ||
                !provider->id || !*provider->id || std::strlen(provider->id) > 128 || !provider->inspect ||
                providers.size() >= 16)
                return 0;
            for (const auto& entry : providers)
                if (entry.id == provider->id)
                    return 0;
            providers.push_back({provider->id, *provider});
            return static_cast<std::uint32_t>(providers.size());
        }
    }

    yliwf::policy::State Inspect(RE::Actor* actor) {
        yliwf::policy::State result;
        result.blocked = 0;
        if (!actor)
            return result;
        bool found = false;
        for (const auto& entry : providers) {
            yliwf::policy::State value;
            if (!entry.provider.inspect(entry.provider.context, actor->GetFormID(), &value))
                continue;
            value.reason[sizeof(value.reason) - 1] = 0;
            if (found || !yliwf::policy::Valid(value)) {
                result = {};
                std::strcpy(result.reason, "Quest restriction providers conflict or returned invalid data.");
                return result;
            }
            found = true;
            result = value;
        }
        return result;
    }

    bool Allows(RE::Actor* actor, Action action) {
        return !(Inspect(actor).blocked & yliwf::policy::Bit(action));
    }

    bool CanRelease(RE::Actor* actor, RE::TESQuest* owner) {
        const auto state = Inspect(actor);
        return yliwf::policy::CanRelease(state, owner ? owner->GetFormID() : 0);
    }

    bool AllowsOperation(RE::Actor* actor, controller_rules::Operation operation) {
        using controller_rules::Operation;
        switch (operation) {
            case Operation::Recruit:
            case Operation::Adopt:
                return Allows(actor, Action::Recruit);
            case Operation::Follow:
            case Operation::DialogueFollow:
            case Operation::PromotePrimary:
                return Allows(actor, Action::Follow);
            case Operation::Wait:
            case Operation::DialogueWait:
                return Allows(actor, Action::Wait);
            case Operation::Dismiss:
            case Operation::DialogueDismiss:
            case Operation::Timeout:
                return Allows(actor, Action::Dismiss);
            case Operation::HomeAssign:
            case Operation::HomeRemove:
                return Allows(actor, Action::Home);
            case Operation::QuestRelease:
                return CanRelease(actor);
            default:
                return true;
        }
    }
}

extern "C" __declspec(dllexport) const yliwf::policy::API* YLIWF_GetFollowerPolicyAPI(std::uint32_t version) {
    static const yliwf::policy::API api{sizeof(yliwf::policy::API), yliwf::policy::Version,
                                        mod::quest_policies::Register};
    return version == yliwf::policy::Version ? &api : nullptr;
}
