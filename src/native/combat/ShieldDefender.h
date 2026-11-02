#pragma once

#include <cstdint>
#include <string>
#include "DefenderTactics.h"
#include "DefenderMovement.h"

namespace RE {
    class Actor;
}

namespace mod::shield_defender {
    struct Observation {
        bool available = false, requested = false, owned = false, yielded = false;
        bool incoming = false, committed = false, recovering = false, counter = false, targeting = false;
        std::uint32_t stance = 0, style = 0, threat = 0;
        float distance = -1, heading = 0, stamina = 0, maximumStamina = 0, guardRemaining = 0, age = -1;
        std::string reason;
        defender_tactics::Decision tactic;
        std::uint32_t threatKind = 0, threats = 0, movementRequests = 0, movementFailures = 0;
        bool moving = false;
        bool guardPreparing = false;
        bool threatVisible = false, threatAttacking = false, threatPreparing = false;
        defender_movement::Observation movement;
        float weaponReach = 0;
        std::uint32_t precisionAvailable = 0, precisionCallbacks = 0, precisionIncoming = 0, precisionOutgoing = 0;
    };

    void Configure();
    void Tick(float elapsed);
    bool NeedsObservation();
    void Invalidate();
    void UpdateCombatStyle(RE::Actor* actor);
    std::string Status(std::uint32_t actor);
    Observation Inspect(std::uint32_t actor);
}
