#include <doctest/doctest.h>
#include "runtime/Rules.h"
#include <array>

TEST_CASE("Fixture recovery keeps the additional actor and rejects aliased ownership") {
    using namespace mod::test_rules;
    Plan plan{.subject = 1, .enemy = 2, .enemyBase = 3, .origin = 4, .health = 100, .generation = 1};
    plan.fixture = 11;
    plan.auxiliary = 5;
    CHECK(Valid(plan));
    std::array<std::uint8_t, sizeof(Plan)> bytes{};
    std::memcpy(bytes.data(), &plan, sizeof(plan));
    const auto decoded = DecodePlan(5, bytes);
    REQUIRE(decoded);
    CHECK(decoded->auxiliary == 5);
    CHECK(decoded->fixture == 11);
    const auto legacy = DecodePlan(4, std::span(bytes).first(76));
    REQUIRE(legacy);
    CHECK(legacy->auxiliary == 0);
    CHECK(legacy->fixture == 0);
    for (const auto ref : {1u, 2u, 3u, 4u}) {
        plan.auxiliary = ref;
        CHECK_FALSE(Valid(plan));
    }
    plan.auxiliary = 5;
    plan.fixture = 7;
    CHECK_FALSE(Valid(plan));
}

TEST_CASE("Fixture-owned hits are distinguished from external interference") {
    using namespace mod::test_rules;
    CHECK(ClassifyFixtureHit(1, 2, 3, true, 3, 2) == HitKind::AllyIncoming);
    CHECK(ClassifyFixtureHit(1, 2, 3, true, 2, 3) == HitKind::AllyOutgoing);
    CHECK(ClassifyFixtureHit(1, 2, 3, true, 1, 3) == HitKind::Friendly);
    CHECK(ClassifyFixtureHit(1, 2, 3, true, 3, 1) == HitKind::Friendly);
    CHECK(ClassifyFixtureHit(1, 2, 3, false, 1, 3) == HitKind::Incoming);
    CHECK(ClassifyFixtureHit(1, 2, 3, false, 3, 1) == HitKind::Outgoing);
    CHECK(ClassifyFixtureHit(1, 2, 3, false, 2, 3) == HitKind::Crossfire);
    CHECK(ClassifyFixtureHit(1, 2, 3, true, 1, 4) == HitKind::Foreign);
    CHECK(ClassifyFixtureHit(1, 2, 3, false, 2, 4) == HitKind::Foreign);
}
