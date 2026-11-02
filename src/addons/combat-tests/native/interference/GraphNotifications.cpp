#include "GraphNotifications.h"
#include "../telemetry/Dodge.h"
#include "ExternalAttacks.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <intrin.h>
#include <vector>
#include <string>

namespace {
    namespace telemetry = mod::combat_tests::telemetry;
    using Notify = bool (*)(RE::IAnimationGraphManagerHolder*, const RE::BSFixedString&);
    Notify previous = nullptr;
    mod::combat_tests::interference::graph_notifications::Filter filter = nullptr;
    std::uintptr_t table = 0;

    struct Module {
        std::uintptr_t begin, end;
        std::string name;
    };

    std::vector<Module> modules;

    void CaptureModules() {
        const auto handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (handle == INVALID_HANDLE_VALUE)
            return;
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(handle, &entry)) {
            do {
                char name[256]{};
                WideCharToMultiByte(CP_UTF8, 0, entry.szModule, -1, name, sizeof(name), nullptr, nullptr);
                const auto base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                modules.push_back({base, base + entry.modBaseSize, name});
            } while (Module32NextW(handle, &entry));
        }
        CloseHandle(handle);
    }

    __declspec(noinline) bool NotifyGraph(RE::IAnimationGraphManagerHolder* holder, const RE::BSFixedString& name) {
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        const auto actor = holder ? static_cast<RE::TESObjectREFR*>(holder)->GetFormID() : 0;
        const std::string_view event = name.c_str() ? name.c_str() : "";
        std::uint64_t ticket = 0;
        std::uint32_t attack = 0;
        if (telemetry::Watching(actor) || mod::combat_tests::interference::external_attacks::Watching(actor)) {
            const Module* source = nullptr;
            for (const auto& module : modules)
                if (caller >= module.begin && caller < module.end) {
                    source = &module;
                    break;
                }
            attack = mod::combat_tests::interference::external_attacks::Before(
                actor, source ? source->name : "<unknown>", event);
            ticket = telemetry::BeginNotification(actor, event, source ? source->name : "<unknown>",
                                                  source ? caller - source->begin : caller);
        }
        const bool suppressed = filter && filter(actor, event, caller);
        const bool result = suppressed ? false : previous(holder, name);
        if (attack)
            mod::combat_tests::interference::external_attacks::After(attack, result);
        if (ticket)
            telemetry::EndNotification(ticket, result, suppressed);
        return result;
    }
}

void mod::combat_tests::interference::graph_notifications::Install(Filter callback) {
    if (previous || REL::Module::get().version() != REL::Version(1, 6, 1170, 0))
        return;
    CaptureModules();
    filter = callback;
    REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_Character[3]};
    table = vtable.address();
    previous = reinterpret_cast<Notify>(vtable.write_vfunc(1, NotifyGraph));
    SKSE::log::info("Combat tests: installed NPC graph request provenance observer.");
}

bool mod::combat_tests::interference::graph_notifications::Verified(RE::Actor* actor) {
    if (!previous || !actor)
        return false;
    auto* holder = static_cast<RE::IAnimationGraphManagerHolder*>(actor);
    return *reinterpret_cast<const std::uintptr_t*>(holder) == table &&
           *reinterpret_cast<const std::uintptr_t*>(table + sizeof(void*)) ==
               reinterpret_cast<std::uintptr_t>(NotifyGraph);
}
