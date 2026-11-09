#pragma once
#include "DispatchRules.h"
#include <cstdint>
#include <string>

namespace RE {
    class Actor;
}

namespace mod::combat_actions {
    struct DispatchEvidence {
        bool seen = false, returned = false;
        bool rejectedBeforeDispatch = false, externalBlocked = false;
        std::int32_t result = -1;
        std::uint32_t flags = 0, validationMask = 0;
        std::string event, rejection;
    };

    struct Receipt {
        Dispatch outcome = Dispatch::Rejected;
        DispatchEvidence evidence;
        bool attempted = false;
    };

    struct Capabilities {
        bool installed = false, process = false, selection = false, resolved = false;
    };

    struct Counters {
        std::uint32_t actions = 0, attacks = 0, rejected = 0, engineRejected = 0, lastAction = 0;
        std::uint32_t blockRequests = 0, bashRequests = 0, unknownLeftRequests = 0;
        std::uint32_t handoffAttackRejections = 0;
    };

    // Main-thread dispatch for one experimental actor scope. Acceptance is not
    // animation acknowledgement. Callers retain lifecycle and permission checks.
    void Install();
    Capabilities Available();
    bool Active();
    void Begin(std::uint32_t actor, std::uint32_t token);
    void RestoreScope(std::uint32_t actor, std::uint32_t token);
    void Clear();
    Receipt Send(RE::Actor* actor, std::uint32_t token, Intent intent, std::uint32_t idle = 0, std::uint32_t gate = 0);
    Counters Inspect();
    std::uint32_t ForeignGuardEpoch();
    void PublishSuppression(bool value, bool recovering = false, bool safeRecoveryGap = false);

    // Engine-free callback mailbox; never takes the caller's controller mutex.
    bool ExternalActive();
    void CloseExternal();
    bool SetExternal(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence, bool active);
    bool FinishExternal(std::uint32_t actor, std::uint32_t token, std::uint32_t sequence);
}
