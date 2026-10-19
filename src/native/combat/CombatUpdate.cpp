#include "CombatUpdate.h"
#include "CombatUpdateGate.h"
#include "CombatOrders.h"
#include "StandDownMovement.h"
#include "HealerPackages.h"

namespace mod::combat_update {
    namespace {
        Gate gate;
        REL::Relocation<void (*)(RE::Actor*)> original;

        void Update(RE::Actor* actor) {
            if (actor && combat_orders::Enabled.load()) {
                const auto* process = actor->GetActorRuntimeData().currentProcess;
                const auto* interrupt =
                    process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
                if (gate.Suppress(actor->GetFormID(), actor->GetCurrentScene() != nullptr,
                                  interrupt && !stand_down_movement::Owned(interrupt) &&
                                      !healer_packages::OwnsPackage(interrupt)))
                    return;
            }
            // Preserve the previous hook/engine implementation for every other actor.
            // No StopCombat, package writes or callbacks run on the combat job thread.
            original(actor);
        }
    }

    void Install() {
        if (original.address())
            return;
        // Actor::UpdateCombat in the pinned CommonLib maps SE/AE E4 and VR E6.
        // Hooking the vtable avoids copying/versioning executable instructions.
        // May God forgive me for this hack.
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_Character[0]};
        original = table.write_vfunc(REL::Module::IsVR() ? 0xE6 : 0xE4, Update);
        logger::info("Stand-down combat-update hook installed");
    }

    void Acquire(RE::Actor* actor) {
        gate.Acquire(actor->GetFormID());
    }

    void Release(RE::Actor* actor) {
        if (actor)
            gate.Release(actor->GetFormID());
    }

    void Clear() {
        gate.Clear();
    }

    std::uint64_t TakeSuppressed(RE::Actor* actor) {
        return gate.TakeSuppressed(actor->GetFormID());
    }
}
