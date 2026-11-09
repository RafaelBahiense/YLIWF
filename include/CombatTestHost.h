#pragma once
#include <cstdint>
#include "DefenderTuning.h"

// Combat-test backend services. Engine callbacks run on the main game thread.
// No engine or STL objects cross the DLL boundary.
namespace yliwf::testing {
    inline constexpr std::uint32_t Version = 1;
    inline constexpr char QueryExport[] = "YLIWF_GetCombatTestHost";
    inline constexpr std::uint32_t DefenderControl = 3;

    enum class CommandState : std::uint32_t { Unknown, Pending, Succeeded, Failed };

    struct Subject {
        std::uint32_t size = sizeof(Subject), managed = 0, allowed = 0, role = 0;
        char reason[192]{}, behavior[192]{};
        std::uint32_t eligibleRoles = 0;
    };

    enum class AttackCommitment : std::uint32_t { Clear, Active, Finishing };
    enum class BashAnimationEvent : std::uint32_t { Started, Ended, Interrupted };

    enum class PrimitiveAction : std::uint32_t { Guard, Release, Bash, PowerBash, RightAttack };
    enum class EvadeDirection : std::uint32_t { Back, Left, Right };

    struct EvadeBackend {
        std::uint32_t size = sizeof(EvadeBackend);
        // Main-thread callbacks. Negative cost means unavailable. The provider
        // executes the animation and owns its stamina cost and immunity rules.
        float (*cost)(std::uint32_t actor) = nullptr;
        bool (*request)(std::uint32_t actor, EvadeDirection direction) = nullptr;
        float clearance = 384;
    };

    enum class AttackAnimationEvent : std::uint32_t {
        Started,
        Ended,
        Interrupted,
        ConflictingStarted,
        ConflictingEnded
    };

    struct Host {
        std::uint32_t size = sizeof(Host), version = Version;
        std::uint32_t (*inspect)(std::uint32_t actor, Subject*) = nullptr;
        std::uint32_t (*setRole)(std::uint32_t actor, std::uint32_t role) = nullptr;
        std::uint32_t (*observe)(std::uint32_t actor, struct DefenderObservation*) = nullptr;
        // control must be DefenderControl; other values are rejected.
        std::uint32_t (*beginDefense)(std::uint32_t actor, std::uint32_t control, std::uint32_t token) = nullptr;
        std::uint32_t (*endDefense)(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase) = nullptr;
        // Thread-safe, engine-free recovery state; permitted during co-save writing.
        std::uint32_t (*defenseState)(std::uint32_t actor, std::uint32_t token) = nullptr;
        // Restore requests require the captured command sequence; newer commands retain priority.
        std::uint32_t (*setFollowing)(std::uint32_t actor, bool following, std::uint32_t expected) = nullptr;
        // Requests are observed without replaying them.
        CommandState (*commandState)(std::uint32_t request) = nullptr;
        // Thread-safe and engine-free; actor/token/sequence ownership is checked on every call.
        // Finishing retains ownership while blocking new offensive actions.
        bool (*setAttackCommitment)(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence,
                                    AttackCommitment phase) = nullptr;
        // kind: 0 off, 1 normal, 2 power; configured before guard acquisition.
        // Power requires a standalone test idle and the live power-root condition gate.
        bool (*setDefensiveBash)(std::uint32_t actor, std::uint32_t token, std::uint32_t kind, std::uint32_t idle,
                                 std::uint32_t gate) = nullptr;
        // Engine-free graph evidence; read a request ID and return it unchanged with the event.
        std::uint32_t (*bashRequest)(std::uint32_t actor, std::uint32_t token) = nullptr;
        bool (*bashAnimation)(std::uint32_t actor, std::uint32_t token, std::uint32_t request,
                              BashAnimationEvent event) = nullptr;
        // Explicit primitive requests bypass role tactics. Acceptance queues one
        // request; observe reports its engine outcome and lifecycle evidence.
        std::uint32_t (*beginDirect)(std::uint32_t actor, std::uint32_t token) = nullptr;
        bool (*requestDirect)(std::uint32_t actor, std::uint32_t token, PrimitiveAction action) = nullptr;
        std::uint32_t (*attackRequest)(std::uint32_t actor, std::uint32_t token) = nullptr;
        bool (*attackAnimation)(std::uint32_t actor, std::uint32_t token, std::uint32_t request,
                                AttackAnimationEvent event) = nullptr;
        std::uint32_t observationSize = 0;
        // Main-thread, active actor/token only. Replaces the spectator tether
        // with an anchor/radius in the same area until teardown or revert.
        bool (*setMovementBounds)(std::uint32_t actor, std::uint32_t token, std::uint32_t anchor,
                                  float radius) = nullptr;
        // Configure once, before combat, for the active actor/token only.
        bool (*setDefenderTuning)(std::uint32_t actor, std::uint32_t token, const DefenderTuning*) = nullptr;
        // Main-thread, read-only: first eligible core combat-role alias package,
        // or zero. Excludes ordinary follow/sandbox packages and grants no permission.
        std::uint32_t (*combatPackage)(std::uint32_t actor) = nullptr;
        // Test actor/token only, configured before combat. Copied callbacks are
        // discarded at scope end/revert; provider must outlive the scope.
        bool (*setEvadeBackend)(std::uint32_t actor, std::uint32_t token, const EvadeBackend*) = nullptr;
    };

