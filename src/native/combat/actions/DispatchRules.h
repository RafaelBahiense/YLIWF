#pragma once
#include <cstdint>
#include <string_view>

namespace mod::combat_actions {
    enum class Intent { Guard, ReleaseGuard, Bash, PowerBash, RightAttack };

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

    constexpr bool AllowedRightAttackEvent(std::string_view event) {
        return event == "attackStart";
    }

    constexpr bool AllowedBashEvent(std::string_view event, Intent intent) {
        return intent == Intent::PowerBash ? event == "bashPowerStart" : intent == Intent::Bash && event == "bashStart";
    }
}
