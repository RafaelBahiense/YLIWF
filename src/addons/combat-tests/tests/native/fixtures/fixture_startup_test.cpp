#include <doctest/doctest.h>
#include "fixtures/FixtureStartupRules.h"
#include <limits>

using namespace mod::combat_tests;

TEST_CASE("Fixture setup waits for minimum loading time and stable equipment and resources") {
    fixture_startup::Settling settle;
    resources::Stats primary{2000, 166, 2000, 0, 1.7f, 1.05f, 0, 100, 100};
    auto result = fixture_startup::Result::Waiting;
    for (int i = 0; i < 15; ++i) {
        result = settle.Observe(.25f, primary, {}, 1, 4);
        CHECK(result == fixture_startup::Result::Waiting);
    }
    CHECK(settle.Observe(.25f, primary, {}, 2, 4) == fixture_startup::Result::Waiting);
    for (int i = 0; i < 3; ++i)
        CHECK(settle.Observe(.25f, primary, {}, 2, 4) == fixture_startup::Result::Waiting);
    CHECK(settle.Observe(.25f, primary, {}, 2, 4) == fixture_startup::Result::Ready);
    primary[0] += 30;
    CHECK(settle.Observe(.25f, primary, {}, 2, 4) == fixture_startup::Result::Waiting);
}

TEST_CASE("Post-normalization verification starts a new quiet window and unstable fixtures expire") {
    fixture_startup::Settling settle;
    resources::Stats primary{2000, 166, 2000};
    CHECK(settle.Observe(.25f, primary, {}, 1, 1) == fixture_startup::Result::Waiting);
    for (int i = 0; i < 4; ++i)
        settle.Observe(.25f, primary, {}, 1, 1);
    CHECK(settle.Observe(.25f, primary, {}, 1, 1) == fixture_startup::Result::Ready);
    settle = {};
    CHECK(settle.Observe(.25f, primary, {}, 1, 1) == fixture_startup::Result::Waiting);
    settle = {};
    auto result = fixture_startup::Result::Waiting;
    for (int i = 0; i < 80; ++i)
        result = settle.Observe(.25f, primary, {}, static_cast<std::uint64_t>(i), 4);
    CHECK(result == fixture_startup::Result::Expired);
    settle = {};
    primary[0] = std::numeric_limits<float>::quiet_NaN();
    for (int i = 0; i < 80; ++i)
        result = settle.Observe(.25f, primary, {}, 1, 4);
    CHECK(result == fixture_startup::Result::Expired);
}

TEST_CASE("Health protection evidence follows damage rather than changing resource maxima") {
    CHECK(resources::Undamaged(0, 0));
    CHECK(resources::Undamaged(-5, -2));
    CHECK_FALSE(resources::Undamaged(0, -30));
    CHECK_FALSE(resources::Undamaged(0, std::numeric_limits<float>::quiet_NaN()));
}
