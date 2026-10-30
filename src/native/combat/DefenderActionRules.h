#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace mod::defender_action_rules {
    enum class Mode : std::uint32_t { Styles, RecoveryGate, Guard, Combined };
    enum class Phase : std::uint32_t { Idle, StartPending, Holding, ReleasePending, Fault };
    enum class Step { None, Start, Acknowledge, Release, Finished, Fault };
    enum class Recovery { Finished, ObserveRelease, Manual };
    enum class Dispatch : std::uint32_t { Accepted, Rejected, Unconfirmed };
    enum class LeftAction : std::uint32_t { Unknown, Block, Bash };

    enum ValidationEvidence : std::uint32_t {
        InitialRejected = 1,
        InitialAllowed = 2,
        SelectionRejected = 4,
        SelectionAllowed = 8,
        ResolvedRejected = 16,
        ResolvedAllowed = 32
    };

    constexpr bool EventEquals(std::string_view event, std::string_view expected) {
        if (event.size() != expected.size())
            return false;
        for (std::size_t i = 0; i < event.size(); ++i) {
            const auto c = event[i] >= 'A' && event[i] <= 'Z' ? event[i] + ('a' - 'A') : event[i];
            if (c != expected[i])
                return false;
        }
        return true;
    }

    constexpr LeftAction ClassifyLeftAction(std::string_view event) {
        if (EventEquals(event, "blockstart"))
            return LeftAction::Block;
        if (EventEquals(event, "bashstart") || EventEquals(event, "bashpowerstart"))
            return LeftAction::Bash;
        return LeftAction::Unknown;
    }

    constexpr bool RejectLeftAction(LeftAction action, bool ownGuard, bool suppressOffense) {
        // A requested guard must never execute a bash or an unidentified event.
        // Other callers retain unknown actions; only known bashes are gated.
        return ownGuard ? action != LeftAction::Block : suppressOffense && action == LeftAction::Bash;
    }

    constexpr Dispatch ClassifyDispatch(bool processSeen, bool processReturned, std::int32_t result,
                                        std::uint32_t flags, bool rejectedBeforeDispatch = false) {
        if (!processSeen || (flags & 1))
            return Dispatch::Unconfirmed;
        if (!processReturned && result == -1 && flags == 0 && rejectedBeforeDispatch)
            return Dispatch::Rejected;
        if (result != 0)
            return Dispatch::Unconfirmed;
        return processReturned ? Dispatch::Accepted : Dispatch::Rejected;
    }

    inline constexpr float AcknowledgeTimeout = 0.75f;
    inline constexpr float ReleaseTimeout = 1.0f;

    constexpr bool ReleaseCleared(bool graphReadable, bool blocking, bool wantsBlock) {
        return graphReadable && !blocking && !wantsBlock;
    }

    constexpr bool ReconcileRelease(bool completed, bool graphReadable, bool blocking, bool wantsBlock) {
        return completed && ReleaseCleared(graphReadable, blocking, wantsBlock);
    }

    constexpr Recovery RestoreGuard(Phase phase, bool blocking) {
        if (phase == Phase::Idle || (phase == Phase::Holding && !blocking))
            return Recovery::Finished;
        if (phase == Phase::ReleasePending)
            return Recovery::ObserveRelease;
        return Recovery::Manual;
    }

    constexpr bool ValidMode(std::uint32_t value) {
        return value <= 3;
    }

    constexpr bool Gate(Mode mode) {
        return mode == Mode::RecoveryGate || mode == Mode::Combined;
    }

    constexpr bool Guard(Mode mode) {
        return mode == Mode::Guard || mode == Mode::Combined;
    }

    constexpr bool Suppress(Mode mode, bool live, bool recovering, bool guarding, bool offensiveRequest) {
        return live && Gate(mode) && offensiveRequest && (recovering || (Guard(mode) && guarding));
    }

    inline Step Decide(Phase phase, bool wanted, bool canStart, bool blocking, bool armed, float elapsed,
                       bool graphReadable = true, bool wantsBlock = false) {
        if (!std::isfinite(elapsed) || elapsed < 0)
            return Step::Fault;
        switch (phase) {
            case Phase::Idle:
                return wanted && canStart && graphReadable && !wantsBlock && !blocking && armed ? Step::Start
                                                                                                : Step::None;
            case Phase::StartPending:
                if (graphReadable && blocking)
                    return Step::Acknowledge;
                return elapsed >= AcknowledgeTimeout ? Step::Release : Step::None;
            case Phase::Holding:
                if (ReleaseCleared(graphReadable, blocking, wantsBlock))
                    return Step::Finished;
                return wanted ? Step::None : Step::Release;
            case Phase::ReleasePending:
                if (ReleaseCleared(graphReadable, blocking, wantsBlock) && elapsed >= 0.1f)
                    return Step::Finished;
                return elapsed >= ReleaseTimeout ? Step::Fault : Step::None;
            case Phase::Fault:
                return Step::Fault;
        }
        return Step::Fault;
    }
}
