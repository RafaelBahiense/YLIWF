#pragma once
#include <string_view>

namespace mod::combat_tests::interference::external_attack_rules {
    constexpr bool Equal(std::string_view a, std::string_view b) {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            const auto lower = [](char ch) { return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch; };
            if (lower(a[i]) != lower(b[i]))
                return false;
        }
        return true;
    }

    enum class Source { None, GapClose, PermaBlock };

    constexpr Source Classify(std::string_view module, std::string_view event) {
        if (Equal(module, "ProjectGapClose.dll") &&
            (Equal(event, "attackStartSprint") || Equal(event, "attackPowerStart_Sprint")))
            return Source::GapClose;
        if (Equal(module, "FixNPCPermaBlock.dll") && Equal(event, "attackStart"))
            return Source::PermaBlock;
        return Source::None;
    }

    constexpr bool Supported(std::string_view module, std::string_view event) {
        return Classify(module, event) != Source::None;
    }

    constexpr bool StartEvent(std::string_view event) {
        return Equal(event, "Collision_AttackStart") || Equal(event, "MCO_SprintPowerAttackInitiate") ||
               Equal(event, "MCO_SprintAttackInitiate") || Equal(event, "SBF_PowerAttackStart") ||
               Equal(event, "SBF_NormalAttackStart");
    }

    constexpr bool EndEvent(std::string_view event) {
        return Equal(event, "MCO_EndAnimation") || Equal(event, "SBF_PowerAttackStop") ||
               Equal(event, "SBF_NormalAttackStop");
    }

    struct State {
        enum class Finish { None, Animation, RecoveryCycle, Rejected };
        enum class Family { Unknown, Normal, Power };
        bool returned = false, accepted = false, started = false, ended = false, nativeSeen = false;
        bool graphStarted = false, recoverySeen = false;
        bool handingOff = false;
        Family family = Family::Unknown;
        Finish finish = Finish::None;
        unsigned idleSamples = 0;

        void Event(std::string_view event) {
            if (StartEvent(event)) {
                started = true;
                graphStarted = true;
                if (Equal(event, "SBF_NormalAttackStart") || Equal(event, "MCO_SprintAttackInitiate"))
                    family = Family::Normal;
                else if (Equal(event, "SBF_PowerAttackStart") || Equal(event, "MCO_SprintPowerAttackInitiate"))
                    family = Family::Power;
                ended = false;
                finish = Finish::None;
                nativeSeen = false;
                recoverySeen = false;
                idleSamples = 0;
            } else if (started && EndEvent(event) &&
                       (Equal(event, "MCO_EndAnimation") ||
                        (family != Family::Power && Equal(event, "SBF_NormalAttackStop")) ||
                        (family != Family::Normal && Equal(event, "SBF_PowerAttackStop"))))
                ended = handingOff = true;
        }

        bool Sample(bool attacking, bool recoveryReadable, bool recovering) {
            if ((started || attacking) && recoveryReadable && recovering)
                recoverySeen = true;
            if (attacking) {
                nativeSeen = started = true;
                idleSamples = 0;
                return false;
            }
            if (!returned || (recoveryReadable && recovering)) {
                idleSamples = 0;
                return false;
            }
            // Native idle alone can precede animation cleanup. For unrecognized
            // animation graphs require an observed recovery cycle as fallback.
            const bool fallback = !graphStarted && nativeSeen && recoverySeen && recoveryReadable;
            const bool endKnown = ended || fallback || (!accepted && !started);
            if (!endKnown) {
                idleSamples = 0;
                return false;
            }
            if (ended || fallback)
                handingOff = true;
            if (++idleSamples < 2)
                return false;
            finish = ended ? Finish::Animation : fallback ? Finish::RecoveryCycle : Finish::Rejected;
            return true;
        }
    };
}
