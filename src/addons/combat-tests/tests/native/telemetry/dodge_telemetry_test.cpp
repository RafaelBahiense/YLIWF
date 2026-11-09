#include <doctest/doctest.h>
#include "telemetry/DodgeRules.h"
#include "telemetry/NotificationRules.h"

TEST_CASE("Dodge telemetry separates active state from style and unavailable variables") {
    using namespace mod::combat_tests::telemetry::rules;
    CHECK(Active(1, 1));
    CHECK_FALSE(Active(0, 1));
    CHECK_FALSE(Active(15, 14));
    CHECK_FALSE(Active(2, 2));
    CHECK_FALSE(Active(15, 0));
}

TEST_CASE("Dodge telemetry recognizes graph naming variants without counting unrelated tags") {
    using namespace mod::combat_tests::telemetry::rules;
    CHECK(DodgeEvent("TKDodgeForward"));
    CHECK(DodgeEvent("TKDR_DodgeStart"));
    CHECK(DodgeEvent("DMCO_End"));
    CHECK(DodgeEvent("DODGESTOP"));
    CHECK(GuardEvent("blockStop"));
    CHECK(GuardEvent("bashStart"));
    CHECK(Relevant("weaponSwing"));
    CHECK(Relevant("preHitFrame"));
    CHECK(Relevant("staggerStart"));
    CHECK(Relevant("MCO_ComboStart"));
    CHECK_FALSE(DodgeEvent("blockStart"));
    CHECK_FALSE(Relevant("FootLeft"));
    CHECK_FALSE(Relevant(""));
}

TEST_CASE("Notification results cannot overwrite a reused trace slot or cross a reset") {
    using namespace mod::combat_tests::telemetry::notification_rules;
    const auto first = Ticket(1, 0);
    const auto wrapped = Ticket(1025, 1024);
    CHECK(Slot(first) == Slot(wrapped));
    CHECK(first != wrapped);
    CHECK(Current(first, 0));
    CHECK_FALSE(Current(0, 0));
    CHECK_FALSE(Current(first, 1));
    CHECK_FALSE(Current(first, 1024));
    CHECK(Current(wrapped, 1024));
    CHECK(Slot(Ticket(12, 1023)) == 1023);
}
