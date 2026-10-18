#include "TravelFormation.h"
#include "RE/Skyrim.h"
#include "ModInfo.h"
#include <algorithm>
#include <array>

namespace mod::serana_formation {
    namespace {
        std::array<RE::TESPackage*, 16> formationPackages{};
        std::array<RE::TESPackage*, 4> ordinaryPackages{};
        std::array<RE::TESPackage*, 36> standDownPackages{};

        bool StandDownOwned(const RE::TESPackage* package) {
            return package && std::ranges::find(standDownPackages, package) != standDownPackages.end();
        }

        bool Owned(const RE::TESPackage* package) {
            return package && std::ranges::find(formationPackages, package) != formationPackages.end();
        }

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return;
        for (std::size_t i = 0; i < standDownPackages.size(); ++i)
            standDownPackages[i] = data->LookupForm<RE::TESPackage>(
                static_cast<RE::FormID>(i < 4 ? 0xA11 + i : 0xA20 + i - 4), mod::info::PluginFile);
        for (std::size_t i = 0; i < formationPackages.size(); ++i)
            formationPackages[i] = data->LookupForm<RE::TESPackage>(
                static_cast<RE::FormID>((i < 8 ? 0x9A0 : 0x9C0) + i % 8), mod::info::PluginFile);
        // Only ordinary following/sandboxing from the mental-model alias.
        // Quest-specific escort, stay and force-greet packages are excluded.
        constexpr std::array<RE::FormID, 4> forms{0x2BB9, 0x2BBA, 0x2BBB, 0x3BC1};
        for (std::size_t i = 0; i < forms.size(); ++i)
            ordinaryPackages[i] = data->LookupForm<RE::TESPackage>(forms[i], "Dawnguard.esm");
    }

    bool PermittedPackage(RE::Actor* actor, bool standDown) {
        if (!actor || !actor->GetActorRuntimeData().currentProcess || !actor->Is3DLoaded() ||
            (!standDown && actor->IsInCombat()) || actor->GetCurrentScene() || actor->IsOnMount() ||
            actor->AsActorState()->IsSwimming() ||
            actor->AsActorState()->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal)
            return false;
        const auto* interrupt = Interrupt(actor);
        if (interrupt && !Owned(interrupt) && !(standDown && StandDownOwned(interrupt)))
            return false;
        // Inspect the underlying package even while our temporary package runs.
        // A newly selected quest package revokes movement immediately.
        const auto* package = actor->GetActorRuntimeData().currentProcess->currentPackage.package;
        return package && std::ranges::find(ordinaryPackages, package) != ordinaryPackages.end();
    }

    void Release(RE::Actor* actor) {
        if (!Owned(Interrupt(actor)))
            return;
        // Never end a foreign interrupt or close the player's dialogue.
        actor->EndInterruptPackage(true);
        actor->EvaluatePackage(true, false);
    }

    bool ApplyStandDown(RE::Actor* actor, std::uint32_t package) {
        if (!actor)
            return false;
        if (!package) {
            if (StandDownOwned(Interrupt(actor))) {
                actor->EndInterruptPackage(true);
                actor->EvaluatePackage(true, false);
            }
            return true;
        }
        const auto found = std::ranges::find_if(
            standDownPackages, [package](const auto* value) { return value && value->GetFormID() == package; });
        if (found == standDownPackages.end() || !PermittedPackage(actor, true))
            return false;
        Release(actor);
        if (Interrupt(actor) != *found) {
            if (StandDownOwned(Interrupt(actor)))
                actor->EndInterruptPackage(true);
            actor->PutCreatedPackage(*found, true, false, false);
        }
        return Interrupt(actor) == *found;
    }

    bool Apply(RE::Actor* actor, std::uint32_t package) {
        if (!package) {
            Release(actor);
            return true;
        }
        const auto found = std::ranges::find_if(
            formationPackages, [package](const auto* value) { return value && value->GetFormID() == package; });
        if (found == formationPackages.end() || !PermittedPackage(actor))
            return false;
        if (Interrupt(actor) != *found) {
            // The plugin owns this record: it is temporary movement, not a
            // dynamically allocated package that the engine may destroy.
            actor->PutCreatedPackage(*found, true, false, false);
        }
        return Interrupt(actor) == *found;
    }
}
