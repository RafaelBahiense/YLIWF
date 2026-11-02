#include <doctest/doctest.h>
#include "combat/DefensiveBashRules.h"
#include <limits>

#include "combat/actions/BashRules.h"

using namespace mod::combat_actions::bash_rules;
using namespace mod::defensive_bash_rules;

TEST_CASE("Native bash requires an owned guard, a close frontal windup and a stamina reserve") {
    Opportunity value{true, true, true, true, false, false, false, true, 100, 0, 150, 390, 0};
    CHECK(CanRequest(value));
    value.stamina = 100;
    CHECK_FALSE(CanRequest(value));
    value.stamina = 150;
    value.ownsGuard = false;
    CHECK_FALSE(CanRequest(value));
    value.ownsGuard = true;
    value.interrupted = true;
    CHECK_FALSE(CanRequest(value));
    value.interrupted = false;
    value.reacting = true;
    CHECK_FALSE(CanRequest(value));
    value.reacting = false;
    value.targetWindingUp = false;
    CHECK_FALSE(CanRequest(value));
    value.targetWindingUp = true;
    value.distance = 145;
    CHECK_FALSE(CanRequest(value));
    value.distance = 100;
    value.heading = 61;
    CHECK_FALSE(CanRequest(value));
    value.heading = 0;
    value.cooldown = 1;
    CHECK_FALSE(CanRequest(value));
    value.cooldown = 0;
    value.stamina = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(CanRequest(value));
    CHECK(AllowedEvent("bashStart", Kind::Normal));
    CHECK_FALSE(AllowedEvent("bashPowerStart", Kind::Normal));
    CHECK_FALSE(AllowedEvent("blockStart", Kind::Normal));
    CHECK_FALSE(AllowedEvent("attackStart", Kind::Normal));
    CHECK_FALSE(AllowedEvent("attackPowerStartInPlace", Kind::Normal));
}

TEST_CASE("Power bash keeps a larger budget and accepts only the power bash idle") {
    Opportunity value{true, true, true, true, false, false, false, true, 100, 0, 130, 390, 0};
    CHECK(CanRequest(value));
    value.kind = Kind::Power;
    CHECK_FALSE(CanRequest(value));
    value.stamina = 150;
    CHECK(CanRequest(value));
    CHECK(AllowedEvent("bashPowerStart", Kind::Power));
    CHECK_FALSE(AllowedEvent("bashStart", Kind::Power));
    CHECK_FALSE(AllowedEvent("attackPowerStartInPlace", Kind::Power));
    CHECK_FALSE(AllowedEvent("bashPowerStart", Kind::Off));
}
