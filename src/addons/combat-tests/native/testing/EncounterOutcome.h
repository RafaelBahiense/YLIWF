#pragma once
#include <cstdint>
#include <string_view>

namespace mod::combat_tests::testing {
    struct OutcomeAssessment {};

    enum class Outcome : std::uint32_t { NotMeasured, Victory, Defeat, TimeLimit, Cancelled, Invalid };

    struct OutcomeCounts {
        std::uint32_t victories = 0, defeats = 0, timeLimits = 0, cancelled = 0, invalid = 0;

        void Add(Outcome outcome) {
            switch (outcome) {
                case Outcome::Victory:
                    ++victories;
                    break;
                case Outcome::Defeat:
                    ++defeats;
                    break;
                case Outcome::TimeLimit:
                    ++timeLimits;
                    break;
                case Outcome::Cancelled:
                    ++cancelled;
                    break;
                case Outcome::Invalid:
                    ++invalid;
                    break;
                case Outcome::NotMeasured:
                    break;
            }
        }
    };

    constexpr Outcome Resolve(bool running, bool subjectDown, bool opponentDown, bool timeLimit) {
        if (!running)
            return Outcome::NotMeasured;
        // Simultaneous defeat must not be reported as a clean victory.
        if (subjectDown)
            return Outcome::Defeat;
        if (opponentDown)
            return Outcome::Victory;
        return timeLimit ? Outcome::TimeLimit : Outcome::NotMeasured;
    }

    constexpr std::string_view Name(Outcome outcome) {
        switch (outcome) {
            case Outcome::Victory:
                return "Victory";
            case Outcome::Defeat:
                return "Defeat";
            case Outcome::TimeLimit:
                return "Time limit";
            case Outcome::Cancelled:
                return "Cancelled";
            case Outcome::Invalid:
                return "Invalid test";
            default:
                return "Not measured";
        }
    }
}
