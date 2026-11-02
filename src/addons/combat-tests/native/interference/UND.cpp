#include "UND.h"
#include "UNDProfile.h"
#include "Rules.h"
#include "DodgeBudgetRules.h"
#include "UNDStamina.h"
#include <mutex>
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace {
    namespace control = mod::combat_tests::interference;
    using KeywordCheck = bool (*)(RE::Actor*, std::string_view);
    using Eligibility = bool (*)(void*, RE::Actor*);
    // UND 2.4.3 private ABI. Profile validation precedes every write.
    constexpr std::array<std::uintptr_t, 11> keywordSites{0x2DC14, 0x4E191, 0x4E8AB, 0x4EDE2, 0x4F438, 0x4F84E,
                                                          0x4FC7E, 0x529C7, 0x52BD7, 0x52DE7, 0x52FF7};
    constexpr std::array<std::uintptr_t, 5> eligibilitySites{0x4B2CE, 0x4B865, 0x4BD8A, 0x4C36E, 0x4C94A};
    constexpr std::array<unsigned char, 32> fingerprint{
        0x47, 0xD7, 0xF6, 0xDE, 0xFD, 0x60, 0xE2, 0x94, 0x95, 0x5F, 0xC9, 0x0A, 0x5D, 0xB1, 0x50, 0x00,
        0x59, 0xEF, 0x69, 0x91, 0x2F, 0x1B, 0x8D, 0x95, 0xC2, 0xC7, 0x18, 0x2B, 0x1E, 0x33, 0x81, 0xFE};
    SKSE::Trampoline trampoline{"Combat tests UND"};
    KeywordCheck originalKeyword = nullptr;
    Eligibility originalEligibility = nullptr;
    std::uintptr_t base = 0;
    std::array<std::array<std::uint8_t, 5>, 16> installedBytes{};
    std::atomic<control::Support> support{control::Support::Missing};
    std::atomic_uint32_t selected{0}, checks{0}, exclusions{0}, attempts{0}, denied{0};
    std::atomic_bool bypass{false};
    std::mutex budgetMutex;

    struct Budget {
        std::uint32_t actor = 0;
        std::uint64_t epoch = 0, expires = 0, next = 0;
        bool enabled = false;
        float fraction = .2f, cooldown = 2, cost = 0, stamina = 0, maximum = 0;
        std::uint32_t allowed = 0, reserveDenied = 0, cooldownDenied = 0, snapshotDenied = 0;
    } budget;

    std::uint64_t budgetEpoch = 0;

    bool Matches(RE::Actor* actor) {
        return actor && control::rules::Selected(selected.load(), actor->GetFormID());
    }

    bool HasKeyword(RE::Actor* actor, std::string_view name) {
        if (name == "UND_ExcludeDodge" && Matches(actor)) {
            checks.fetch_add(1);
            if (bypass.load()) {
                exclusions.fetch_add(1);
                return true;
            }
        }
        return originalKeyword(actor, name);
    }

    bool AbleDodge(void* owner, RE::Actor* actor) {
        if (!Matches(actor))
            return originalEligibility(owner, actor);
        attempts.fetch_add(1);
        if (bypass.load()) {
            denied.fetch_add(1);
            return false;
        }
        std::uint64_t epoch = 0;
        {
            std::scoped_lock lock(budgetMutex);
            if (actor && budget.actor == actor->GetFormID() && budget.enabled)
                epoch = budget.epoch;
        }
        const bool allowed = originalEligibility(owner, actor);
        if (!allowed || !epoch)
            return allowed;
        std::scoped_lock lock(budgetMutex);
        if (budget.epoch != epoch || !budget.enabled || budget.actor != actor->GetFormID())
            return allowed;
        const auto now = GetTickCount64();
        const auto reason = control::dodge_budget_rules::Check(now, budget.expires, budget.next, budget.stamina,
                                                               budget.maximum, budget.cost, budget.fraction);
        if (reason == control::dodge_budget_rules::Denial::None) {
            ++budget.allowed;
            budget.next = now + static_cast<std::uint64_t>(budget.cooldown * 1000);
            return true;
        }
        if (reason == control::dodge_budget_rules::Denial::Reserve)
            ++budget.reserveDenied;
        else if (reason == control::dodge_budget_rules::Denial::Cooldown)
            ++budget.cooldownDenied;
        else
            ++budget.snapshotDenied;
        denied.fetch_add(1);
        return false;
    }

    bool Fingerprint(HMODULE module) {
        wchar_t path[32768]{};
        const auto length = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
        if (!length || length >= std::size(path))
            return false;
        std::ifstream file(std::filesystem::path(path), std::ios::binary);
        if (!file)
            return false;
        file.seekg(0, std::ios::end);
        const auto fileSize = file.tellg();
        if (fileSize <= 0 || fileSize > 32 * 1024 * 1024)
            return false;
        file.seekg(0);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
        if (file.bad() || bytes.empty() || bytes.size() > 32 * 1024 * 1024)
            return false;
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            return false;
        std::array<unsigned char, 32> hash{};
        BCRYPT_HASH_HANDLE hashing = nullptr;
        auto result = BCryptCreateHash(algorithm, &hashing, nullptr, 0, nullptr, 0, 0);
        if (result >= 0) {
            result = BCryptHashData(hashing, bytes.data(), static_cast<ULONG>(bytes.size()), 0);
            if (result >= 0)
                result = BCryptFinishHash(hashing, hash.data(), static_cast<ULONG>(hash.size()), 0);
            BCryptDestroyHash(hashing);
        }
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return result >= 0 && hash == fingerprint;
    }

    bool CallMatches(std::uintptr_t site, std::uintptr_t target) {
        std::array<std::uint8_t, 5> bytes{};
        std::memcpy(bytes.data(), reinterpret_cast<void*>(site), bytes.size());
        return control::und::profile::DirectCall(bytes, site, target);
    }

    bool HooksIntact() {
        std::size_t i = 0;
        for (const auto offset : keywordSites)
            if (std::memcmp(reinterpret_cast<void*>(base + offset), installedBytes[i++].data(), 5))
                return false;
        for (const auto offset : eligibilitySites)
            if (std::memcmp(reinterpret_cast<void*>(base + offset), installedBytes[i++].data(), 5))
                return false;
        return true;
    }

    bool AllocateTrampoline() {
        constexpr std::size_t capacity = 512;
        const auto first = base + *std::min_element(keywordSites.begin(), keywordSites.end());
        const auto last = base + *std::max_element(keywordSites.begin(), keywordSites.end());
        // Intersect rel32 reach for every call, including the entire trampoline.
        // Allocating relative to the module base can put later sites out of range.
        const auto lower = last + 5 >= 0x80000000 ? last + 5 - 0x80000000 : 0;
        const auto upper = first + 5 + 0x7FFFFFFF - (capacity - 1);
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const auto granularity = static_cast<std::uintptr_t>(system.dwAllocationGranularity);
        auto cursor = lower;
        while (cursor <= upper) {
            MEMORY_BASIC_INFORMATION region{};
            if (!VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)))
                break;
            const auto regionEnd = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
            if (regionEnd <= cursor)
                break;
            const auto address = (cursor + granularity - 1) & ~(granularity - 1);
            if (region.State == MEM_FREE && address <= upper && address < regionEnd &&
                capacity <= regionEnd - address) {
                auto* memory = VirtualAlloc(reinterpret_cast<void*>(address), capacity, MEM_RESERVE | MEM_COMMIT,
                                            PAGE_EXECUTE_READWRITE);
                if (memory) {
                    const auto allocated = reinterpret_cast<std::uintptr_t>(memory);
                    const auto fits = [allocated](std::uintptr_t offset) {
                        return control::und::profile::Reachable(base + offset, allocated) &&
                               control::und::profile::Reachable(base + offset, allocated + capacity - 1);
                    };
                    if (std::all_of(keywordSites.begin(), keywordSites.end(), fits) &&
                        std::all_of(eligibilitySites.begin(), eligibilitySites.end(), fits)) {
                        trampoline.set_trampoline(memory, capacity,
                                                  [](void* block, std::size_t) { VirtualFree(block, 0, MEM_RELEASE); });
                        return true;
                    }
                    VirtualFree(memory, 0, MEM_RELEASE);
                }
            }
            cursor = regionEnd;
        }
        SKSE::log::warn("Combat tests: no trampoline in range of all UND call sites; comparison disabled.");
        return false;
    }
}

