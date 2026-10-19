#include "CombatPrevention.h"
#include "CombatPreventionRules.h"
#include "diagnostics/Debug.h"
#include "ModInfo.h"

namespace mod::combat_prevention {
    namespace {
        RE::TESFaction* ownership = nullptr;

        bool Change(RE::Actor* actor, bool enable) {
            if (!actor || !ownership)
                return false;
            auto* process = actor->GetActorRuntimeData().currentProcess;
            auto* state = process ? process->middleHigh : nullptr;
            if (!state)
                return false;  // Retain ownership until a restored process can be reconciled.
            const bool owned = actor->IsInFaction(ownership);
            const auto change = combat_prevention_rules::Decide(enable, owned, state->preventCombat);
            if (change.claim) {
                actor->AddToFaction(ownership, 0);
                if (!actor->IsInFaction(ownership))
                    return false;
            }
            // IgnoreCombat on a temporary package does not itself prevent
            // a teammate's combat controller from joining the player's fight.
            if (change.prevent.has_value())
                state->preventCombat = *change.prevent;
            if (change.release)
                actor->RemoveFromFaction(ownership);
            if (change.claim || change.release)
                debug::Trace("CombatOrder.Prevention", actor, "claimed={} released={} preventCombat={}", change.claim,
                             change.release, state->preventCombat);
            return !enable || state->preventCombat;
        }
    }

    bool Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        ownership = data ? data->LookupForm<RE::TESFaction>(0xA16, info::PluginFile) : nullptr;
        return ownership != nullptr;
    }

    bool Acquire(RE::Actor* actor) {
        return Change(actor, true);
    }

    void Release(RE::Actor* actor) {
        Change(actor, false);
    }

    void ReconcileLoadedActors() {
        if (const auto* processes = RE::ProcessLists::GetSingleton())
            for (const auto& handle : processes->highActorHandles)
                if (auto actor = handle.get(); actor && ownership && actor->IsInFaction(ownership))
                    Release(actor.get());
    }
}
