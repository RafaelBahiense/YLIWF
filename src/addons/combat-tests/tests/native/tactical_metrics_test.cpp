#include <doctest/doctest.h>
#include "TacticalMetrics.h"

using namespace mod::combat_tests;

TEST_CASE("Ranged scenario verdict needs useful movement and actual threat coverage") {
    tactics::Metrics metrics;
    CHECK(tactics::Missing(suite::Fixture::Archer, metrics));
    metrics.Observe(.1f, 1, 3, 1, 1200, 0, 0);
    CHECK(tactics::Missing(suite::Fixture::Archer, metrics));
    metrics.Observe(.1f, 1, 3, 1, 180, 100, 0);
    CHECK_FALSE(tactics::Missing(suite::Fixture::Archer, metrics));
    CHECK(tactics::Missing(suite::Fixture::Projectile, metrics));
    metrics.Observe(.1f, 6, 4, 1, 180, 200, 0);
    metrics.casts = 1;
    CHECK_FALSE(tactics::Missing(suite::Fixture::Projectile, metrics));
}

TEST_CASE("Earlier caster coverage cannot mask terminal inactivity or fixture friendly fire") {
    tactics::Metrics metrics;
    metrics.casts = 1;
    metrics.Observe(.1f, 6, 5, 1, 200, 0, 0);
    metrics.Observe(.1f, 6, 5, 1, 180, 100, 0);
    CHECK_FALSE(tactics::Missing(suite::Fixture::Concentration, metrics));
    for (int i = 0; i < 110; ++i)
        metrics.Observe(.1f, 6, 5, 1, 180, 100, 0);
    CHECK(tactics::Missing(suite::Fixture::Concentration, metrics));
    metrics.Contacts(1, 0);
    metrics.Observe(.1f, 6, 5, 1, 180, 100, 0);
    CHECK_FALSE(tactics::Missing(suite::Fixture::Concentration, metrics));
    metrics.friendlyHits = 1;
    CHECK(tactics::Missing(suite::Fixture::Concentration, metrics));
    metrics.friendlyHits = 0;
    metrics.casts = 0;
    CHECK(tactics::Missing(suite::Fixture::Concentration, metrics));
}

TEST_CASE("Heavy and multiple attacker coverage cannot pass from elapsed time alone") {
    tactics::Metrics metrics;
    metrics.Observe(.1f, 6, 2, 1, 100, 0, 0);
    CHECK(tactics::Missing(suite::Fixture::Heavy, metrics));
    metrics.powerWindups = 1;
    CHECK_FALSE(tactics::Missing(suite::Fixture::Heavy, metrics));
    CHECK(tactics::Missing(suite::Fixture::Multiple, metrics));
    metrics.Observe(.1f, 6, 1, 2, 100, 10, 0);
    CHECK_FALSE(tactics::Missing(suite::Fixture::Multiple, metrics));
}

TEST_CASE("Teleport jumps and invalid samples never become tactical displacement") {
    tactics::Metrics metrics;
    metrics.Observe(.1f, 6, 4, 1, 100, 0, 0);
    metrics.Observe(.1f, 6, 4, 1, 100, 5000, 0);
    CHECK(metrics.moved == 0);
    metrics.Observe(-1, 6, 4, 1, 1, 5010, 0);
    CHECK(metrics.closest == 100);
    metrics.Observe(.1f, 6, 4, 1, 50, 5010, 0);
    CHECK(metrics.moved == 10);
}

TEST_CASE("Additional fixture references retain independent recovery ownership") {
    CHECK(suite::AdditionalActor(suite::Fixture::AllyPressure));
    CHECK(suite::AdditionalActor(suite::Fixture::Multiple));
    CHECK_FALSE(suite::AdditionalActor(suite::Fixture::Archer));
    CHECK(suite::Equipment(suite::Fixture::Archer) == suite::Loadout::Bow);
    CHECK(suite::Equipment(suite::Fixture::Concentration) == suite::Loadout::Concentration);
}