void mod::combat_tests::interference::und::Install() {
    if (originalKeyword)
        return;
    const auto module = GetModuleHandleW(L"UND.dll");
    if (!module)
        return;
    support = Support::Unsupported;
    if (REL::Module::get().version() != REL::Version(1, 6, 1170, 0) || !Fingerprint(module))
        return;
    base = reinterpret_cast<std::uintptr_t>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->OptionalHeader.SizeOfImage <= 0x79050)
        return;
    for (const auto offset : keywordSites)
        if (!CallMatches(base + offset, base + 0x32CF0))
            return;
    for (const auto offset : eligibilitySites)
        if (!CallMatches(base + offset, base + 0x4CC40))
            return;
    constexpr std::array<std::uint8_t, 6> keywordEntry{0x40, 0x53, 0x48, 0x83, 0xEC, 0x30};
    constexpr std::array<std::uint8_t, 7> eligibilityEntry{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x48, 0x08};
    if (std::memcmp(reinterpret_cast<void*>(base + 0x32CF0), keywordEntry.data(), keywordEntry.size()) ||
        std::memcmp(reinterpret_cast<void*>(base + 0x4CC40), eligibilityEntry.data(), eligibilityEntry.size()))
        return;
    if (!AllocateTrampoline())
        return;
    originalKeyword = reinterpret_cast<KeywordCheck>(base + 0x32CF0);
    originalEligibility = reinterpret_cast<Eligibility>(base + 0x4CC40);
    std::size_t i = 0;
    for (const auto offset : keywordSites) {
        trampoline.write_call<5>(base + offset, HasKeyword);
        std::memcpy(installedBytes[i++].data(), reinterpret_cast<void*>(base + offset), 5);
    }
    for (const auto offset : eligibilitySites) {
        trampoline.write_call<5>(base + offset, AbleDodge);
        std::memcpy(installedBytes[i++].data(), reinterpret_cast<void*>(base + offset), 5);
    }
    support = Support::Ready;
    SKSE::log::info("Combat tests: installed verified UND 2.4.3 actor-scoped exclusion and eligibility controls.");
}

