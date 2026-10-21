#include "PackageControl.h"
#include "PackageControlRules.h"
#include "ControllerRuntime.h"
#include "ControllerExecutor.h"
#include "QuestPolicies.h"
#include "adapters/Adapters.h"
#include "diagnostics/Debug.h"
#include "settings/Settings.h"
#include "RE/E/ExtraAliasInstanceArray.h"

#include <map>
#include <shared_mutex>

namespace mod::package_control {
    namespace {
        struct Held {
            RE::ActorHandle actor;
            Snapshot snapshot;
        };

        // State and package conditions are accessed only on the game thread.
        std::map<RE::FormID, Held> held;
        // Package selection may read this table from the engine's AI jobs.
        std::shared_mutex selectionMutex;
        std::map<RE::FormID, RE::TESPackage*> selections;
        std::atomic_bool selecting{false};
        REL::Relocation<RE::TESPackage* (*)(RE::Actor*)> original;

        bool HumanSlot(std::uint32_t id) {
            return id == 0 || (id >= 2 && id <= 8);
        }

        RE::BGSRefAlias* Registration(RE::Actor* actor) {
            auto* quest = controller::detail::GetContext().quest;
            if (!actor || !quest || adapters::Owns(actor->GetFormID()))
                return nullptr;
            for (auto* alias : quest->aliases) {
                auto* reference = alias && HumanSlot(alias->aliasID) ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
                if (reference && reference->GetActorReference() == actor)
                    return reference;
            }
            return nullptr;
        }

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }

        std::vector<RE::TESPackage*> Packages(RE::Actor* actor, const RE::BGSRefAlias* alias) {
            std::vector<RE::TESPackage*> result;
            auto* instances = actor->extraList.GetByType<RE::ExtraAliasInstanceArray>();
            if (!instances)
                return result;
            RE::BSReadLockGuard guard(instances->lock);
            for (const auto* instance : instances->aliases) {
                if (!instance || instance->quest != controller::detail::GetContext().quest ||
                    instance->alias != alias || !instance->instancedPackages)
                    continue;
                for (auto* package : *instance->instancedPackages)
                    if (package)
                        result.push_back(package);
            }
            return result;
        }

        bool Publish(RE::FormID actor, RE::TESPackage* package) {
            std::unique_lock lock(selectionMutex);
            const auto found = selections.find(actor);
            const bool changed = found == selections.end() ? package != nullptr : found->second != package;
            if (package)
                selections[actor] = package;
            else
                selections.erase(actor);
            selecting = !selections.empty();
            return changed;
        }

        RE::TESPackage* Select(RE::Actor* actor) {
            if (!selecting.load())
                return original(actor);
            // Keep engine calls outside the table lock and retain scene priority.
            if (actor && !actor->GetCurrentScene()) {
                std::shared_lock lock(selectionMutex);
                const auto found = selections.find(actor->GetFormID());
                if (found != selections.end())
                    return found->second;
            }
            return original(actor);
        }

