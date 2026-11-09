#include <doctest/doctest.h>
#include "arena/SceneControlRules.h"

TEST_CASE("Arena setup holds its subject and combat yields to role movement") {
    mod::combat_tests::scene_rules::Scope scope;
    CHECK(scope.Preferred(90) == 0);
    REQUIRE(scope.Begin(1, 2, 10, 11));
    CHECK(scope.Includes(1));
    CHECK_FALSE(scope.Includes(3));
    CHECK(scope.Preferred(90) == 10);
    REQUIRE(scope.StartCombat(1, 2));
    CHECK(scope.Preferred(90) == 90);
    CHECK(scope.Preferred(0) == 11);
    REQUIRE(scope.End(1, 2));
    CHECK(scope.Preferred(90) == 0);
}

TEST_CASE("Stale arena requests cannot replace or release another scope") {
    mod::combat_tests::scene_rules::Scope scope;
    CHECK_FALSE(scope.Begin(0, 2, 10, 11));
    CHECK_FALSE(scope.Begin(1, 0, 10, 11));
    CHECK_FALSE(scope.Begin(1, 2, 10, 10));
    REQUIRE(scope.Begin(1, 2, 10, 11));
    CHECK_FALSE(scope.Begin(3, 4, 10, 11));
    CHECK_FALSE(scope.StartCombat(1, 3));
    CHECK_FALSE(scope.StartCombat(3, 2));
    CHECK_FALSE(scope.End(1, 3));
    CHECK_FALSE(scope.End(3, 2));
    CHECK(scope.Preferred(90) == 10);
    REQUIRE(scope.End(1, 2));
    REQUIRE(scope.Begin(1, 3, 10, 11));
    CHECK_FALSE(scope.End(1, 2));
    CHECK(scope.Includes(1));
}
