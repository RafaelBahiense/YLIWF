#include "CombatOrders.h"
#include "CombatOrderRules.h"
#include "CombatPrevention.h"
#include "CombatUpdate.h"
#include "StandDownMovement.h"
#include "ModInfo.h"
#include "adapters/Adapters.h"
#include "followers/ControllerRuntime.h"
#include "followers/ControllerExecutor.h"
#include "formations/Formation.h"
#include "combat/Healer.h"
#include "diagnostics/Debug.h"
#include <map>
#include <set>

namespace mod::combat_orders {
    namespace {
        RE::TESGlobal* enabledGlobal = nullptr;

        std::atomic_uint64_t generation{0};

        struct Held {
            RE::ActorHandle actor;
            bool adapted = false;
            bool reentered = false;
        };

        std::map<RE::FormID, Held> held;

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }

        void Release(RE::Actor* actor, bool adapted) {
            if (!actor)
                return;
            combat_update::Release(actor);
            combat_prevention::Release(actor);
            stand_down_movement::Release(actor);
            if (adapted)
                adapters::SetStandDownPackage(actor->GetFormID(), 0);
            else {
                const bool owned = stand_down_movement::Owned(Interrupt(actor));
                if (owned)
                    actor->EndInterruptPackage(true);
                if (owned || stand_down_movement::Owned(actor->GetCurrentPackage()))
                    actor->EvaluatePackage(true, false);
            }
        }

        bool ApplyPackage(RE::Actor* actor, RE::TESPackage* package, bool adapted, bool restart = false) {
            if (adapted) {
                if (restart && !adapters::SetStandDownPackage(actor->GetFormID(), 0))
                    return false;
                if (!adapters::SetStandDownPackage(actor->GetFormID(), package->GetFormID()))
                    return false;
            } else if (actor->GetCurrentPackage() != package) {
                if (stand_down_movement::Owned(Interrupt(actor)))
                    actor->EndInterruptPackage(true);
                actor->PutCreatedPackage(package, true, false, false);
            }
            return actor->GetCurrentPackage() == package;
        }

        bool StopCombat(RE::Actor* actor) {
            if (!actor->IsInCombat())
                return false;
            if (auto* processes = RE::ProcessLists::GetSingleton())
                processes->StopCombatAndAlarmOnActor(actor, false);
            if (actor->IsInCombat())
                actor->StopCombat();
            return true;
        }

