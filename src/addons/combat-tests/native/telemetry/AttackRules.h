#pragma once
#include "CombatTestHost.h"
#include <optional>
#include <string_view>

namespace mod::combat_tests::telemetry::attack_rules {
    constexpr std::optional<yliwf::testing::AttackAnimationEvent> Classify(std::string_view event) {
        using Event = yliwf::testing::AttackAnimationEvent;
        if (event == "SBF_NormalAttackStart")
            return Event::Started;
        if (event == "SBF_NormalAttackStop" || event == "MCO_EndAnimation")
            return Event::Ended;
        if (event == "SBF_PowerAttackStart" || event == "SBF_BashStart")
            return Event::ConflictingStarted;
        if (event == "SBF_PowerAttackStop" || event == "SBF_BashStop" || event == "bashStop" || event == "bashExit")
            return Event::ConflictingEnded;
        if (event == "TKDR_DodgeStart" || event == "SBF_StaggerStart" || event == "SBF_RecoilStart")
            return Event::Interrupted;
        return std::nullopt;
    }
}
