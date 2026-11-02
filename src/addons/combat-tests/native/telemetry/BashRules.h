#pragma once
#include "CombatTestHost.h"
#include <optional>
#include <string_view>

namespace mod::combat_tests::telemetry::bash_rules {
    constexpr std::optional<yliwf::testing::BashAnimationEvent> Classify(std::string_view event) {
        using Event = yliwf::testing::BashAnimationEvent;
        if (event == "SBF_BashStart")
            return Event::Started;
        if (event == "SBF_BashStop" || event == "bashStop" || event == "bashExit")
            return Event::Ended;
        if (event == "TKDR_DodgeStart" || event == "SBF_StaggerStart" || event == "SBF_RecoilStart")
            return Event::Interrupted;
        return std::nullopt;
    }
}
