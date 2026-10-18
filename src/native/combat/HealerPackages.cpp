#include "combat/HealerPackages.h"
#include "ModInfo.h"
#include "diagnostics/Debug.h"
#include "RE/B/BGSPackageDataPointerTemplate.h"
#include "RE/T/TESCustomPackageData.h"

#include <array>

namespace mod::healer_packages {
    namespace {
        // The pinned CommonLib exposes the pointer-template layout and RTTI,
        // but not the two concrete target-input classes. These abstract views
        // are only used after an engine RTTI check; no objects are constructed.
        struct SpellInput : RE::BGSPackageDataPointerTemplate<RE::IPackageData, RE::PackageTarget> {
            inline static constexpr auto RTTI = RE::RTTI_BGSPackageDataTargetSelector;
        };

        // IAITarget derives from IPackageDataAIWorldLocationHandle: its second
        // interface places IPackageData at +8, and the target pointer at +24.
        // Verified against Skyrim's RTTI hierarchy, not the selector's layout.
        struct ReferenceInput
            : RE::BGSPackageDataPointerTemplate<RE::IPackageDataAIWorldLocationHandle, RE::PackageTarget> {
            inline static constexpr auto RTTI = RE::RTTI_BGSPackageDataRef;
        };

        static_assert(sizeof(SpellInput) == 0x18);
        static_assert(sizeof(ReferenceInput) == 0x20);
        static_assert(offsetof(SpellInput, pointer) == 0x10);
        static_assert(offsetof(ReferenceInput, pointer) == 0x18);

        std::array<RE::TESPackage*, Slots> travel{}, alternate{}, casts{};
        std::array<RE::TESObjectREFR*, Slots> markers{};
        RE::TESFaction* activity = nullptr;

        RE::TESPackage* Interrupt(RE::Actor* actor) {
            const auto* process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
            return process && process->middleHigh ? process->middleHigh->runOncePackage.package : nullptr;
        }

        bool Owned(const RE::TESPackage* package) {
            return package && (std::ranges::find(travel, package) != travel.end() ||
                               std::ranges::find(alternate, package) != alternate.end() ||
                               std::ranges::find(casts, package) != casts.end());
        }

        RE::IPackageData* Input(RE::TESPackage* package, int uid) {
            auto* data = package ? skyrim_cast<RE::TESCustomPackageData*>(package->data) : nullptr;
            if (!data || !data->data.data || !data->data.uids)
                return nullptr;
            for (std::size_t i = 0; i < data->data.dataSize; ++i)
                if (data->data.uids[i] == uid)
                    return data->data.data[i];
            return nullptr;
        }

        bool Apply(RE::Actor* actor, RE::TESPackage* package) {
            if (!actor || !package || !activity || ForeignInterrupt(actor))
                return false;
            int rank = -1;
            for (std::size_t slot = 0; slot < Slots; ++slot) {
                if (package == travel[slot])
                    rank = static_cast<int>(slot);
                else if (package == alternate[slot])
                    rank = static_cast<int>(Slots + slot);
                else if (package == casts[slot])
                    rank = static_cast<int>(Slots * 2 + slot);
            }
            if (rank < 0)
                return false;
            // Reconcile temporary packages left by earlier previews. New actions
            // are selected through the alias's normal/combat-override lists.
            if (Owned(Interrupt(actor)))
                actor->EndInterruptPackage(true);
            if (actor->GetFactionRank(activity, false) != rank) {
                actor->AddToFaction(activity, static_cast<std::int8_t>(rank));
                actor->EvaluatePackage(true, false);
            }
            return actor->GetFactionRank(activity, false) == rank;
        }
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return;
        activity = data->LookupForm<RE::TESFaction>(0xA08, mod::info::PluginFile);
        for (std::size_t slot = 0; slot < Slots; ++slot) {
            const auto index = static_cast<RE::FormID>(slot);
            travel[slot] = data->LookupForm<RE::TESPackage>(0x9D0 + index, mod::info::PluginFile);
            alternate[slot] = data->LookupForm<RE::TESPackage>(0x9E0 + index, mod::info::PluginFile);
            casts[slot] = data->LookupForm<RE::TESPackage>(0x9F0 + index, mod::info::PluginFile);
            markers[slot] = data->LookupForm<RE::TESObjectREFR>(0xA00 + index, mod::info::PluginFile);
        }
    }

    bool Ready(std::size_t slot) {
        return activity && slot < Slots && travel[slot] && alternate[slot] && casts[slot] && markers[slot];
    }

    bool Owned(RE::Actor* actor) {
        return actor && (Owned(actor->GetCurrentPackage()) || Owned(Interrupt(actor)) ||
                         (activity && actor->GetFactionRank(activity, false) >= 0));
    }

    bool ForeignInterrupt(RE::Actor* actor) {
        auto* package = Interrupt(actor);
        return package && !Owned(package);
    }

    bool Move(RE::Actor* actor, std::size_t slot, const RE::NiPoint3& destination) {
        if (!Ready(slot) || !actor || ForeignInterrupt(actor))
            return false;
        auto* marker = markers[slot];
        const bool areaChanged = marker->GetWorldspace() != actor->GetWorldspace() ||
                                 (!actor->GetWorldspace() && marker->GetParentCell() != actor->GetParentCell());
        const bool changed = areaChanged || marker->GetPosition().GetDistance(destination) >= 48;
        if (areaChanged)
            marker->MoveTo(actor);
        if (changed)
            marker->SetPosition(destination);
        auto* package = actor->GetCurrentPackage();
        if (!changed && (package == travel[slot] || package == alternate[slot]))
            return true;
        return Apply(actor, package == travel[slot] ? alternate[slot] : travel[slot]);
    }

    bool Cast(RE::Actor* actor, std::size_t slot, RE::Actor* target, RE::SpellItem* spell) {
        if (!Ready(slot) || !actor || !target || !spell || Casting(actor, slot) || ForeignInterrupt(actor))
            return false;
        auto* spellInput = skyrim_cast<SpellInput*>(Input(casts[slot], 3));
        auto* targetInput = skyrim_cast<ReferenceInput*>(Input(casts[slot], 4));
        if (!spellInput || !targetInput || !spellInput->pointer || !targetInput->pointer) {
            debug::Trace("Healer.PackageData", actor, "unsupported spell/target input layout slot={}", slot);
            return false;
        }
        // These records belong exclusively to one live healer slot. Never
        // rewrite a running procedure or a shared vanilla/third-party record.
        spellInput->pointer->targType = 1;  // ObjectID, as authored in the ESP.
        spellInput->pointer->target.object = spell;
        spellInput->pointer->value = 1;
        targetInput->pointer->target.object = nullptr;
        // UseMagic needs the Self selector, not a reference pointing at the caster.
        targetInput->pointer->targType = target == actor ? 6 : 0;
        if (target != actor)
            targetInput->pointer->target.handle = target->CreateRefHandle();
        return Apply(actor, casts[slot]);
    }

    bool Casting(RE::Actor* actor, std::size_t slot) {
        return actor && slot < Slots && casts[slot] &&
               (actor->GetCurrentPackage() == casts[slot] || Interrupt(actor) == casts[slot]);
    }

    void Release(RE::Actor* actor) {
        if (!Owned(actor))
            return;
        if (Owned(Interrupt(actor)))
            actor->EndInterruptPackage(true);
        if (activity && actor->IsInFaction(activity))
            actor->RemoveFromFaction(activity);
        actor->EvaluatePackage(true, false);
    }
}
