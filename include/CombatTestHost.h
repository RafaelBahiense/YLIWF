#pragma once
#include <cstdint>

// Combat-test backend services. Engine callbacks run on the main game thread.
// No engine or STL objects cross the DLL boundary.
namespace yliwf::testing {
    inline constexpr std::uint32_t Version = 1;
    inline constexpr char QueryExport[] = "YLIWF_GetCombatTestHost";

    struct Subject {
        std::uint32_t size = sizeof(Subject), managed = 0, allowed = 0, role = 0;
        char reason[192]{}, behavior[192]{};
        std::uint32_t eligibleRoles = 0;
    };

    struct Host {
        std::uint32_t size = sizeof(Host), version = Version;
        std::uint32_t (*inspect)(std::uint32_t actor, Subject*) = nullptr;
        std::uint32_t (*setRole)(std::uint32_t actor, std::uint32_t role) = nullptr;
        std::uint32_t (*observe)(std::uint32_t actor, struct DefenderObservation*) = nullptr;
        std::uint32_t (*beginDefense)(std::uint32_t actor, std::uint32_t mode, std::uint32_t token) = nullptr;
        std::uint32_t (*endDefense)(std::uint32_t actor, std::uint32_t token, std::uint32_t savedPhase) = nullptr;
        // Thread-safe, engine-free recovery state; permitted during co-save writing.
        std::uint32_t (*defenseState)(std::uint32_t actor, std::uint32_t token) = nullptr;
    };

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
    };

    using QueryHost = const Host* (*)(std::uint32_t);
    static_assert(sizeof(Subject) == 404 && sizeof(Host) == 56);
}
