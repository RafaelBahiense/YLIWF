#include "ObserverControl.h"
#include "ObserverRules.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <atomic>
#include <memory>
#include <vector>

namespace mod::combat_tests::observer_control {
    namespace {
        struct Scope {
            std::uint32_t observer = 0;
            std::vector<std::uint32_t> opponents;
            std::vector<std::uint32_t> allies;
        };

        std::atomic<std::shared_ptr<const Scope>> published;
        std::atomic_uint32_t rejections{0};
        std::atomic_uint32_t friendlyRejections{0};
        std::vector<std::uint32_t> alliedActors, opponentActors;
        std::uint32_t generation = 0;
        REL::Relocation<bool (*)(RE::Actor*, RE::TESObjectREFR&)> original;

        bool ValidTarget(RE::Actor* actor, RE::TESObjectREFR& target) {
            const auto scope = published.load();
            if (scope && actor &&
                observer_rules::Reject(actor->GetFormID(), target.GetFormID(), scope->observer, scope->opponents,
                                       scope->allies)) {
                if (target.GetFormID() == scope->observer)
                    ++rejections;
                else
                    ++friendlyRejections;
                return false;
            }
            return original(actor, target);
        }
    }

    void Install() {
        if (original.address())
            return;
        // CommonLib's Actor::CheckValidTarget virtual: SE/AE D6, VR D8.
        // Worker callbacks read only the published IDs; no VM/quest access.
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_Character[0]};
        original = table.write_vfunc(REL::Module::IsVR() ? 0xD8 : 0xD6, ValidTarget);
    }

    bool Begin(std::uint32_t participant, std::uint32_t opponent, std::uint32_t extra, bool ally, std::uint32_t token) {
        std::vector allies{participant};
        std::vector opponents{opponent};
        if (extra)
            (ally ? allies : opponents).push_back(extra);
        return Begin(allies, opponents, token);
    }

    bool Begin(std::span<const std::uint32_t> allies, std::span<const std::uint32_t> opponents, std::uint32_t token) {
        if (!original.address() || allies.empty() || opponents.empty() || !token || generation ||
            std::ranges::find(allies, 0u) != allies.end() || std::ranges::find(opponents, 0u) != opponents.end())
            return false;
        alliedActors.assign(allies.begin(), allies.end());
        opponentActors.assign(opponents.begin(), opponents.end());
        generation = token;
        rejections = 0;
        friendlyRejections = 0;
        Refresh();
        return true;
    }

    void Refresh() {
        if (!generation)
            return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return;
        auto scope = std::make_shared<Scope>();
        scope->observer = player->GetFormID();
        scope->opponents = opponentActors;
        scope->allies = alliedActors;
        if (auto* processes = RE::ProcessLists::GetSingleton())
            for (const auto& handle : processes->highActorHandles) {
                const auto actor = handle.get();
                if (!actor || !actor->IsSummoned())
                    continue;
                auto owner = actor->GetCommandingActor();
                for (unsigned depth = 0; owner && depth < 4; ++depth) {
                    if (std::ranges::find(opponentActors, owner->GetFormID()) != opponentActors.end()) {
                        scope->opponents.push_back(actor->GetFormID());
                        break;
                    }
                    if (std::ranges::find(alliedActors, owner->GetFormID()) != alliedActors.end()) {
                        scope->allies.push_back(actor->GetFormID());
                        break;
                    }
                    owner = owner->GetCommandingActor();
                }
            }
        const auto previous = published.load();
        if (!previous || previous->observer != scope->observer || previous->opponents != scope->opponents ||
            previous->allies != scope->allies)
            published = std::move(scope);
    }

    void End() {
        published = nullptr;
        alliedActors.clear();
        opponentActors.clear();
        generation = 0;
    }

    std::uint32_t Rejections() {
        return rejections.load();
    }

    std::uint32_t FriendlyRejections() {
        return friendlyRejections.load();
    }
}
