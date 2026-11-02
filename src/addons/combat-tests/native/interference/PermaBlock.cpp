#include "Control.h"
#include "Rules.h"
#include "GraphNotifications.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <Windows.h>
#include <atomic>
#include <string_view>

namespace {
    namespace control = mod::combat_tests::interference;
    namespace rules = control::rules;
    bool installed = false;
    std::uintptr_t sourceBegin = 0, sourceEnd = 0;
    std::atomic<control::Support> support{control::Support::Missing};
    std::atomic_uint32_t selected{0}, blockStops{0}, bashStops{0}, suppressed{0};
    std::atomic_bool bypass{false};

    bool Filter(std::uint32_t actor, std::string_view name, std::uintptr_t caller) {
        const auto target = selected.load();
        if (target && caller >= sourceBegin && caller < sourceEnd) {
            const auto event = rules::Classify(name);
            if (rules::Observe(target, actor, true, event)) {
                (event == rules::Event::BlockStop ? blockStops : bashStops).fetch_add(1);
                if (rules::Suppress(bypass.load(), true)) {
                    suppressed.fetch_add(1);
                    return true;
                }
            }
        }
        return false;
    }

    bool FindCode(HMODULE module) {
        const auto base = reinterpret_cast<std::uintptr_t>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000)
            return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC || nt->FileHeader.NumberOfSections > 32)
            return false;
        const auto* sections = IMAGE_FIRST_SECTION(nt);
        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            const auto& section = sections[i];
            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;
            if (sourceBegin || !section.Misc.VirtualSize || section.VirtualAddress >= nt->OptionalHeader.SizeOfImage ||
                section.Misc.VirtualSize > nt->OptionalHeader.SizeOfImage - section.VirtualAddress)
                return false;
            sourceBegin = base + section.VirtualAddress;
            sourceEnd = sourceBegin + section.Misc.VirtualSize;
        }
        return sourceBegin && sourceEnd > sourceBegin;
    }
}

void mod::combat_tests::interference::Install() {
    if (installed)
        return;
    graph_notifications::Install(Filter);
    const auto module = GetModuleHandleW(L"FixNPCPermaBlock.dll");
    if (!module)
        return;
    if (REL::Module::get().version() != REL::Version(1, 6, 1170, 0) || !FindCode(module)) {
        support = Support::Unsupported;
        return;
    }
    installed = true;
    support = Support::Ready;
    SKSE::log::info("Combat tests: installed caller-scoped PermaBlock interruption observer.");
}

bool mod::combat_tests::interference::Begin(RE::Actor* actor, bool requested) {
    selected = 0;
    bypass = false;
    blockStops = bashStops = suppressed = 0;
    if (!actor)
        return !requested;
    if (installed)
        support = graph_notifications::Verified(actor) ? Support::Ready : Support::Shadowed;
    if (support != Support::Ready)
        return !requested;
    bypass = requested;
    selected = actor->GetFormID();
    return true;
}

void mod::combat_tests::interference::End() {
    selected = 0;
    bypass = false;
}

void mod::combat_tests::interference::Reset() {
    End();
    blockStops = bashStops = suppressed = 0;
}

mod::combat_tests::interference::Snapshot mod::combat_tests::interference::Read() {
    return {support.load(),    bypass.load() ? 1u : 0u, selected.load() ? 1u : 0u,
            blockStops.load(), bashStops.load(),        suppressed.load()};
}
