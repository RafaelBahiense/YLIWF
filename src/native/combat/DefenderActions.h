#pragma once

#include "CombatTestHost.h"

#include <cstdint>
#include <string>

namespace mod::defender_actions {
    struct Observation {
        bool available = false, fault = false;
        bool prepareGuardMovement = false;
        float locomotionSpeed = -1;
        bool locomotionReady = false;
        std::uint32_t mode = 0, phase = 0, actions = 0, attacks = 0, rejected = 0, engineRejected = 0;
        std::uint32_t guardRequests = 0, guardAccepted = 0, guardObserved = 0;
        std::uint32_t releaseRequests = 0, releaseAccepted = 0, releaseObserved = 0, timeouts = 0, lastAction = 0;
        std::uint32_t guardRejected = 0, dispatch = 2, validationMask = 0, processFlags = 0;
        std::int32_t processResult = -1, processReturned = -1;
        std::uint32_t blockRequests = 0, bashRequests = 0, unknownLeftRequests = 0;
        bool guardSuspended = false, blockReadable = false, wantsBlock = false, startRecovering = false;
        float startStaminaFraction = -1;
        std::uint32_t releaseRejected = 0;
        std::uint32_t dodgeDeferrals = 0, dodgeInterruptions = 0, dodgeResumes = 0;
        std::uint32_t attackDeferrals = 0, attackInterruptions = 0, attackResumes = 0;
        std::uint32_t releaseDeferrals = 0;
        bool blockHitReadable = false, blockHit = false, graphWantReadable = false;
        std::int32_t graphWantBlock = -1;
        float releaseWait = 0, releaseWaitPeak = 0;
        std::uint32_t recoveredInterruptTimeouts = 0;
        std::uint32_t nativeBashRequests = 0, nativeBashAccepted = 0, nativeBashObserved = 0, nativeBashCompleted = 0,
                      nativeBashRejected = 0, nativeBashTimeouts = 0, bashPhase = 0;
        std::string bashStatus;
        std::uint32_t powerBashRequests = 0, powerBashAccepted = 0, powerBashObserved = 0, powerBashCompleted = 0,
                      powerBashRejected = 0, powerBashTimeouts = 0;
        std::uint32_t handoffAttackRejections = 0;
        std::uint32_t nativeBashInterrupted = 0, powerBashInterrupted = 0;
        std::uint32_t bashGraphStarts = 0, bashGraphEnds = 0, bashNativeStarts = 0;
        std::uint32_t rightAttackRequests = 0, rightAttackAccepted = 0, rightAttackObserved = 0,
                      rightAttackCompleted = 0, rightAttackInterrupted = 0, rightAttackRejected = 0,
                      rightAttackTimeouts = 0, rightAttackPhase = 0;
        std::uint32_t attackGraphStarts = 0, attackGraphEnds = 0, attackNativeStarts = 0, attackRecoveryCompletions = 0;
        std::string rightAttackStatus;
        std::string guardEvent;
        std::string status;
    };

    std::uint32_t AttackRequest(std::uint32_t actor, std::uint32_t token);
    bool AttackAnimation(std::uint32_t actor, std::uint32_t token, std::uint32_t request,
                         yliwf::testing::AttackAnimationEvent event);
    void Install();
    bool BeginDirect(std::uint32_t actor, std::uint32_t token);
    bool RequestDirect(std::uint32_t actor, std::uint32_t token, yliwf::testing::PrimitiveAction action);
    bool Begin(std::uint32_t actor, std::uint32_t mode, std::uint32_t token);
    // 0 waiting, 1 finished, 2 unconfirmed release requiring manual review.
    std::uint32_t End(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase);
    // Thread-safe and engine-free; may be read by a co-save callback.
    std::uint32_t SavedPhase(std::uint32_t actor, std::uint32_t token);
    Observation Inspect(std::uint32_t actor);
    void Tick();
    bool HasWork();
    void Invalidate();
    bool SetMovementBounds(std::uint32_t actor, std::uint32_t token, std::uint32_t anchor, float radius);
    bool SetAttackCommitment(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence,
                             yliwf::testing::AttackCommitment phase);
    bool SetDefensiveBash(std::uint32_t actor, std::uint32_t token, std::uint32_t kind, std::uint32_t idle,
                          std::uint32_t gate);
    std::uint32_t BashRequest(std::uint32_t actor, std::uint32_t token);
    bool BashAnimation(std::uint32_t actor, std::uint32_t token, std::uint32_t request,
                       yliwf::testing::BashAnimationEvent event);
}
