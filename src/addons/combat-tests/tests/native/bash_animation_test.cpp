#include <doctest/doctest.h>
#include "telemetry/BashRules.h"

using mod::combat_tests::telemetry::bash_rules::Classify;
using Event = yliwf::testing::BashAnimationEvent;

TEST_CASE("Bash animation evidence recognizes actual starts and specific ends") {
    CHECK(Classify("SBF_BashStart") == Event::Started);
    CHECK(Classify("SBF_BashStop") == Event::Ended);
    CHECK(Classify("bashStop") == Event::Ended);
    CHECK(Classify("bashExit") == Event::Ended);
}

TEST_CASE("Generic attack cleanup and outgoing bash requests do not prove bash execution or completion") {
    CHECK_FALSE(Classify("attackStop"));
    CHECK_FALSE(Classify("blockStop"));
    CHECK_FALSE(Classify("bashPowerStart"));
    CHECK_FALSE(Classify("bashStart"));
    CHECK_FALSE(Classify("HitFrame"));
    CHECK_FALSE(Classify("Collision_RecoilCancelable"));
}

TEST_CASE("Dodge and stagger starts mark interruption rather than bash completion") {
    CHECK(Classify("TKDR_DodgeStart") == Event::Interrupted);
    CHECK(Classify("SBF_StaggerStart") == Event::Interrupted);
    CHECK(Classify("SBF_RecoilStart") == Event::Interrupted);
    CHECK_FALSE(Classify("TKDR_IframeEnd"));
    CHECK_FALSE(Classify("SBF_StaggerStop"));
}