    enum class ThreatKind : std::uint32_t { Unknown, Melee, HeavyMelee, Bow, Projectile, Concentration, Area };

    struct DefenderObservation {
        std::uint32_t size = sizeof(DefenderObservation), available = 0, requested = 0, owned = 0, yielded = 0;
        std::uint32_t stance = 0, style = 0, threat = 0, incoming = 0, committed = 0, recovering = 0, counter = 0,
                      targeting = 0;
        float distance = -1, heading = 0, stamina = 0, maximumStamina = 0, guardRemaining = 0, age = -1;
        char reason[192]{};
        std::uint32_t liveStyle = 0;
        std::uint32_t actionAvailable = 0, actionMode = 0, guardPhase = 0, actionFault = 0;
        std::uint32_t actionRequests = 0, attackRequests = 0, attacksRejected = 0, engineRejected = 0;
        std::uint32_t guardRequests = 0, guardAccepted = 0, guardObserved = 0;
        std::uint32_t releaseRequests = 0, releaseAccepted = 0, releaseObserved = 0, actionTimeouts = 0, lastAction = 0;
        char actionStatus[192]{};
        std::uint32_t guardRejected = 0, dispatch = 2, validationMask = 0, processFlags = 0;
        std::int32_t processResult = -1, processReturned = -1;
        std::uint32_t blockRequests = 0, bashRequests = 0, unknownLeftRequests = 0;
        char guardEvent[96]{};
        std::uint32_t guardSuspended = 0, blockReadable = 0, wantsBlock = 0, startRecovering = 0, releaseRejected = 0;
        float startStaminaFraction = -1;
        std::uint32_t dodgeDeferrals = 0, dodgeInterruptions = 0, dodgeResumes = 0;
        std::uint32_t attackDeferrals = 0, attackInterruptions = 0, attackResumes = 0;
        std::uint32_t releaseDeferrals = 0, blockHitReadable = 0, blockHit = 0, graphWantReadable = 0;
        std::int32_t graphWantBlock = -1;
        float releaseWait = 0, releaseWaitPeak = 0;
        // A missed start is recovered only after interrupted guard cleanup and a verified resume.
        std::uint32_t recoveredInterruptTimeouts = 0;
        std::uint32_t nativeBashRequests = 0, nativeBashAccepted = 0, nativeBashObserved = 0, nativeBashCompleted = 0,
                      nativeBashRejected = 0, nativeBashTimeouts = 0, bashPhase = 0;
        char bashStatus[128]{};
        std::uint32_t powerBashRequests = 0, powerBashAccepted = 0, powerBashObserved = 0, powerBashCompleted = 0,
                      powerBashRejected = 0, powerBashTimeouts = 0;
        std::uint32_t handoffAttackRejections = 0;
        std::uint32_t nativeBashInterrupted = 0, powerBashInterrupted = 0;
        std::uint32_t bashGraphStarts = 0, bashGraphEnds = 0, bashNativeStarts = 0;
        std::uint32_t rightAttackRequests = 0, rightAttackAccepted = 0, rightAttackObserved = 0,
                      rightAttackCompleted = 0, rightAttackInterrupted = 0, rightAttackRejected = 0,
                      rightAttackTimeouts = 0, rightAttackPhase = 0;
        std::uint32_t attackGraphStarts = 0, attackGraphEnds = 0, attackNativeStarts = 0, attackRecoveryCompletions = 0;
        char rightAttackStatus[192]{};
        std::uint32_t intent = 0, motion = 0, threatKind = 0, threatCount = 0, moving = 0;
        std::uint32_t movementRequests = 0, movementFailures = 0, precisionAvailable = 0, precisionCallbacks = 0;
        float weaponReach = 0;
        std::uint32_t precisionIncoming = 0, precisionOutgoing = 0;
        std::uint32_t movementStalls = 0, movementDetours = 0, movementStatus = 0, guardPreparing = 0;
        float destinationX = 0, destinationY = 0, destinationZ = 0, movementProgress = 0, movementWait = 0;
        float guardLocomotionSpeed = -1;
        std::uint32_t guardLocomotionReady = 0;
        std::uint32_t threatVisible = 0, threatAttacking = 0, threatPreparing = 0;
        std::uint32_t tauntAvailable = 0, tauntPhase = 0, tauntEnemy = 0, tauntSpell = 0;
        std::uint32_t tauntRequests = 0, tauntObserved = 0, tauntUnobserved = 0, tauntReleased = 0, tauntCancelled = 0,
                      tauntTargetRejections = 0, tauntEffectPresent = 0;
        float tauntRemaining = 0, tauntCooldown = 0;
        std::uint32_t precisionPreCallbacks = 0, precisionPreOutgoing = 0, precisionLastCandidate = 0;
        std::int32_t precisionActorActive = -1, precisionThreatActive = -1, precisionThreatControllerHittable = -1;
        std::uint32_t casterBashRequests = 0, casterBashCompleted = 0;
        std::uint32_t flightReadable = 0, projectileGap = 0, incomingProjectiles = 0, projectilesExamined = 0;
        std::uint32_t evadeAvailable = 0, evadePhase = 0, evadeRequests = 0, evadeAccepted = 0, evadeObserved = 0,
                      evadeCompleted = 0, evadeRejected = 0, evadeTimeouts = 0;
        float evadeDisplacement = 0;
    };

    using QueryHost = const Host* (*)(std::uint32_t);
    static_assert(sizeof(Subject) == 404 && sizeof(Host) == 176);
}
