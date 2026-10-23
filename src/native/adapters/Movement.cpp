#include "adapters/Movement.h"
#include "RE/Skyrim.h"
#include "ModInfo.h"
#include "common/MovementRecords.h"
#include <array>
#include <unordered_map>

namespace mod::adapter_movement {
    namespace {
        using namespace yliwf::sdk;
        std::array<RE::TESPackage*, movement_records::Distances> distances{};
        std::array<RE::TESPackage*, movement_records::Formations> formations{};
        std::array<RE::TESPackage*, movement_records::StandDown> standDown{};

        struct Selection {
            RE::TESPackage* distance = nullptr;
            RE::TESPackage* formation = nullptr;
            RE::TESPackage* passive = nullptr;
            RE::TESPackage* applied = nullptr;

            bool Empty() const {
                return !distance && !formation && !passive && !applied;
            }
        };

        std::unordered_map<ActorID, Selection> selections;

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }

        adapter_movement_rules::Snapshot Read(RE::Actor* actor, const Inspect& inspect) {
            adapter_movement_rules::Snapshot result;
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!process || !process->middleHigh || !player)
                return result;
            const auto* owner = process->currentPackage.package;
            result = inspect(actor->GetFormID(), owner ? owner->GetFormID() : 0);
            result.available = !actor->IsDead() && !actor->IsDisabled() && actor->Is3DLoaded() &&
                               player->Is3DLoaded() && !actor->AsActorState()->IsBleedingOut() &&
                               !actor->AsActorState()->IsUnconscious();
            auto* ui = RE::UI::GetSingleton();
            result.scene =
                actor->GetCurrentScene() || player->GetCurrentScene() || !ui || ui->IsMenuOpen("Dialogue Menu");
            result.normalMovement = !actor->IsOnMount() && !player->IsOnMount() &&
                                    !actor->AsActorState()->IsSwimming() && !player->AsActorState()->IsSwimming() &&
                                    actor->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
                                    player->AsActorState()->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal;
            const auto* interrupt = Interrupt(actor);
            const auto found = selections.find(actor->GetFormID());
            result.foreignInterrupt = interrupt && (found == selections.end() || interrupt != found->second.applied);
            result.combat = actor->IsInCombat() || player->IsInCombat();
            return result;
        }

        void Release(RE::Actor* actor, Selection& selection) {
            if (actor && selection.applied && Interrupt(actor) == selection.applied) {
                actor->EndInterruptPackage(true);
                actor->EvaluatePackage(true, false);
            }
            selection.applied = nullptr;
        }

        RE::TESPackage* Resolve(Movement feature, std::uint32_t id) {
            const auto find = [id](const auto& packages) -> RE::TESPackage* {
                for (auto* package : packages)
                    if (package && package->GetFormID() == id)
                        return package;
                return nullptr;
            };
            return feature == Movement::Formation   ? find(formations)
                   : feature == Movement::StandDown ? find(standDown)
                                                    : nullptr;
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return;
        for (std::size_t i = 0; i < distances.size(); ++i)
            distances[i] = data->LookupForm<RE::TESPackage>(movement_records::Distance(i), mod::info::PluginFile);
        for (std::size_t i = 0; i < formations.size(); ++i)
            formations[i] = data->LookupForm<RE::TESPackage>(movement_records::Formation(i), mod::info::PluginFile);
        for (std::size_t i = 0; i < standDown.size(); ++i)
            standDown[i] =
                data->LookupForm<RE::TESPackage>(movement_records::PassiveOrAvoidance(i), mod::info::PluginFile);
    }

    bool KnownPackage(std::uint32_t package) {
        return package && (Resolve(Movement::Formation, package) || Resolve(Movement::StandDown, package) ||
                           std::ranges::any_of(
                               distances, [package](auto* value) { return value && value->GetFormID() == package; }));
    }

    std::uint32_t IsPackage(std::uint32_t package) {
        return KnownPackage(package) ? 1u : 0u;
    }

    bool Permitted(std::uint32_t actor, Movement feature, const Inspect& inspect) {
        return adapter_movement_rules::Permitted(Read(RE::TESForm::LookupByID<RE::Actor>(actor), inspect), feature);
    }

    void Refresh(std::uint32_t id, const Inspect& inspect) {
        const auto found = selections.find(id);
        if (found == selections.end())
            return;
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
        auto& selection = found->second;
        const auto feature =
            adapter_movement_rules::Select(Read(actor, inspect), selection.distance != nullptr,
                                           selection.formation != nullptr, selection.passive != nullptr);
        auto* desired = feature == MovementBit(Movement::StandDown)   ? selection.passive
                        : feature == MovementBit(Movement::Formation) ? selection.formation
                        : feature == MovementBit(Movement::Distance)  ? selection.distance
                                                                      : nullptr;
        if (!desired) {
            Release(actor, selection);
            return;
        }
        if (Interrupt(actor) == desired && selection.applied == desired)
            return;
        Release(actor, selection);
        // Package reevaluation can select quest behavior. Re-read both the
        // owner policy and engine guards before installing an interrupt.
        if (adapter_movement_rules::Permitted(Read(actor, inspect), static_cast<Movement>(feature))) {
            actor->PutCreatedPackage(desired, true, false, false);
            if (Interrupt(actor) == desired)
                selection.applied = desired;
        }
    }

    bool SetDistance(std::uint32_t actor, int preset, const Inspect& inspect) {
        if (preset < 0 || preset >= static_cast<int>(distances.size()) || !distances[preset])
            return false;
        selections[actor].distance = distances[preset];
        Refresh(actor, inspect);
        return true;
    }

    bool SetPackage(std::uint32_t actor, Movement feature, std::uint32_t package, const Inspect& inspect) {
        if (feature != Movement::Formation && feature != Movement::StandDown)
            return false;
        if (!package && !selections.contains(actor))
            return true;
        auto* resolved = package ? Resolve(feature, package) : nullptr;
        if (package && (!resolved || !Permitted(actor, feature, inspect)))
            return false;
        auto& selection = selections[actor];
        auto*& destination = feature == Movement::Formation ? selection.formation : selection.passive;
        auto* previous = destination;
        destination = resolved;
        if (!package && selection.applied == previous)
            Release(RE::TESForm::LookupByID<RE::Actor>(actor), selection);
        Refresh(actor, inspect);
        if (package && selection.applied != resolved) {
            destination = nullptr;
            if (selection.Empty())
                selections.erase(actor);
            return false;
        }
        if (selection.Empty())
            selections.erase(actor);
        return true;
    }

    void Suspend(std::uint32_t actor) {
        const auto found = selections.find(actor);
        if (found == selections.end())
            return;
        found->second.formation = found->second.passive = nullptr;
        Release(RE::TESForm::LookupByID<RE::Actor>(actor), found->second);
        if (!found->second.distance)
            selections.erase(found);
    }

    void Forget(std::uint32_t actor) {
        Suspend(actor);
        selections.erase(actor);
    }

    bool ClearSaved(std::uint32_t id) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
        if (!actor)
            return true;
        const auto* process = actor->GetActorRuntimeData().currentProcess;
        if (!actor->Is3DLoaded() || !process || !process->middleHigh)
            return false;
        const auto* interrupt = Interrupt(actor);
        if (interrupt && KnownPackage(interrupt->GetFormID())) {
            actor->EndInterruptPackage(true);
            actor->EvaluatePackage(true, false);
        }
        return true;
    }

    std::vector<std::uint32_t> Actors() {
        std::vector<std::uint32_t> result;
        for (const auto& [actor, selection] : selections)
            result.push_back(actor);
        return result;
    }

    bool HasWork() {
        return !selections.empty();
    }

    bool Tracks(std::uint32_t actor) {
        return selections.contains(actor);
    }

    void Invalidate() {
        selections.clear();
    }
}
