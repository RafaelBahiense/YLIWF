#include <doctest/doctest.h>
#include "fixtures/FixtureProtectionRules.h"
#include <limits>
using namespace mod::combat_tests::fixture_protection_rules;

TEST_CASE("Fixture damage protection only covers the explicit opponent references") {
    CHECK(Scoped(10, 10, 20));
    CHECK(Scoped(20, 10, 20));
    CHECK_FALSE(Scoped(30, 10, 20));
    CHECK_FALSE(Scoped(0, 10, 20));
    CHECK_FALSE(Scoped(10, 0, 0));
    CHECK_FALSE(Scoped(20, 10, 0));
    CHECK(Suppress(true, true, true, -1000000));
    CHECK_FALSE(Suppress(false, true, true, -1000000));
}

TEST_CASE("Fixture damage protection preserves healing and does not sanitize invalid engine inputs") {
    CHECK_FALSE(Suppress(true, true, true, 100));
    CHECK_FALSE(Suppress(true, true, true, 0));
    CHECK_FALSE(Suppress(true, true, false, -100));
    CHECK_FALSE(Suppress(true, false, true, -100));
    CHECK_FALSE(Suppress(true, true, true, std::numeric_limits<float>::quiet_NaN()));
    CHECK_FALSE(Suppress(true, true, true, std::numeric_limits<float>::infinity()));
}
