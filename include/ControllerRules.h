#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

// Operation policy is native. Small Papyrus adapters use engine APIs whose
// side effects and latent state must remain compatible with Skyrim and saves.
namespace mod::controller_rules {
    // Saved queue/plan ABI. Never reuse a number or reorder its meaning.
    enum class Operation : std::int32_t {
        Sync = 0, Recruit = 1, Follow = 2, Wait = 3, Dismiss = 4, Death = 5, Timeout = 6, Unload = 7,
        ReservedPrepare = 8, ReservedCleanup = 9, ReservedPromote = 10, ReservedPop = 11, AnimalRecruit = 12, AnimalWait = 13,
        AnimalFollow = 14, AnimalDismiss = 15, Repair = 16, ClearSlot = 17, Adopt = 18, Release = 19,
        ReservedAddExtra = 20, ReservedClearDead = 21, HomeAssign = 22, HomeRemove = 23,
        DialogueFollow = 24, DialogueWait = 25, DialogueDismiss = 26, PromotePrimary = 27
    };
    enum class Effect : std::int32_t {
        CancelTimer = 1, Clear = 2, Assign = 3, Teammate = 4, Prepare = 5, Release = 6, Protection = 7,
        Waiting = 8, Evaluate = 9, ReservedUpdate = 10, Relationship = 11, StopCombat = 12, Cleanup = 13,
        Message = 14, Hireling = 15, DismissLine = 16, EndDismissLine = 17, StartTimer = 18,
        HideObjective = 19, Speaker = 20, ClearSpeaker = 21, AnimalPrepare = 22, AnimalCount = 23,
        HomeClear = 24, HomeMove = 25, HomeAssign = 26, HomeFaction = 27, HomeEvaluate = 28, SwapPrimary = 29
    };
    struct Slot {
        std::int32_t id;
        std::uint32_t actor = 0;
        bool valid = false, occupied = false, dead = false;
        float waiting = 0;
    };
    // Saved selection marker for captured party targets; resolve their current slot.
    constexpr std::int32_t PartySelection = -2;
    struct Request {
        Operation operation = Operation::Sync;
        std::uint32_t actor = 0;
        std::int32_t selected = -1, message = 0, sayLine = 1, cap = 1;
        bool actorValid = false, actorDead = false, recruitable = false, orphan = false;
    };
    struct PlannedStep {
        Effect effect;
        std::int32_t alias = -1;
        std::uint32_t actor = 0;
        std::int32_t argument = 0;
        std::uint32_t otherActor = 0;
        bool operator==(const PlannedStep&) const = default;
    };
    struct Plan {
        bool accepted = true;
        std::vector<PlannedStep> steps;
        void Add(Effect effect, std::int32_t alias = -1, std::uint32_t actor = 0, std::int32_t argument = 0, std::uint32_t otherActor = 0) {
            steps.push_back({effect, alias, actor, argument, otherActor});
        }
    };
    inline bool SupportedOperation(Operation operation) {
        switch (operation) {
        // Former wrapper-only operations keep their IDs reserved in the save format.
        case Operation::ReservedPrepare: case Operation::ReservedCleanup: case Operation::ReservedPromote:
        case Operation::ReservedPop: case Operation::ReservedAddExtra: case Operation::ReservedClearDead: return false;
        default: return operation >= Operation::Sync && operation <= Operation::PromotePrimary;
        }
    }
    inline bool IsHomeEffect(Effect effect) { return effect >= Effect::HomeClear && effect <= Effect::HomeEvaluate; }
    inline bool CanSwapPrimary(const Slot& primary, const Slot& extra, std::uint32_t actor, std::uint32_t previous) {
        return primary.valid && primary.id == 0 && primary.actor == previous && !primary.dead &&
            primary.occupied == (previous != 0) && extra.valid && extra.id >= 2 && extra.id <= 8 &&
            extra.occupied && extra.actor == actor && actor != 0 && actor != previous && !extra.dead;
    }
    inline bool ExpectedOccupant(Effect effect, bool occupied, std::uint32_t actualActor,
        std::uint32_t expectedActor, bool expectedDead, std::int32_t argument) {
        if (!expectedActor) return false;
        if (effect == Effect::Assign || effect == Effect::HomeAssign) return !occupied && !expectedDead;
        if (effect == Effect::HomeMove) return !occupied;
        if (actualActor != expectedActor) return false;
        if ((effect == Effect::Protection && argument == 0) ||
            ((effect == Effect::CancelTimer || effect == Effect::Clear) && argument == 1)) return expectedDead;
        return true;
    }
    inline Slot* Find(std::span<Slot> slots, std::uint32_t actor) {
        if (!actor) return nullptr;
        for (auto& slot : slots) if (slot.valid && slot.actor == actor) return &slot;
        return nullptr;
    }
    inline std::vector<std::uint32_t> PartyTargets(std::span<const Slot> slots, Operation operation) {
        std::vector<std::uint32_t> targets;
        if (operation != Operation::Follow && operation != Operation::Wait && operation != Operation::Dismiss) return targets;
        for (const auto& slot : slots) {
            if (!slot.valid || slot.id == 1 || !slot.actor || (slot.dead && operation != Operation::Dismiss)) continue;
            if (std::ranges::find(targets, slot.actor) == targets.end()) targets.push_back(slot.actor);
        }
        return targets;
    }
    inline std::int32_t Count(std::span<const Slot> slots) {
        std::int32_t count = 0;
        for (std::size_t i = 0; i < slots.size(); ++i) {
            const auto& slot = slots[i];
            if (!slot.valid || !slot.actor || slot.dead) continue;
            bool duplicate = false;
            for (std::size_t j = 0; j < i; ++j)
                if (slots[j].valid && !slots[j].dead && slots[j].actor == slot.actor) { duplicate = true; break; }
            if (!duplicate) ++count;
        }
        return count;
    }
    inline void Clear(Plan& plan, Slot& slot) {
        plan.Add(Effect::CancelTimer, slot.id, slot.actor, slot.dead ? 1 : 0);
        plan.Add(Effect::ClearSpeaker, -1, slot.actor);
        plan.Add(Effect::Clear, slot.id, slot.actor, slot.dead ? 1 : 0);
        slot.actor = 0; slot.occupied = false; slot.dead = false;
    }
    inline void Promote(Plan& plan, std::span<Slot> slots) {
        if (slots.empty() || !slots[0].valid || slots[0].occupied) return;
        for (auto& extra : slots.subspan(1)) {
            if (!extra.valid || !extra.actor || extra.dead) continue;
            const auto actor = extra.actor;
            const auto waiting = extra.waiting;
            // Assign first: an interrupted promotion never removes the actor's only registration.
            plan.Add(Effect::Assign, slots[0].id, actor);
            Clear(plan, extra);
            slots[0].actor = actor; slots[0].occupied = true; slots[0].waiting = waiting;
            plan.Add(Effect::Teammate, -1, actor, 1);
            plan.Add(Effect::Prepare, -1, actor, 1);
            if (waiting == 1) plan.Add(Effect::StartTimer, slots[0].id, actor);
            plan.Add(Effect::Evaluate, -1, actor);
            return;
        }
    }
    inline void Sync(Plan& plan, std::span<Slot> slots) {
        for (auto& slot : slots) if (slot.valid && slot.actor && slot.dead) {
            plan.Add(Effect::Protection, slot.id, slot.actor, 0);
            Clear(plan, slot);
        }
        Promote(plan, slots);
    }
    inline void Prepare(Plan& plan, std::uint32_t actor) {
        plan.Add(Effect::Relationship, -1, actor);
        plan.Add(Effect::Teammate, -1, actor, 1);
        plan.Add(Effect::StopCombat, -1, actor);
        plan.Add(Effect::Prepare, -1, actor);
    }
    inline Plan Build(std::span<const Slot> input, const Slot& animal, Request request) {
        Plan plan;
        if (!SupportedOperation(request.operation)) { plan.accepted = false; return plan; }
        std::vector<Slot> slots(input.begin(), input.end());
        auto op = request.operation;
        // An earlier party member's reconciliation can clear other dead aliases.
        // Release captured corpses' service flags, but never adopt a living orphan.
        if (request.selected == PartySelection && op == Operation::Dismiss &&
            request.actorValid && request.actorDead && !Find(slots, request.actor)) {
            op = Operation::Release;
            request.orphan = true;
        }
        if (op == Operation::DialogueFollow || op == Operation::DialogueWait || op == Operation::DialogueDismiss) {
            op = op == Operation::DialogueFollow ? Operation::Follow : op == Operation::DialogueWait ? Operation::Wait : Operation::Dismiss;
            Sync(plan, slots);
            if (!Find(slots, request.actor)) {
                request.actor = slots.empty() ? 0 : slots[0].actor;
                request.actorValid = request.actor != 0; request.actorDead = false;
            }
            request.selected = -1;
            if (!request.actor) return plan;
        }
        if (op == Operation::Sync || op == Operation::Recruit || op == Operation::Adopt) Sync(plan, slots);
        auto* own = Find(slots, request.actor);
        const auto actor = request.actor;
        auto reject = [&]() { plan.accepted = false; plan.steps.clear(); return plan; };
        if (op == Operation::Sync) return plan;
        if (!request.actorValid || !actor) return reject();
        if (op == Operation::Recruit || op == Operation::Adopt) {
            if (slots.empty() || !slots[0].valid) return reject();
            if (request.actorDead || animal.actor == actor || (op == Operation::Adopt && !request.recruitable)) return reject();
            if (!own) {
                if (Count(slots) >= std::max(request.cap, 1)) return reject();
                auto free = std::ranges::find_if(slots, [](const Slot& slot) { return slot.valid && !slot.occupied; });
                if (free == slots.end()) return reject();
                plan.Add(Effect::Assign, free->id, actor, 1);
                free->actor = actor; free->occupied = true;
            }
            Prepare(plan, actor);
            return plan;
        }
        if (op == Operation::Release) {
            if (own || animal.actor == actor || !request.orphan) return reject();
            plan.Add(Effect::Teammate, -1, actor, 0);
            plan.Add(Effect::Release, -1, actor);
            plan.Add(Effect::ClearSpeaker, -1, actor);
            Sync(plan, slots);
            return plan;
        }
        if (op >= Operation::AnimalRecruit && op <= Operation::AnimalDismiss) {
            if (!animal.valid || (op != Operation::AnimalRecruit && animal.actor != actor)) return reject();
            if (op == Operation::AnimalRecruit) {
                if (request.actorDead || own || (animal.occupied && animal.actor != actor)) return reject();
                if (!animal.occupied) plan.Add(Effect::Assign, 1, actor);
                plan.Add(Effect::AnimalPrepare, -1, actor);
                plan.Add(Effect::AnimalCount, -1, actor, 1);
            } else if (op == Operation::AnimalWait) {
                plan.Add(Effect::Waiting, -1, actor, 1); plan.Add(Effect::StartTimer, 1, actor);
            } else if (op == Operation::AnimalFollow) {
                plan.Add(Effect::Waiting, -1, actor, 0); plan.Add(Effect::CancelTimer, 1, actor); plan.Add(Effect::HideObjective, 1, actor, 20);
            } else {
                if (request.actorDead) return reject();
                plan.Add(Effect::CancelTimer, 1, actor); plan.Add(Effect::Teammate, -1, actor, 0);
                plan.Add(Effect::Protection, -1, actor, 0);
                plan.Add(Effect::AnimalCount, -1, actor, 0); plan.Add(Effect::Clear, 1, actor); plan.Add(Effect::Message, -1, actor, -2);
            }
            return plan;
        }
        if (!own) return reject();
        if (request.selected >= 0) {
            const auto selected = std::ranges::find_if(slots, [&](const Slot& slot) { return slot.id == request.selected && slot.valid && slot.actor == actor; });
            if (selected == slots.end()) return reject();
            if (op == Operation::ClearSlot) own = &*selected;
        }
        if (op == Operation::PromotePrimary) {
            if (request.actorDead || slots.empty()) return reject();
            if (own->id == 0) return plan;
            const auto& primary = slots[0];
            if (!CanSwapPrimary(primary, *own, actor, primary.actor)) return reject();
            for (auto target : {actor, primary.actor}) if (target &&
                std::ranges::count_if(slots, [target](const auto& slot) { return slot.valid && slot.actor == target; }) != 1) return reject();
            plan.Add(Effect::SwapPrimary, own->id, actor, 0, primary.actor);
            for (auto target : {actor, primary.actor}) if (target) {
                plan.Add(Effect::Teammate, -1, target, 1);
                plan.Add(Effect::Prepare, -1, target, 1);
                plan.Add(Effect::Evaluate, -1, target);
            }
            return plan;
        }
        if (op == Operation::Unload) {
            if (own->waiting == 1) plan.Add(Effect::StartTimer, own->id, actor);
            return plan;
        }
        if (op == Operation::Timeout) {
            plan.Add(Effect::CancelTimer, own->id, actor);
            if (request.actorDead || own->waiting == 0) return plan;
        }
        if ((op == Operation::Follow || op == Operation::Wait || op == Operation::Repair) && request.actorDead) return reject();
        if (op == Operation::Repair || op == Operation::Follow || op == Operation::Wait || op == Operation::Dismiss || op == Operation::Timeout || op == Operation::Death || (op == Operation::ClearSlot && request.actorDead)) {
            // Prefer the stable primary registration. Clear only duplicates of this actor.
            for (auto& slot : slots) if (&slot != own && slot.valid && slot.actor == actor) Clear(plan, slot);
        }
        if (op == Operation::Repair) {
            plan.Add(Effect::Teammate, -1, actor, 1); plan.Add(Effect::Prepare, -1, actor, 1);
            plan.Add(Effect::CancelTimer, own->id, actor);
            if (own->waiting == 1) plan.Add(Effect::StartTimer, own->id, actor);
            Sync(plan, slots); return plan;
        }
        if (op == Operation::Follow || op == Operation::Wait) {
            plan.Add(Effect::Teammate, own->id, actor, 1);
            plan.Add(Effect::Prepare, own->id, actor, 1);
            plan.Add(Effect::Waiting, -1, actor, op == Operation::Wait ? 1 : 0);
            own->waiting = op == Operation::Wait ? 1.0f : 0.0f;
            plan.Add(op == Operation::Wait ? Effect::StartTimer : Effect::CancelTimer, own->id, actor);
            if (op == Operation::Follow && own->id == 0) plan.Add(Effect::HideObjective, 0, actor, 10);
            Sync(plan, slots); return plan;
        }
        if (op == Operation::Dismiss || op == Operation::Timeout || op == Operation::Death || op == Operation::ClearSlot) {
            if (op == Operation::Death && !request.actorDead) return reject();
            const auto id = own->id;
            if ((op == Operation::Dismiss || op == Operation::Timeout) && !request.actorDead) {
                if (request.message >= 0) plan.Add(Effect::Message, id, actor, request.message);
                plan.Add(Effect::CancelTimer, id, actor);
                plan.Add(Effect::StopCombat, id, actor); plan.Add(Effect::Teammate, id, actor, 0);
                plan.Add(Effect::Cleanup, id, actor); plan.Add(Effect::Hireling, id, actor);
                if (request.sayLine == 1) plan.Add(Effect::DismissLine, id, actor);
            }
            Clear(plan, *own);
            if (op == Operation::Dismiss && request.selected == PartySelection)
                for (auto& slot : slots) if (slot.valid && slot.actor == actor) Clear(plan, slot);
            if (!Find(slots, actor)) {
                plan.Add(Effect::Teammate, -1, actor, 0); plan.Add(Effect::Release, -1, actor);
            } else {
                plan.Add(Effect::Teammate, -1, actor, 1); plan.Add(Effect::Prepare, -1, actor, 1);
                auto* remaining = Find(slots, actor);
                plan.Add(Effect::CancelTimer, remaining->id, actor);
                if (remaining->waiting == 1) plan.Add(Effect::StartTimer, remaining->id, actor);
            }
            plan.Add(Effect::EndDismissLine);
            Sync(plan, slots); return plan;
        }
        return reject();
    }
    // Validate a complete home destination before clearing any existing assignment.
    inline Plan Home(std::span<const Slot> slots, std::span<const bool> markers, const Request& request) {
        Plan plan;
        if (!request.actorValid || request.actorDead || slots.size() != 8 || markers.size() != 8) { plan.accepted = false; return plan; }
        const Slot* destination = nullptr;
        if (request.operation == Operation::HomeAssign) {
            for (std::size_t i = 0; i < slots.size(); ++i)
                if (slots[i].valid && markers[i] && (!slots[i].occupied || slots[i].actor == request.actor)) { destination = &slots[i]; break; }
            if (!destination) { plan.accepted = false; return plan; }
        }
        for (const auto& slot : slots) if (slot.valid && slot.actor == request.actor) plan.Add(Effect::HomeClear, slot.id, request.actor);
        plan.Add(Effect::HomeFaction, -1, request.actor, 0);
        if (destination) {
            plan.Add(Effect::HomeMove, destination->id, request.actor);
            plan.Add(Effect::HomeAssign, destination->id, request.actor);
            plan.Add(Effect::HomeFaction, -1, request.actor, 1);
        }
        plan.Add(Effect::HomeEvaluate, -1, request.actor);
        return plan;
    }
}
