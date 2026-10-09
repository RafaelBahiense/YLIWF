#include "FollowDistance.h"
#include "Settings.h"
#include "Adapters.h"
#include "ControllerRuntime.h"
#include "Debug.h"

namespace mod::follow_distance {
    namespace {
        RE::TESFaction* marker = nullptr;

        bool HumanSlot(std::uint32_t id) {
            return id == 0 || (id >= 2 && id <= 8);
        }

        std::int32_t Rank(RE::Actor* actor) {
            return marker && actor ? actor->GetFactionRank(marker, false) : -1;
        }

        template <class Visit>
        void NativeFollowers(Visit visit) {
            auto* quest = controller::detail::GetContext().quest;
            if (!quest)
                return;
            for (auto* alias : quest->aliases) {
                if (!alias || !HumanSlot(alias->aliasID))
                    continue;
                auto* reference = skyrim_cast<RE::BGSRefAlias*>(alias);
                auto* actor = reference ? reference->GetActorReference() : nullptr;
                if (actor && !adapters::Owns(actor->GetFormID()))
                    visit(actor);
            }
        }
    }

    void Configure(RE::TESFaction* faction) {
        marker = faction;
    }

    bool Available() {
        return marker != nullptr;
    }

    Choice Read(RE::Actor* actor) {
        return Decode(Rank(actor), settings::FollowDistance);
    }

    bool Remember(RE::Actor* actor, std::int32_t preset, bool individual) {
        if (!marker || !actor || !ValidPreset(preset))
            return false;
        const auto rank = Encode(preset, individual);
        if (Rank(actor) != rank)
            actor->AddToFaction(marker, rank);
        return Rank(actor) == rank;
    }

    void ApplyActor(RE::Actor* actor) {
        if (!actor || actor->IsDead())
            return;
        if (!ValidRank(Rank(actor)) && !Remember(actor, settings::FollowDistance, false))
            return;
        actor->EvaluatePackage();
    }

    void ApplyAll() {
        std::scoped_lock lock(settings::Mutex);
        NativeFollowers([](RE::Actor* actor) {
            if (Remember(actor, settings::FollowDistance, false) && !actor->IsDead())
                actor->EvaluatePackage();
        });
        adapters::ApplyFollowDistance(true);
        debug::NotifyStateChanged();
    }

    void Loaded() {
        std::scoped_lock lock(settings::Mutex);
        NativeFollowers(ApplyActor);
        adapters::ApplyFollowDistance();
        debug::NotifyStateChanged();
    }

    void Request(std::uint32_t actorID, std::int32_t aliasID, std::int32_t preset, std::uint64_t generation) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !ValidPreset(preset) || !debug::IsCurrentGame(generation))
            return;
        tasks->AddTask([actorID, aliasID, preset, generation] {
            std::scoped_lock lock(settings::Mutex);
            if (!debug::IsCurrentGame(generation))
                return;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
            if (!actor || actor->IsDead())
                return;
            if (adapters::Owns(actorID)) {
                if (!adapters::SetFollowDistance(actorID, preset))
                    return;
            } else {
                auto* quest = controller::detail::GetContext().quest;
                if (!quest || aliasID < 0 || !HumanSlot(static_cast<std::uint32_t>(aliasID)))
                    return;
                RE::BGSRefAlias* reference = nullptr;
                for (auto* slot : quest->aliases)
                    if (slot && slot->aliasID == static_cast<std::uint32_t>(aliasID)) {
                        reference = skyrim_cast<RE::BGSRefAlias*>(slot);
                        break;
                    }
                if (!reference || reference->GetActorReference() != actor || !Remember(actor, preset, true))
                    return;
                actor->EvaluatePackage();
            }
            debug::NotifyStateChanged();
        });
    }
}