        bool Apply(RE::Actor* actor, bool adapted) {
            if (!actor || !actor->Is3DLoaded() || actor->IsDead() || actor->GetCurrentScene())
                return false;
            if (actor->IsInCombat() && held.contains(actor->GetFormID())) {
                auto& state = held.at(actor->GetFormID());
                if (!state.reentered) {
                    state.reentered = true;
                    ++Reentries;
                }
                const auto* running = actor->GetCurrentPackage();
                const auto* interrupt = Interrupt(actor);
                const auto* process = actor->GetActorRuntimeData().currentProcess;
                const bool prevented = process && process->middleHigh && process->middleHigh->preventCombat;
                debug::Trace("CombatOrder.ReenteredCombat", actor,
                             "adapted={} running={:08X} interrupt={:08X} preventCombat={}", adapted,
                             running ? running->GetFormID() : 0, interrupt ? interrupt->GetFormID() : 0, prevented);
            }
            if (!adapted && !combat_order_rules::Eligible(
                                actor->IsPlayerTeammate(), actor->IsDead(), actor->GetCurrentScene() != nullptr,
                                Interrupt(actor) && !stand_down_movement::Owned(Interrupt(actor))))
                return false;
            auto* package = stand_down_movement::Select(actor);
            if (!ApplyPackage(actor, package, adapted))
                return false;
            if (!combat_prevention::Acquire(actor))
                return false;
            combat_update::Acquire(actor);
            if (actor->IsInCombat() || !held.contains(actor->GetFormID()))
                for (const auto source :
                     {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                    if (auto* caster = actor->GetMagicCaster(source); caster && caster->currentSpell)
                        caster->InterruptCast(true);
            const bool stopped = StopCombat(actor);

            // Ending combat can reevaluate AI. Quest-managed followers have no
            // host alias package list to restore our temporary override. A
            // matching package pointer also does not prove its procedure is
            // executing after combat: restart the adapter-owned interrupt.
            if (actor->GetCurrentPackage() != package || (adapted && stopped)) {
                const auto* replaced = actor->GetCurrentPackage();
                const bool restored = ApplyPackage(actor, package, adapted, adapted && stopped);
                debug::Trace("CombatOrder.PackageRecheck", actor,
                             "adapted={} requested={:08X} replaced={:08X} restored={} combat={}", adapted,
                             package->GetFormID(), replaced ? replaced->GetFormID() : 0, restored, actor->IsInCombat());
                if (stopped)
                    debug::Trace("CombatOrder.CombatCleanup", actor,
                                 "adapted={} combatAfter={} restarted={} restored={}", adapted, actor->IsInCombat(),
                                 adapted, restored);
                return restored;
            }
            if (stopped)
                debug::Trace("CombatOrder.CombatCleanup", actor, "adapted={} combatAfter={} restarted=false", adapted,
                             actor->IsInCombat());
            return true;
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        enabledGlobal = data ? data->LookupForm<RE::TESGlobal>(0xA10, mod::info::PluginFile) : nullptr;
        Ready = stand_down_movement::Configure() && combat_prevention::Configure() && enabledGlobal;
    }

    void Tick() {
        if (!Ready)
            return;
        std::set<RE::FormID> roster;
        std::size_t affected = 0, unavailable = 0;
        const auto visit = [&](RE::Actor* actor, bool adapted) {
            if (!actor || !roster.insert(actor->GetFormID()).second)
                return;
            if (!Enabled) {
                // Reconcile saved temporary packages, including after a load.
                Release(actor, adapted);
                return;
            }
            const auto suppressed = combat_update::TakeSuppressed(actor);
            SuppressedUpdates += suppressed;
            if (suppressed)
                debug::Trace("CombatOrder.SuppressedUpdates", actor, "count={}", suppressed);
            if (Apply(actor, adapted)) {
                auto& state = held[actor->GetFormID()];
                state.actor = actor->CreateRefHandle();
                state.adapted = adapted;
                // A combat re-entry remains a failure for this order, even if
                // StopCombat cleared it before this snapshot was published.
                if (state.reentered || actor->IsInCombat())
                    ++unavailable;
                else
                    ++affected;
            } else {
                ++unavailable;
                Release(actor, adapted);
            }
        };
        const auto& context = controller::detail::GetContext();
        if (context.quest)
            for (auto* alias : context.quest->aliases)
                // Include the separate vanilla animal companion as well as humans.
                if (alias && alias->aliasID <= 8)
                    if (auto* reference = skyrim_cast<RE::BGSRefAlias*>(alias))
                        if (auto* actor = reference->GetActorReference(); actor && !adapters::Owns(actor->GetFormID()))
                            visit(actor, false);
        for (const auto& follower : adapters::Followers(false))
            visit(RE::TESForm::LookupByID<RE::Actor>(follower.actor), true);
        std::erase_if(held, [&](const auto& item) {
            if (Enabled && roster.contains(item.first))
                return false;
            if (auto actor = item.second.actor.get())
                Release(actor.get(), item.second.adapted);
            return true;
        });
        Affected = affected;
        Unavailable = unavailable;
    }

    void Set(bool enabled) {
        const auto expected = generation.load();
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([enabled, expected] {
                if (expected != generation || !Ready)
                    return;
                if (enabled && !controller::storage::Available())
                    return;
                Enabled = enabled;
                if (enabled) {
                    SuppressedUpdates = Reentries = 0;
                    for (auto& [id, state] : held)
                        state.reentered = false;
                }
                enabledGlobal->value = enabled ? 1.0f : 0.0f;
                formation::Changed();
                if (enabled)
                    healer::Suspend();
                Tick();
                controller::executor::Wake();
                debug::NotifyStateChanged();
                logger::info("Party combat order: {}", enabled ? "Stand down" : "Normal behavior");
            });
    }

    void Invalidate() {
        ++generation;
        Enabled = false;
        combat_update::Clear();
        SuppressedUpdates = Reentries = 0;
        Affected = Unavailable = 0;
        held.clear();
        stand_down_movement::Invalidate();
    }

    void Loaded() {
        Invalidate();
        if (enabledGlobal)
            enabledGlobal->value = 0;
        combat_prevention::ReconcileLoadedActors();
        Tick();
    }

    void ActorLoaded(RE::Actor* actor) {
        combat_update::Release(actor);
        combat_prevention::Release(actor);
        if (Enabled)
            controller::executor::Wake();
    }
}
