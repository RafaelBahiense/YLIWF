#include "Actors.h"
#include "Resources.h"
#include "RE/A/Actor.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/P/ProcessLists.h"
#include "RE/T/TESForm.h"
#include <cstdio>
#include <cstdint>
#include <string>

namespace mod::combat_tests::testing {
    Result AdoptActor(Resources& resources, Scope scope, const std::string& key, RE::Actor* actor,
                      std::uint32_t expectedBase) {
        if (!actor || !expectedBase || (actor->GetFormID() & 0xFF000000) != 0xFF000000 || !actor->GetActorBase() ||
            actor->GetActorBase()->GetFormID() != expectedBase)
            return Result::Failed("Spawned actor identity is unverified; cleanup was not registered.");
        const auto handle = actor->GetHandle();
        const auto id = actor->GetFormID();
        const bool registered = resources.Own(scope, key, id, [handle, id, expectedBase, issued = false]() mutable {
            const auto live = handle.get();
            if (!live)
                return Result{};
            if (live->GetFormID() != id || !live->GetActorBase() || live->GetActorBase()->GetFormID() != expectedBase)
                return Result::Failed("Spawned actor identity changed; cleanup requires review.");
            if (!issued) {
                live->InterruptCast(true);
                live->StopCombat();
                live->Disable();
                live->SetDelete(true);
                issued = true;
            }
            return live->IsDisabled() && live->IsMarkedForDeletion()
                       ? Result{}
                       : Result::Pending("Waiting for spawned actor cleanup; no command replayed.");
        });
        return registered ? Result{} : Result::Failed("Spawned actor cleanup could not be registered.");
    }

    RE::Actor* SpawnActor(Resources& resources, Scope scope, const std::string& key, RE::TESNPC* base,
                          RE::TESObjectREFR* anchor) {
        if (const auto existing = resources.Find(scope, key))
            return RE::TESForm::LookupByID<RE::Actor>(existing);
        if (!base || !anchor)
            return nullptr;
        const auto created = anchor->PlaceObjectAtMe(base, false);
        auto* actor = created ? created->As<RE::Actor>() : nullptr;
        if (!actor)
            return nullptr;
        if (AdoptActor(resources, scope, key, actor, base->GetFormID()).state != State::Ready) {
            // This exact reference was created by this call and never published.
            actor->Disable();
            actor->SetDelete(true);
            return nullptr;
        }
        return actor;
    }

    Result CheckArena(RE::Actor* subject, RE::TESObjectREFR* anchor) {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !anchor || player->GetParentCell() != anchor->GetParentCell())
            return Result::Failed("Enter the test arena before starting.");
        const auto* processes = RE::ProcessLists::GetSingleton();
        if (!processes)
            return Result::Failed("Actor process lists are unavailable.");
        for (const auto& handle : processes->highActorHandles)
            if (const auto other = handle.get(); other && other.get() != player && other.get() != subject &&
                                                 !other->IsDead() && !other->IsDisabled() &&
                                                 other->GetParentCell() == anchor->GetParentCell()) {
                char reason[256];
                std::snprintf(reason, sizeof(reason),
                              "Another living actor is in the arena: %08X %.96s. "
                              "Move other actors outside before starting the test.",
                              other->GetFormID(), other->GetName() ? other->GetName() : "Unnamed actor");
                return Result::Failed(reason);
            }
        return {};
    }
}
