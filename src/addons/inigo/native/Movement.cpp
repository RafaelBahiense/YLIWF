#include "Movement.h"
#include "RE/Skyrim.h"
#include "ModInfo.h"
#include <algorithm>
#include <array>

namespace mod::inigo_movement {
    namespace {
        std::array<RE::TESPackage*, 3> distancePackages{};
        std::array<RE::TESPackage*, 16> formationPackages{};
        std::array<RE::TESPackage*, 36> standDownPackages{};
        std::array<RE::TESPackage*, 3> ordinaryPassive{};
        RE::TESPackage* ordinaryOnFoot = nullptr;
        RE::TESPackage* selectedDistance = nullptr;
        RE::TESPackage* selectedFormation = nullptr;
        RE::TESPackage* selectedStandDown = nullptr;

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }
    }

    void Configure(RE::TESPackage* onFoot, RE::TESPackage* wait, RE::TESPackage* relax) {
        ordinaryOnFoot = onFoot;
        ordinaryPassive = {onFoot, wait, relax};
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return;
        for (std::size_t i = 0; i < distancePackages.size(); ++i)
            distancePackages[i] =
                data->LookupForm<RE::TESPackage>(static_cast<RE::FormID>(0x990 + i), mod::info::PluginFile);
        for (std::size_t i = 0; i < formationPackages.size(); ++i)
            formationPackages[i] = data->LookupForm<RE::TESPackage>(
                static_cast<RE::FormID>((i < 8 ? 0x9A0 : 0x9C0) + i % 8), mod::info::PluginFile);
        for (std::size_t i = 0; i < standDownPackages.size(); ++i)
            standDownPackages[i] = data->LookupForm<RE::TESPackage>(
                static_cast<RE::FormID>(i < 4 ? 0xA11 + i : 0xA20 + i - 4), mod::info::PluginFile);
    }

    bool Ready() {
        return ordinaryOnFoot &&
               std::ranges::all_of(distancePackages, [](const auto* package) { return package != nullptr; }) &&
               std::ranges::all_of(formationPackages, [](const auto* package) { return package != nullptr; });
    }

    bool Owns(const RE::TESPackage* package) {
        return package && (std::ranges::find(distancePackages, package) != distancePackages.end() ||
                           std::ranges::find(formationPackages, package) != formationPackages.end() ||
                           std::ranges::find(standDownPackages, package) != standDownPackages.end());
    }

    bool Permitted(RE::Actor* actor) {
        const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
        const auto* interrupt = Interrupt(actor);
        return process && process->middleHigh && actor->Is3DLoaded() && ordinaryOnFoot &&
               process->currentPackage.package == ordinaryOnFoot && (!interrupt || Owns(interrupt));
    }

    inigo_movement_rules::Snapshot Read(RE::Actor* actor) {
        inigo_movement_rules::Snapshot result;
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
        if (!actor || !player || !process || !process->middleHigh)
            return result;
        const auto* interrupt = Interrupt(actor);
        result.loaded = actor->Is3DLoaded() && player->Is3DLoaded();
        result.ordinaryOnFoot = ordinaryOnFoot && process->currentPackage.package == ordinaryOnFoot;
        result.ordinaryPassive =
            process->currentPackage.package &&
            std::ranges::find(ordinaryPassive, process->currentPackage.package) != ordinaryPassive.end();
        result.combat = actor->IsInCombat() || player->IsInCombat();
        auto* ui = RE::UI::GetSingleton();
        result.scene = actor->GetCurrentScene() || player->GetCurrentScene() || !ui || ui->IsMenuOpen("Dialogue Menu");
        result.normalMovement = !actor->IsOnMount() && !player->IsOnMount() && !actor->AsActorState()->IsSwimming() &&
                                !player->AsActorState()->IsSwimming() &&
                                actor->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
                                player->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
        result.foreignInterrupt = interrupt && !Owns(interrupt);
        return result;
    }

    bool SetDistance(yliwf::sdk::FollowDistance distance) {
        const auto index = static_cast<std::size_t>(distance);
        if (index >= distancePackages.size() || !distancePackages[index])
            return false;
        selectedDistance = distancePackages[index];
        return true;
    }

    bool SetFormation(RE::Actor* actor, std::uint32_t package) {
        if (!package) {
            selectedFormation = nullptr;
            // Zero releases formation movement, including on dismissal or a quest lock.
            if (std::ranges::find(formationPackages, Interrupt(actor)) != formationPackages.end())
                Release(actor);
            return true;
        }
        const auto found = std::ranges::find_if(
            formationPackages, [package](const auto* value) { return value && value->GetFormID() == package; });
        if (found == formationPackages.end() || !Permitted(actor))
            return false;
        selectedFormation = *found;
        return true;
    }

    bool FormationActive(RE::Actor* actor, std::uint32_t package) {
        const auto* active = Interrupt(actor);
        return active && active == selectedFormation && active->GetFormID() == package;
    }

    bool SetStandDown(RE::Actor* actor, std::uint32_t package) {
        if (!package) {
            selectedStandDown = nullptr;
            const auto* active = Interrupt(actor);
            if (active && std::ranges::find(standDownPackages, active) != standDownPackages.end())
                Release(actor);
            return true;
        }
        const auto found = std::ranges::find_if(
            standDownPackages, [package](const auto* value) { return value && value->GetFormID() == package; });
        if (found == standDownPackages.end())
            return false;
        selectedStandDown = *found;
        return true;
    }

    bool StandDownActive(RE::Actor* actor, std::uint32_t package) {
        const auto* active = Interrupt(actor);
        return active && active == selectedStandDown && active->GetFormID() == package;
    }

    void Release(RE::Actor* actor) {
        if (!Owns(Interrupt(actor)))
            return;
        actor->EndInterruptPackage(true);
        actor->EvaluatePackage(true, false);
    }

    void Refresh(RE::Actor* actor, const inigo_movement_rules::Snapshot& state) {
        const auto selection = inigo_movement_rules::Select(state, selectedDistance != nullptr,
                                                            selectedFormation != nullptr, selectedStandDown != nullptr);
        auto* desired = selection == inigo_movement_rules::Selection::StandDown   ? selectedStandDown
                        : selection == inigo_movement_rules::Selection::Formation ? selectedFormation
                        : selection == inigo_movement_rules::Selection::Distance  ? selectedDistance
                                                                                  : nullptr;
        if (!desired) {
            Release(actor);
            return;
        }
        if (Interrupt(actor) == desired)
            return;
        Release(actor);
        // Reevaluate can select a quest package. Recheck before replacing movement.
        const auto current = Read(actor);
        const bool packageAllowed = selection == inigo_movement_rules::Selection::StandDown
                                        ? current.loaded && current.ordinaryPassive && !current.scene &&
                                              current.normalMovement && !current.foreignInterrupt
                                        : Permitted(actor);
        if (packageAllowed)
            actor->PutCreatedPackage(desired, true, false, false);
    }

    void Reset(RE::Actor* actor) {
        selectedDistance = nullptr;
        selectedFormation = nullptr;
        selectedStandDown = nullptr;
        Release(actor);
    }
}
