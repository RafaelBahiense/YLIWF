#include "StandDownMovement.h"
#include "CombatOrderRules.h"
#include "HealerMovement.h"
#include "ModInfo.h"
#include "followers/FollowDistance.h"
#include "diagnostics/Debug.h"
#include <array>

namespace mod::stand_down_movement {
    namespace {
        constexpr std::size_t Slots = 16;
        std::array<RE::TESPackage*, 4> passive{};
        std::array<RE::TESPackage*, Slots * 2> avoidance{};
        RE::TESFaction* activity = nullptr;

        struct Destination {
            RE::TESObjectREFR* marker = nullptr;
            RE::ActorHandle actor;
            std::optional<formation_rules::Point> goal;
            std::size_t packageIndex = 0;
        };

        std::array<Destination, Slots> destinations{};

        formation_rules::Point Position(RE::TESObjectREFR* actor) {
            const auto position = actor->GetPosition();
            return {position.x, position.y, position.z};
        }

        void Clear(RE::Actor* actor) {
            if (activity && actor->IsInFaction(activity))
                actor->RemoveFromFaction(activity);
            for (auto& destination : destinations)
                if (destination.actor == actor->CreateRefHandle()) {
                    destination.actor = {};
                    destination.goal.reset();
                }
        }
    }

    bool Configure() {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return false;
        activity = data->LookupForm<RE::TESFaction>(0xA15, info::PluginFile);
        for (std::size_t index = 0; index < passive.size(); ++index)
            passive[index] = data->LookupForm<RE::TESPackage>(static_cast<RE::FormID>(0xA11 + index), info::PluginFile);
        for (std::size_t slot = 0; slot < avoidance.size(); ++slot)
            avoidance[slot] = data->LookupForm<RE::TESPackage>(static_cast<RE::FormID>(0xA20 + slot), info::PluginFile);
        for (std::size_t slot = 0; slot < Slots; ++slot) {
            destinations[slot].marker =
                data->LookupForm<RE::TESObjectREFR>(static_cast<RE::FormID>(0xA40 + slot), info::PluginFile);
        }
        return activity && std::ranges::all_of(passive, [](const auto* p) { return p != nullptr; }) &&
               std::ranges::all_of(avoidance, [](const auto* p) { return p != nullptr; }) &&
               std::ranges::all_of(destinations, [](const auto& d) { return d.marker != nullptr; });
    }

    bool Owned(const RE::TESPackage* package) {
        return package && (std::ranges::find(passive, package) != passive.end() ||
                           std::ranges::find(avoidance, package) != avoidance.end());
    }

    RE::TESPackage* Select(RE::Actor* actor) {
        const auto choice = follow_distance::Read(actor);
        const auto preset = choice.preset;
        follow_distance::Remember(actor, preset, choice.individual);
        auto* fallback = passive[1 + preset];
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kWaitingForPlayer) > 0) {
            Clear(actor);
            return passive[0];
        }
        if (!player || actor->GetWorldspace() != player->GetWorldspace() ||
            (!player->GetWorldspace() && actor->GetParentCell() != player->GetParentCell())) {
            Clear(actor);
            return fallback;
        }
        auto threats = healer_movement::Threats(actor, true);
        const auto origin = Position(actor), center = Position(player);
        const auto radius = combat_order_rules::FollowRadius(preset);
        auto found =
            std::ranges::find_if(destinations, [actor](const auto& d) { return d.actor == actor->CreateRefHandle(); });
        if (found != destinations.end() && found->goal && found->marker->GetWorldspace() == actor->GetWorldspace() &&
            (actor->GetWorldspace() || found->marker->GetParentCell() == actor->GetParentCell()) &&
            combat_order_rules::KeepDestination(*found->goal, center, threats, radius) &&
            (healing_rules::NearestThreat(origin, threats) >= combat_order_rules::Clearance ||
             healing_rules::EscapeDirection(origin, *found->goal, threats)))
            return avoidance[found->packageIndex];

        if (healing_rules::NearestThreat(origin, threats) >= combat_order_rules::DangerDistance) {
            Clear(actor);
            return fallback;
        }
        const auto goal = healer_movement::PassiveDestination(actor, player, radius, threats);
        if (!goal) {
            Clear(actor);
            return fallback;
        }
        if (found == destinations.end()) {
            found = std::ranges::find_if(destinations, [](const auto& d) { return !d.actor.get(); });
            if (found != destinations.end())
                found->goal.reset();
        }
        if (found == destinations.end())
            return fallback;
        const auto slot = static_cast<std::size_t>(found - destinations.begin());
        auto* marker = found->marker;
        // Only reposition our marker. Skyrim owns the follower's route and locomotion.
        if (marker->GetWorldspace() != actor->GetWorldspace() || marker->GetParentCell() != actor->GetParentCell())
            marker->MoveTo(actor);
        const bool changed = !found->goal || healing_rules::SpatialDistance(*goal, *found->goal) >= 96;
        if (changed) {
            marker->SetPosition({goal->x, goal->y, goal->z});
            found->goal = goal;
            // Alternating records force Skyrim to rebuild the route to a moved marker.
            found->packageIndex = actor->GetCurrentPackage() == avoidance[slot] ? Slots + slot : slot;
            debug::Trace("CombatOrder.Avoid", actor, "slot={} threats={} destination=({:.0f},{:.0f},{:.0f})", slot,
                         threats.size(), goal->x, goal->y, goal->z);
        }
        found->actor = actor->CreateRefHandle();
        actor->AddToFaction(activity, static_cast<std::int8_t>(found->packageIndex));
        return avoidance[found->packageIndex];
    }

    void Release(RE::Actor* actor) {
        if (actor)
            Clear(actor);
    }

    void Invalidate() {
        for (auto& destination : destinations) {
            destination.actor = {};
            destination.goal.reset();
        }
    }
}
