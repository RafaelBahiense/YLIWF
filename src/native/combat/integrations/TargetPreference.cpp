#include "TargetPreference.h"
#include "combat/actions/TauntRules.h"
#include <atomic>
#include <chrono>
#include <memory>

namespace mod::target_preference {
    namespace {
        struct Preference {
            std::uint32_t enemy = 0, defender = 0;
            std::chrono::steady_clock::time_point expires;
        };

        std::atomic<std::shared_ptr<const Preference>> published;
        std::atomic_uint32_t rejections{0};
        REL::Relocation<bool (*)(RE::Actor*, RE::TESObjectREFR&)> previous;

        bool Check(RE::Actor* actor, RE::TESObjectREFR& candidate) {
            if (!previous(actor, candidate))
                return false;
            const auto value = published.load();
            if (value && actor &&
                combat_actions::taunt_rules::RejectTarget(actor->GetFormID(), candidate.GetFormID(), value->enemy,
                                                          value->defender,
                                                          std::chrono::steady_clock::now() <= value->expires)) {
                ++rejections;
                return false;
            }
            return true;
        }
    }

    void Install() {
        if (!previous.address()) {
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_Character[0]};
            previous = table.write_vfunc(REL::Module::IsVR() ? 0xD8 : 0xD6, Check);
        }
    }

    bool Available() {
        return previous.address() != 0;
    }

    void Publish(std::uint32_t enemy, std::uint32_t defender) {
        if (!enemy || !defender) {
            published = nullptr;
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto prior = published.load();
        if (prior && prior->enemy == enemy && prior->defender == defender &&
            prior->expires - now > std::chrono::milliseconds(150))
            return;
        published = std::make_shared<Preference>(Preference{enemy, defender, now + std::chrono::milliseconds(250)});
    }

    void Clear() {
        published = nullptr;
        rejections = 0;
    }

    std::uint32_t Rejections() {
        return rejections.load();
    }
}
