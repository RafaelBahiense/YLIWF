#include "FixtureBases.h"
#include "FixtureSpells.h"
#include "Archetypes.h"
#include "RE/Skyrim.h"
#include <array>
#include <algorithm>

namespace mod::combat_tests::fixture_bases {
    namespace {
        constexpr const char* Plugin = "YouLeadIWillFollow.CombatArena.esp";
        std::array<RE::TESNPC*, Count> bases{};
        std::array<bool, Count> prepared{};
    }

    void Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        for (std::uint32_t index = 0; index < Count; ++index)
            bases[index] = data ? data->LookupForm<RE::TESNPC>(First + index, Plugin) : nullptr;
        Reset();
    }

    void Reset() {
        prepared = {};
    }

    bool Owns(const RE::TESNPC* base) {
        return base && std::ranges::find(bases, base) != bases.end();
    }

    bool IsAlly(const RE::TESNPC* base) {
        return base && base == bases[Ally];
    }

    RE::TESNPC* Prepare(std::uint32_t loadout) {
        if (loadout >= Count || !bases[loadout])
            return nullptr;
        auto* base = bases[loadout];
        if (!prepared[loadout]) {
            auto* data = RE::TESDataHandler::GetSingleton();
            const auto sourceID = fixtures::Template(static_cast<fixtures::Archetype>(loadout));
            auto* source = data ? data->LookupForm<RE::TESNPC>(sourceID, "Skyrim.esm") : nullptr;
            if (!source || source == base)
                return nullptr;
            // Copy components into an ESP-defined record. Its stable FormID is
            // resolvable on reload even when a deleted actor remains in the ESS.
            const auto identity = base->GetFormID();
            base->Copy(source);
            if (base->GetFormID() != identity)
                return nullptr;
            base->fullName = loadout == Ally ? AllyName : Name;
            base->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kInvulnerable,
                                                 RE::ACTOR_BASE_DATA::Flag::kEssential,
                                                 RE::ACTOR_BASE_DATA::Flag::kProtected);
            base->actorData.templateUseFlags.reset(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kSpells);
            if (!fixture_spells::DetachPassives(base, source))
                return nullptr;
            prepared[loadout] = true;
        }
        return base;
    }
}
