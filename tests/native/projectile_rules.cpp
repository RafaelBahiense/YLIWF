#include <doctest/doctest.h>
#include "combat/ProjectileRules.h"
#include "combat/actions/EvadeRules.h"
#include <limits>

TEST_CASE("Projectile closest approach distinguishes incoming shots, misses and receding arrows") {
    using namespace mod::projectile_rules;
    auto shot = Observe({0, 600, 0}, {0, -1200, 0});
    CHECK(shot.readable);
    CHECK(shot.incoming);
    CHECK(shot.seconds == doctest::Approx(.5f));
    CHECK_FALSE(Observe({0, 600, 0}, {0, 1200, 0}).incoming);
    CHECK_FALSE(Observe({300, 600, 0}, {0, -1200, 0}).incoming);
    CHECK_FALSE(Observe({0, 600, 400}, {0, -1200, 0}).incoming);
    CHECK_FALSE(Observe({0, 6000, 0}, {0, -1200, 0}).incoming);
    CHECK_FALSE(Observe({0, 600, 0}, {0, std::numeric_limits<float>::quiet_NaN(), 0}).readable);
}

TEST_CASE("Projectile gaps need continuous readable quiet samples and reset on uncertainty") {
    mod::projectile_rules::QuietWindow window;
    CHECK_FALSE(window.Update(true, false, .1f));
    CHECK_FALSE(window.Update(true, false, .1f));
    CHECK(window.Update(true, false, .1f));
    CHECK_FALSE(window.Update(true, true, .1f));
    CHECK_FALSE(window.Update(true, false, .1f));
    CHECK_FALSE(window.Update(false, false, .1f));
    CHECK_FALSE(window.Update(true, false, .1f));
    CHECK_FALSE(window.Update(true, false, 1));
}

TEST_CASE("Directed evasion needs a reserve and actual graph acknowledgement before completion") {
    using namespace mod::combat_actions::evade_rules;
    CHECK(Affordable(110, 300, 50, false));
    CHECK_FALSE(Affordable(109, 300, 50, false));
    CHECK_FALSE(Affordable(110, 300, 50, true));
    CHECK_FALSE(Affordable(110, 300, -1, false));
    Execution action;
    action.Request();
    action.Observe(true, false, .1f);
    CHECK(action.phase == Phase::Pending);
    action.Observe(true, true, .1f);
    CHECK(action.phase == Phase::Active);
    action.Observe(true, false, .1f);
    CHECK(action.phase == Phase::Idle);
    CHECK(action.Started());
    action.Request();
    action.Observe(true, false, .7f);
    CHECK(action.phase == Phase::Disabled);
    CHECK_FALSE(action.Started());
}

TEST_CASE("Unending or unreadable dodge state disables the capability without forging completion") {
    using namespace mod::combat_actions::evade_rules;
    Execution action;
    action.Request();
    action.Observe(true, true, .1f);
    action.Observe(false, false, 2);
    CHECK(action.phase == Phase::Disabled);
}

TEST_CASE("Directed dodge success excludes movement in the wrong direction and teleports") {
    using namespace mod::combat_actions::evade_rules;
    CHECK(UsefulDisplacement(-100, 0, -384, 0));
    CHECK_FALSE(UsefulDisplacement(100, 0, -384, 0));
    CHECK_FALSE(UsefulDisplacement(0, 100, -384, 0));
    CHECK_FALSE(UsefulDisplacement(-20, 0, -384, 0));
    CHECK_FALSE(UsefulDisplacement(-1000, 0, -384, 0));
}
