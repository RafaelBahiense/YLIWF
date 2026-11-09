#include <doctest/doctest.h>
#include "telemetry/EngagementMetrics.h"
#include "telemetry/ChallengeComparison.h"
#include <limits>

TEST_CASE("Closing damage, first reach and first contact remain separate observations") {
    mod::combat_tests::engagement::Metrics metrics;
    metrics.Observe(1, .1f, 1000, 12, false, 1, 0, 1000, 0, 0, 0, false);
    CHECK(metrics.firstReach == -1);
    CHECK(metrics.firstContact == -1);
    CHECK(metrics.closingDamage == 12);
    metrics.Observe(2, .1f, 250, 5, false, 1, 0, 250, 0, 0, 0, false);
    CHECK(metrics.firstReach == 2);
    CHECK(metrics.firstContact == -1);
    CHECK(metrics.closingDamage == 12);
    metrics.Observe(3, .1f, 200, 0, true, 2, 0, 200, 0, 0, 0, false);
    CHECK(metrics.firstContact == 3);
    CHECK(metrics.targetSwitches == 1);
}

TEST_CASE("Casting-line exits require continuous displacement during the same observed episode") {
    mod::combat_tests::engagement::Metrics metrics;
    metrics.Observe(1, .1f, 500, 0, false, 1, 0, 500, 0, 0, 0, true);
    metrics.Observe(2, .1f, 500, 0, false, 1, 120, 500, 0, 0, 0, true);
    CHECK(metrics.aimLineExits == 1);
    CHECK(metrics.aimLineExposure == doctest::Approx(.1f));
    metrics.Observe(3, .1f, 500, 0, false, 1, 0, 500, 0, 0, 0, true);
    metrics.Observe(4, .1f, 500, 0, false, 1, 1000, 500, 0, 0, 0, true);
    CHECK(metrics.aimLineExits == 1);
    metrics.Observe(5, .1f, 500, 0, false, 1, 1000, 500, 0, 0, 0, false);
    metrics.Observe(6, .1f, 500, 0, false, 1, 1100, 500, 0, 0, 0, true);
    CHECK(metrics.aimLineExits == 1);
    CHECK(metrics.castingPressure == doctest::Approx(.5f));
    const auto before = metrics.castingPressure;
    metrics.Observe(7, 5, 500, 0, false, 1, 0, 500, 0, 0, 0, true);
    metrics.Observe(7, .1f, std::numeric_limits<float>::quiet_NaN(), 0, false, 1, 0, 500, 0, 0, 0, true);
    CHECK(metrics.castingPressure == before);
    metrics.Observe(8, .1f, 500, 0, false, 1, 0, 500, 0, 0, 0, true);
    metrics.Observe(9, 5, 500, 0, false, 1, 0, 500, 0, 0, 0, true);
    metrics.Observe(10, .1f, 500, 0, false, 1, 120, 500, 0, 0, 0, true);
    CHECK(metrics.aimLineExits == 1);
}

TEST_CASE("Comparison rejects different starting resources or equipment rather than averaging unmatched pairs") {
    using namespace mod::combat_tests::comparison;
    Setup on{1, 2, 3, 630, 300, 100, 472.5f, 2000};
    CHECK(Matched(on, on));
    auto off = on;
    off.allyHealth = 512.5f;
    CHECK_FALSE(Matched(on, off));
    off = on;
    off.stamina -= 20;
    CHECK_FALSE(Matched(on, off));
    off = on;
    off.weapon = 4;
    CHECK_FALSE(Matched(on, off));
    off = on;
    off.opponentBase = 4;
    CHECK_FALSE(Matched(on, off));
    off = on;
    off.health = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(Matched(on, off));
    CHECK_FALSE(Matched({}, {}));
    off = on;
    off.allyStats[0] = 20;
    CHECK_FALSE(Matched(on, off));
    off = on;
    off.enemyStats[8] = 25;
    CHECK_FALSE(Matched(on, off));
    off = on;
    off.enemyEquipment = 123;
    CHECK_FALSE(Matched(on, off));
}

TEST_CASE("Fixture drift distinguishes current resource use from maximum and regeneration changes") {
    using namespace mod::combat_tests::resources;
    const Stats baseline{2000, 300, 2000, .7f, 5, 3, 100, 100, 100};
    CHECK(Stable(baseline, baseline));
    auto changed = baseline;
    changed[0] += 20;
    CHECK_FALSE(Stable(baseline, changed));
    changed = baseline;
    changed[5] += 1;
    CHECK_FALSE(Stable(baseline, changed));
    changed = baseline;
    changed[8] = std::numeric_limits<float>::infinity();
    CHECK_FALSE(Stable(baseline, changed));
    CHECK(AtTarget(2000, 2000));
    CHECK_FALSE(AtTarget(2030, 2000));
    CHECK_FALSE(AtTarget(2000, 0));
}
