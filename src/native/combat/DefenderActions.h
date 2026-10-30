#pragma once

#include <cstdint>
#include <string>

namespace mod::defender_actions {
    struct Observation {
        bool available = false, fault = false;
        std::uint32_t mode = 0, phase = 0, actions = 0, attacks = 0, rejected = 0, engineRejected = 0;
        std::uint32_t guardRequests = 0, guardAccepted = 0, guardObserved = 0;
        std::uint32_t releaseRequests = 0, releaseAccepted = 0, releaseObserved = 0, timeouts = 0, lastAction = 0;
        std::uint32_t guardRejected = 0, dispatch = 2, validationMask = 0, processFlags = 0;
        std::int32_t processResult = -1, processReturned = -1;
        std::uint32_t blockRequests = 0, bashRequests = 0, unknownLeftRequests = 0;
        bool guardSuspended = false, blockReadable = false, wantsBlock = false, startRecovering = false;
        float startStaminaFraction = -1;
        std::uint32_t releaseRejected = 0;
        std::string guardEvent;
        std::string status;
    };

    void Install();
    bool Begin(std::uint32_t actor, std::uint32_t mode, std::uint32_t token);
    // 0 waiting, 1 finished, 2 unconfirmed release requiring manual review.
    std::uint32_t End(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase);
    // Thread-safe and engine-free; may be read by a co-save callback.
    std::uint32_t SavedPhase(std::uint32_t actor, std::uint32_t token);
    Observation Inspect(std::uint32_t actor);
    void Tick();
    bool HasWork();
    void Invalidate();
}