bool mod::combat_tests::interference::und::Begin(RE::Actor* actor, bool enabled, bool useBudget, float reserveFraction,
                                                 float cooldown) {
    Reset();
    if (originalKeyword)
        support = HooksIntact() ? Support::Ready : Support::Shadowed;
    if (!actor || support != Support::Ready)
        return !enabled && !useBudget;
    if (!dodge_budget_rules::Valid(reserveFraction, cooldown))
        return false;
    bypass = enabled;
    selected = actor->GetFormID();
    {
        std::scoped_lock lock(budgetMutex);
        budget.actor = actor->GetFormID();
        budget.enabled = useBudget;
        budget.fraction = reserveFraction;
        budget.cooldown = cooldown;
    }
    Sample(actor);
    return true;
}

void mod::combat_tests::interference::und::Sample(RE::Actor* actor) {
    if (!Matches(actor))
        return;
    std::uint64_t epoch;
    {
        std::scoped_lock lock(budgetMutex);
        if (!budget.enabled)
            return;
        epoch = budget.epoch;
    }
    float cost = 0;
    const bool valid = StaminaCost(actor, base, cost);
    const float current = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
    const float maximum = actor->AsActorValueOwner()->GetPermanentActorValue(RE::ActorValue::kStamina) +
                          actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
    std::scoped_lock lock(budgetMutex);
    if (budget.epoch != epoch || !budget.enabled)
        return;
    budget.cost = cost;
    budget.stamina = current;
    budget.maximum = maximum;
    budget.expires = valid ? GetTickCount64() + 250 : 0;
}

void mod::combat_tests::interference::und::End() {
    selected = 0;
    bypass = false;
    std::scoped_lock lock(budgetMutex);
    budget.enabled = false;
    budget.actor = 0;
    budget.epoch = ++budgetEpoch;
}

void mod::combat_tests::interference::und::Reset() {
    End();
    checks = exclusions = attempts = denied = 0;
    std::scoped_lock lock(budgetMutex);
    budget = {};
    budget.epoch = budgetEpoch;
}

mod::combat_tests::interference::und::Snapshot mod::combat_tests::interference::und::Read() {
    std::scoped_lock lock(budgetMutex);
    return {support.load(),
            selected.load() ? 1u : 0u,
            checks.load(),
            exclusions.load(),
            attempts.load(),
            denied.load(),
            budget.enabled ? 1u : 0u,
            budget.allowed,
            budget.reserveDenied,
            budget.cooldownDenied,
            budget.snapshotDenied,
            budget.cost,
            budget.maximum * budget.fraction,
            budget.stamina};
}
