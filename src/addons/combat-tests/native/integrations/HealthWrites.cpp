#include "HealthWrites.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include <algorithm>

namespace mod::combat_tests::health_writes {
    namespace {
        using Write = void (*)(RE::Actor*, RE::ACTOR_VALUE_MODIFIER, RE::ActorValue, float, RE::Actor*);
        SKSE::Trampoline trampoline{"Combat fixture health writes"};
        Write previous = nullptr;
        Filter filter = nullptr;
        std::array<std::uintptr_t, health_write_profile::Sites.size()> sites{};
        std::array<std::array<std::uint8_t, 5>, health_write_profile::Sites.size()> installedBytes{};
        bool installed = false;
        const char* status = "Health-write integration is not installed.";

        template <Path path>
        void Change(RE::Actor* actor, RE::ACTOR_VALUE_MODIFIER modifier, RE::ActorValue value, float delta,
                    RE::Actor* source) {
            if (filter && actor)
                delta = filter(actor->GetFormID(), modifier == RE::ACTOR_VALUE_MODIFIER::kDamage,
                               value == RE::ActorValue::kHealth, delta, path);
            previous(actor, modifier, value, delta, source);
        }

        bool Allocate() {
            constexpr std::size_t capacity = 256;
            const auto first = *std::min_element(sites.begin(), sites.end());
            const auto last = *std::max_element(sites.begin(), sites.end());
            const auto lower = last + 5 >= 0x80000000 ? last + 5 - 0x80000000 : 0;
            const auto upper = first + 5 + 0x7FFFFFFF - (capacity - 1);
            SYSTEM_INFO system{};
            GetSystemInfo(&system);
            const auto granularity = static_cast<std::uintptr_t>(system.dwAllocationGranularity);
            auto cursor = lower;
            while (cursor <= upper) {
                MEMORY_BASIC_INFORMATION region{};
                if (!VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)))
                    return false;
                const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
                if (end <= cursor)
                    return false;
                const auto address = (cursor + granularity - 1) & ~(granularity - 1);
                if (region.State == MEM_FREE && address <= upper && address < end && capacity <= end - address) {
                    auto* memory = VirtualAlloc(reinterpret_cast<void*>(address), capacity, MEM_RESERVE | MEM_COMMIT,
                                                PAGE_EXECUTE_READWRITE);
                    if (memory) {
                        const auto allocated = reinterpret_cast<std::uintptr_t>(memory);
                        const bool fits = std::ranges::all_of(sites, [allocated](auto site) {
                            return health_write_profile::Reachable(site, allocated) &&
                                   health_write_profile::Reachable(site, allocated + capacity - 1);
                        });
                        if (fits) {
                            trampoline.set_trampoline(
                                memory, capacity, [](void* block, std::size_t) { VirtualFree(block, 0, MEM_RELEASE); });
                            return true;
                        }
                        VirtualFree(memory, 0, MEM_RELEASE);
                    }
                }
                cursor = end;
            }
            return false;
        }
    }

    void Install(Filter callback) {
        if (installed)
            return;
        constexpr REL::Version supportedRuntime{1, 6, 1170, 0};
        if (REL::Module::get().version() != supportedRuntime) {
            status = "Health-write profile supports Skyrim 1.6.1170 only.";
            return;
        }
        const auto backend = REL::ID(health_write_profile::Backend).address();
        for (std::size_t index = 0; index < sites.size(); ++index) {
            const auto profile = health_write_profile::Sites[index];
            sites[index] = REL::ID(profile.id).address() + profile.offset;
            const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(sites[index]), 5);
            if (health_write_profile::CallTarget(bytes, sites[index]) != backend) {
                status = "Health-write callsite differs from the verified profile; no calls patched.";
                return;
            }
        }
        if (!callback || !Allocate()) {
            status = "Could not allocate a reachable health-write bridge.";
            return;
        }
        previous = reinterpret_cast<Write>(backend);
        filter = callback;
        constexpr std::array<Write, 5> callbacks{Change<Path::QueuedFirst>, Change<Path::QueuedSecond>,
                                                 Change<Path::Transfer>, Change<Path::Public>, Change<Path::Combat>};
        for (std::size_t index = 0; index < sites.size(); ++index) {
            trampoline.write_call<5>(sites[index], callbacks[index]);
            std::memcpy(installedBytes[index].data(), reinterpret_cast<void*>(sites[index]), 5);
        }
        installed = true;
        status = "Verified direct health-write calls installed.";
    }

    bool Ready() {
        if (!installed)
            return false;
        for (std::size_t index = 0; index < sites.size(); ++index)
            if (std::memcmp(installedBytes[index].data(), reinterpret_cast<void*>(sites[index]), 5))
                return false;
        return true;
    }

    const char* Status() {
        return installed && !Ready() ? "A later hook replaced a health-write call; protection is unavailable." : status;
    }

    bool ProbeCombat(RE::Actor* actor, float delta) {
        if (!actor || !Ready())
            return false;
        const auto index = static_cast<std::size_t>(Path::Combat);
        const auto target = health_write_profile::CallTarget(installedBytes[index], sites[index]);
        if (!target)
            return false;
        reinterpret_cast<Write>(target)(actor, RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kHealth, delta,
                                        nullptr);
        return true;
    }
}
