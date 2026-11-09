#include <doctest/doctest.h>
#include "arena/ObserverRules.h"
#include <array>

TEST_CASE("Arena opponents cannot select the observer; combat participants remain eligible") {
    const std::array opponents{10u, 11u, 12u};
    using mod::combat_tests::observer_rules::Reject;
    CHECK(Reject(10, 1, 1, opponents));
    CHECK(Reject(11, 1, 1, opponents));
    CHECK(Reject(12, 1, 1, opponents));
    CHECK_FALSE(Reject(10, 2, 1, opponents));
    CHECK_FALSE(Reject(10, 3, 1, opponents));
    CHECK_FALSE(Reject(20, 1, 1, opponents));
    CHECK_FALSE(Reject(0, 1, 1, opponents));
    CHECK_FALSE(Reject(10, 0, 0, opponents));
    CHECK_FALSE(Reject(10, 1, 1, {}));
}

TEST_CASE("Arena team isolation rejects friendly targets while retaining hostile and unrelated combat") {
    const std::array opponents{10u, 11u};
    const std::array allies{20u, 21u, 22u};
    using mod::combat_tests::observer_rules::Reject;
    CHECK(Reject(20, 21, 1, opponents, allies));
    CHECK(Reject(21, 20, 1, opponents, allies));
    CHECK(Reject(22, 21, 1, opponents, allies));
    CHECK(Reject(20, 1, 1, opponents, allies));
    CHECK_FALSE(Reject(20, 10, 1, opponents, allies));
    CHECK_FALSE(Reject(10, 21, 1, opponents, allies));
    CHECK_FALSE(Reject(99, 21, 1, opponents, allies));
    CHECK_FALSE(Reject(20, 0, 1, opponents, allies));
    CHECK(Reject(10, 11, 1, opponents, allies));
}
