#include <doctest/doctest.h>
#include "followers/PackageControlRules.h"
#include <array>
#include <cstdint>
#include <vector>

using namespace mod::package_control_rules;

TEST_CASE("Package priority is scoped to live registered debug followers") {
    const State ready{true, true, true, true, false, true};
    CHECK(Decide(ready) == Decision::Override);
    auto state = ready;
    state.debug = false;
    CHECK(Decide(state) == Decision::Release);
    state = ready;
    state.registered = false;
    CHECK(Decide(state) == Decision::Release);
    state = ready;
    state.alive = false;
    CHECK(Decide(state) == Decision::Release);
}

TEST_CASE("Package overrides yield to scenes, quest policy and unloaded actors") {
    const State ready{true, true, true, true, false, true};
    auto state = ready;
    state.scene = true;
    CHECK(Decide(state) == Decision::Suspend);
    state = ready;
    state.questAllows = false;
    CHECK(Decide(state) == Decision::Suspend);
    state = ready;
    state.loaded = false;
    CHECK(Decide(state) == Decision::Suspend);
    state = ready;
    state.commandPending = true;
    CHECK(Decide(state) == Decision::Suspend);
    CHECK(Decide(ready) == Decision::Override);
}

TEST_CASE("Restoring a displaced interrupt never replaces a newer package or scene") {
    CHECK(RestoreInterrupt(true, false, false, true));
    CHECK_FALSE(RestoreInterrupt(false, false, false, true));
    CHECK_FALSE(RestoreInterrupt(true, true, false, true));
    CHECK_FALSE(RestoreInterrupt(true, false, true, true));
    CHECK_FALSE(RestoreInterrupt(true, false, false, false));
}

TEST_CASE("Forced selection preserves alias package priority and stops evaluating after a match") {
    const std::array<std::uint32_t, 4> packages{10, 20, 30, 40};
    std::vector<std::uint32_t> evaluated;
    CHECK(Select(packages, [&evaluated](auto package) {
              evaluated.push_back(package);
              return package == 20 || package == 40;
          }) == 20);
    CHECK(evaluated == std::vector<std::uint32_t>{10, 20});
    CHECK(Select(packages, [](auto) { return false; }) == 0);
    CHECK(Select(std::array<std::uint32_t, 0>{}, [](auto) { return true; }) == 0);
}
