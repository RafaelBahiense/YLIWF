#pragma once
#include "Scenario.h"
#include "../fixtures/Archetypes.h"
#include "../testing/Definition.h"
#include "../testing/Preparation.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace mod::combat_tests::healer {
    inline float InjuryTarget(float maximumHealth, float fraction = .45f) {
        return std::isfinite(maximumHealth) && maximumHealth > 1 && std::isfinite(fraction) && fraction >= 0 &&
                       fraction <= 1
                   ? std::max(1.0f, maximumHealth * fraction)
                   : 0;
    }

    inline const testing::Suite Definition = [] {
        using namespace testing;
        Suite suite{"Healer"};
        suite.Prepare(testing::PrepareArena);
        TestCase pressure{0, "Party pressure",
                          "Heal the injured player while a sword opponent pressures the healer. Five injury waves over "
                          "60 active seconds."};
        pressure.Prepare([](Context& context) { return context.PrepareSubject(); });
        pressure.Actor(fixtures::Spawner("opponent", fixtures::Archetype::Sword).Target("subject").At(-512, 1280));
        for (const auto* name : {"First injury", "Second injury", "Third injury", "Fourth injury", "Fifth injury"})
            pressure.Phase(Phase{name, 12}.OnEnter(Action::InjurePlayer, .45f));
        suite.Case(std::move(pressure));
        return suite;
    }();

    inline constexpr scenarios::Entry Navigation{1,
                                                 1,
                                                 "Healer",
                                                 "Party pressure",
                                                 "Heal the injured player while a sword opponent pressures the healer. "
                                                 "Five injury waves over 60 active seconds.",
                                                 "Needs a known ally-healing spell and enough magicka to begin.",
                                                 &Definition,
                                                 0,
                                                 1,
                                                 0,
                                                 scenarios::Execution::Healer};
}
