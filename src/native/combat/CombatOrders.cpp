#include "CombatOrders.h"
#include "CombatOrderRules.h"
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
        };

        std::map<RE::FormID, Held> held;

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }

        void Release(RE::Actor* actor, bool adapted) {
            if (!actor)
                return;
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

        bool Apply(RE::Actor* actor, bool adapted) {
            if (!actor || !actor->Is3DLoaded() || actor->IsDead() || actor->GetCurrentScene())
                return false;
            if (!adapted && !combat_order_rules::Eligible(
                                actor->IsPlayerTeammate(), actor->IsDead(), actor->GetCurrentScene() != nullptr,
                                Interrupt(actor) && !stand_down_movement::Owned(Interrupt(actor))))
                return false;
            auto* package = stand_down_movement::Select(actor);
            if (adapted) {
                if (!adapters::SetStandDownPackage(actor->GetFormID(), package->GetFormID()))
                    return false;
            } else {
                if (actor->GetCurrentPackage() != package && Interrupt(actor) != package) {
                    if (stand_down_movement::Owned(Interrupt(actor)))
                        actor->EndInterruptPackage(true);
                    actor->PutCreatedPackage(package, true, false, false);
                }
                if (actor->GetCurrentPackage() != package && Interrupt(actor) != package)
                    return false;
            }
            if (actor->IsInCombat() || !held.contains(actor->GetFormID()))
                for (const auto source :
                     {RE::MagicSystem::CastingSource::kLeftHand, RE::MagicSystem::CastingSource::kRightHand})
                    if (auto* caster = actor->GetMagicCaster(source); caster && caster->currentSpell)
                        caster->InterruptCast(true);
            if (actor->IsInCombat())
                if (auto* processes = RE::ProcessLists::GetSingleton())
                    processes->StopCombatAndAlarmOnActor(actor, false);
            return true;
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        enabledGlobal = data ? data->LookupForm<RE::TESGlobal>(0xA10, mod::info::PluginFile) : nullptr;
        Ready = stand_down_movement::Configure() && enabledGlobal;
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
            if (Apply(actor, adapted)) {
                held[actor->GetFormID()] = {actor->CreateRefHandle(), adapted};
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
        Affected = Unavailable = 0;
        held.clear();
        stand_down_movement::Invalidate();
    }

    void Loaded() {
        Invalidate();
        if (enabledGlobal)
            enabledGlobal->value = 0;
        Tick();
    }
}
