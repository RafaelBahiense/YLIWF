#include "SceneControl.h"
#include "SceneControlRules.h"
#include "CombatTestHost.h"
#include "FollowerPolicy.h"
#include "ModInfo.h"
#include <Windows.h>
#include <algorithm>

namespace mod::combat_tests::scene_control {
    namespace {
        // All state is game-thread owned. Provider inspection never notifies the
        // host or calls back into the runner, including during policy aggregation.
        scene_rules::Scope scope;
        const yliwf::policy::API* policy = nullptr;
        std::uint32_t provider = 0, published = 0;
        std::uint32_t (*combatPackage)(std::uint32_t) = nullptr;
        RE::TESFaction* subjectFaction = nullptr;
        bool combatFallbackReady = false;

        std::uint32_t Inspect(void*, std::uint32_t actor, yliwf::policy::State* output) {
            if (!output || !scope.Includes(actor))
                return 0;
            *output = {};
            output->blocked = 0;
            output->packagePreference = yliwf::policy::PackagePreference::Requested;
            output->package = scope.Preferred(combatPackage ? combatPackage(actor) : 0);
            output->packagePriority = 1000;
            return 1;
        }
    }

    void Install() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return;
        subjectFaction = data->LookupForm<RE::TESFaction>(0x844, "YouLeadIWillFollow.CombatArena.esp");
        auto* defender = data->LookupForm<RE::TESPackage>(0x841, "YouLeadIWillFollow.CombatArena.esp");
        auto* healer = data->LookupForm<RE::TESPackage>(0x843, "YouLeadIWillFollow.CombatArena.esp");
        auto* fallback = data->LookupForm<RE::TESPackage>(0x5C851, "Skyrim.esm");
        if (!subjectFaction || !defender || !healer || !fallback)
            return;
        // Combat overrides bypass the ordinary alias-package selector. Insert
        // faction-gated arena fallbacks before vanilla's player-follow entry,
        // preserving every earlier role/quest package and every other actor.
        // DataLoaded runs before combat jobs; never mutate lists during tests.
        for (auto* list : data->GetFormArray<RE::BGSListForm>()) {
            if (!list || std::ranges::find(list->forms, defender) != list->forms.end())
                continue;
            auto found = std::ranges::find(list->forms, fallback);
            if (found == list->forms.end())
                continue;
            const auto index = static_cast<std::size_t>(found - list->forms.begin());
            list->forms.push_back(defender);
            list->forms.push_back(healer);
            std::rotate(list->forms.begin() + index, list->forms.end() - 2, list->forms.end());
            combatFallbackReady = true;
        }
    }

    bool Register(const yliwf::testing::Host& host) {
        if (provider)
            return true;
        const auto module = GetModuleHandleA(mod::info::DllFile);
        const auto query =
            module ? reinterpret_cast<yliwf::policy::QueryAPI>(GetProcAddress(module, yliwf::policy::QueryExport))
                   : nullptr;
        policy = query ? query(yliwf::policy::Version) : nullptr;
        if (!policy || policy->size < sizeof(*policy) || policy->version != yliwf::policy::Version ||
            !policy->registerProvider || !policy->stateChanged || !host.combatPackage)
            return false;
        yliwf::policy::Provider entry;
        entry.id = "yliwf.combat-tests.scene";
        entry.inspect = Inspect;
        combatPackage = host.combatPackage;
        provider = policy->registerProvider(&entry);
        return provider != 0;
    }

    void Refresh() {
        if (!provider || !scope.Actor())
            return;
        const auto desired = scope.Preferred(combatPackage(scope.Actor()));
        if (desired != published) {
            published = desired;
            policy->stateChanged(provider, scope.Actor());
        }
    }

    bool Begin(std::uint32_t actor, std::uint32_t token, bool healer) {
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* setup =
            data ? data->LookupForm<RE::TESPackage>(healer ? 0x842 : 0x840, "YouLeadIWillFollow.CombatArena.esp")
                 : nullptr;
        auto* combat =
            data ? data->LookupForm<RE::TESPackage>(healer ? 0x843 : 0x841, "YouLeadIWillFollow.CombatArena.esp")
                 : nullptr;
        auto* subject = RE::TESForm::LookupByID<RE::Actor>(actor);
        if (!provider || !combatFallbackReady || !subjectFaction || !subject || !setup || !combat ||
            !scope.Begin(actor, token, setup->GetFormID(), combat->GetFormID()))
            return false;
        subject->AddToFaction(subjectFaction, healer ? 2 : 1);
        if (subject->GetFactionRank(subjectFaction, false) != (healer ? 2 : 1)) {
            scope.End(actor, token);
            return false;
        }
        published = 0;
        Refresh();
        return true;
    }

    bool StartCombat(std::uint32_t actor, std::uint32_t token) {
        if (!scope.StartCombat(actor, token))
            return false;
        Refresh();
        return true;
    }

    void End(std::uint32_t actor, std::uint32_t token) {
        // Saved recovery may run after the in-memory scope was reset.
        if ((scope.Matches(actor, token) || !scope.Actor()) && subjectFaction)
            if (auto* subject = RE::TESForm::LookupByID<RE::Actor>(actor);
                subject && subject->IsInFaction(subjectFaction))
                subject->RemoveFromFaction(subjectFaction);
        if (!scope.End(actor, token))
            return;
        published = 0;
        if (provider)
            policy->stateChanged(provider, actor);
    }

    void Reset() {
        scope = {};
        published = 0;
    }

    std::uint32_t Requested(std::uint32_t actor) {
        return scope.Includes(actor) ? scope.Preferred(combatPackage ? combatPackage(actor) : 0) : 0;
    }
}
