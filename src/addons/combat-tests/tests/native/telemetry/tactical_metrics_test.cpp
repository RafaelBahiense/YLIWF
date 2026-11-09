#include <doctest/doctest.h>
#include "telemetry/TacticalMetrics.h"

using namespace mod::combat_tests;

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
