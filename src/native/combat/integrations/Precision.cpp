#include "Precision.h"
#include "REX/W32/KERNEL32.h"
#include "PrecisionAPI.h"

#include <array>
#include <chrono>
#include <mutex>

namespace mod::precision {
    namespace {
        using Clock = std::chrono::steady_clock;
        PRECISION_API::IVPrecision1* api = nullptr;
        bool registered = false;

        struct Entry {
            std::uint32_t actor = 0;
            Observation observation;
            Clock::time_point sampled{};
        };

        std::array<Entry, 64> entries{};
        std::mutex mutex;
    }

    void Configure() {
        if (!api)
            api = static_cast<PRECISION_API::IVPrecision1*>(
                PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V1));
        if (!api || registered)
            return;
        const auto result = api->AddPostHitCallback(
            SKSE::GetPluginHandle(), [](const PRECISION_API::PrecisionHitData& hit, const RE::HitData&) {
                const auto source = hit.attacker ? hit.attacker->GetFormID() : 0;
                const auto target = hit.target ? hit.target->GetFormID() : 0;
                std::lock_guard lock(mutex);
                for (auto& entry : entries) {
                    if (!entry.actor)
                        continue;
                    entry.observation.outgoing += entry.actor == source;
                    entry.observation.incoming += entry.actor == target;
                }
            });
        registered = result == PRECISION_API::APIResult::OK;
    }

    Observation Observe(RE::Actor* actor) {
        if (!actor)
            return {};
        Configure();
        const auto now = Clock::now();
        std::unique_lock lock(mutex);
        Entry* selected = nullptr;
        for (auto& entry : entries)
            if (entry.actor == actor->GetFormID()) {
                selected = &entry;
                break;
            }
        if (!selected) {
            selected = &*std::min_element(entries.begin(), entries.end(),
                                          [](const auto& a, const auto& b) { return a.sampled < b.sampled; });
            *selected = {};
            selected->actor = actor->GetFormID();
        }
        // Query outside the mailbox lock: the provider may dispatch callbacks.
        // Cache access itself is main-thread only; callback counters are locked.
        if (!api ||
            (selected->sampled != Clock::time_point{} && now - selected->sampled < std::chrono::milliseconds(500)))
            return selected->observation;
        selected->sampled = now;
        selected->observation.available = true;
        selected->observation.callbacks = registered;
        lock.unlock();
        const auto length = api->GetAttackCollisionCapsuleLength(
            actor->GetHandle(), PRECISION_API::RequestedAttackCollisionType::RightWeapon);
        lock.lock();
        selected->observation.reach = std::isfinite(length) && length > 0 ? length : 0;
        return selected->observation;
    }

    void Clear() {
        std::lock_guard lock(mutex);
        entries = {};
    }
}