        void Restore(RE::Actor* actor, Snapshot& state) {
            Publish(actor->GetFormID(), nullptr);
            state.active = false;
            state.requestedPackage = 0;
            if (!actor->GetActorRuntimeData().currentProcess || !actor->Is3DLoaded() || actor->IsDisabled() ||
                actor->IsDead())
                return;
            auto* saved = RE::TESForm::LookupByID<RE::TESPackage>(state.savedInterrupt);
            if (package_control_rules::RestoreInterrupt(
                    saved != nullptr, Interrupt(actor) != nullptr, actor->GetCurrentScene() != nullptr,
                    saved && saved->packConditions.IsTrue(actor, RE::PlayerCharacter::GetSingleton()))) {
                actor->PutCreatedPackage(saved, true, false, false);
                debug::Trace("PackageControl.Restore", actor, "interrupt={:08X}", state.savedInterrupt);
            } else {
                // Reevaluate the untouched alias/base lists rather than replaying
                // a package whose conditions or controller have changed.
                actor->EvaluatePackage(true, false);
            }
            state.savedInterrupt = 0;
        }
    }

    void Install() {
        if (original.address())
            return;
        // This inherited TESObjectREFR virtual has the same index in SE/AE/VR.
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_Character[0]};
        original = table.write_vfunc(0x49, Select);
        logger::info("Debug per-actor alias-package selection hook installed");
    }

    void Tick() {
        for (auto it = held.begin(); it != held.end();) {
            auto actor = it->second.actor.get();
            if (!actor) {
                Publish(it->first, nullptr);
                it = held.erase(it);
                continue;
            }
            auto& state = it->second.snapshot;
            auto* alias = Registration(actor.get());
            const auto decision = package_control_rules::Decide(
                {debug::OptionsEnabled.load(), alias != nullptr, !actor->IsDead(),
                 actor->Is3DLoaded() && !actor->IsDisabled(), actor->GetCurrentScene() != nullptr,
                 quest_policies::Allows(actor.get(), quest_policies::Action::Movement),
                 controller::storage::Available() && controller::storage::Get().active.has_value()});
            if (decision == package_control_rules::Decision::Release) {
                Restore(actor.get(), state);
                it = held.erase(it);
                continue;
            }
            ++it;
            if (decision == package_control_rules::Decision::Suspend) {
                if (state.active)
                    Suspend(actor.get());
                state.status = "Suspended: actor unavailable, scene, quest restriction or pending command.";
                continue;
            }
            const auto packages = Packages(actor.get(), alias);
            auto* selected = package_control_rules::Select(packages, [&actor](auto* package) {
                return package->packConditions.IsTrue(actor.get(), RE::PlayerCharacter::GetSingleton());
            });
            if (!selected) {
                if (state.active)
                    Suspend(actor.get());
                state.status = "No eligible YLIWF alias package.";
                continue;
            }
            bool changed = !state.active || state.requestedPackage != selected->GetFormID();
            state.active = true;
            state.requestedPackage = selected->GetFormID();
            changed = Publish(actor->GetFormID(), selected) || changed;
            if (auto* interrupt = Interrupt(actor.get());
                interrupt && std::ranges::find(packages, interrupt) == packages.end()) {
                // Remember FormIDs, never copy ActorPackage's owned procedure data.
                state.savedInterrupt = interrupt->GetFormID();
                actor->EndInterruptPackage(true);
                changed = true;
            }
            if (changed) {
                actor->EvaluatePackage(true, false);
                debug::Trace("PackageControl.Select", actor.get(), "requested={:08X} previous={:08X} saved={:08X}",
                             state.requestedPackage, state.previousPackage, state.savedInterrupt);
            }
            state.status = actor->GetCurrentPackage() == selected ? "YLIWF package running."
                                                                  : "Requested YLIWF package is not running.";
        }
    }

    bool HasWork() {
        return !held.empty();
    }

    void Withdraw(std::uint32_t actorID) {
        Publish(actorID, nullptr);
    }

    void Suspend(RE::Actor* actor) {
        if (!actor)
            return;
        const auto found = held.find(actor->GetFormID());
        if (found == held.end())
            return;
        Publish(actor->GetFormID(), nullptr);
        found->second.snapshot.active = false;
        found->second.snapshot.requestedPackage = 0;
        if (actor->Is3DLoaded() && !actor->IsDisabled() && !actor->IsDead())
            actor->EvaluatePackage(true, false);
    }

    void Release(RE::Actor* actor) {
        if (!actor)
            return;
        const auto found = held.find(actor->GetFormID());
        if (found == held.end())
            return;
        Restore(actor, found->second.snapshot);
        held.erase(found);
    }

    void Invalidate() {
        {
            std::unique_lock lock(selectionMutex);
            selections.clear();
            selecting = false;
        }
        for (auto& [id, entry] : held)
            if (auto actor = entry.actor.get())
                Restore(actor.get(), entry.snapshot);
        held.clear();
    }

    Snapshot Inspect(RE::Actor* actor) {
        const auto found = actor ? held.find(actor->GetFormID()) : held.end();
        return found == held.end() ? Snapshot{} : found->second.snapshot;
    }

    void Request(std::uint32_t actorID, std::uint32_t aliasID, bool enabled, std::uint64_t generation) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !debug::IsCurrentGame(generation))
            return;
        tasks->AddTask([actorID, aliasID, enabled, generation] {
            std::scoped_lock lock(settings::Mutex);
            if (!debug::IsCurrentGame(generation) || !debug::OptionsEnabled.load())
                return;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
            if (!enabled) {
                Release(actor);
            } else {
                const auto* alias = Registration(actor);
                if (!actor || !alias || alias->aliasID != aliasID || actor->IsDead() || held.contains(actorID))
                    return;
                Snapshot state;
                state.enabled = true;
                if (auto* package = actor->GetCurrentPackage())
                    state.previousPackage = package->GetFormID();
                held.emplace(actorID, Held{actor->GetHandle(), std::move(state)});
                Tick();
            }
            controller::executor::Kick();
            debug::NotifyStateChanged();
        });
    }
}
