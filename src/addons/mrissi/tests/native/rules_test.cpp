#include <doctest/doctest.h>
#include "Rules.h"
#include <limits>
#include <initializer_list>

namespace {
    using namespace yliwf::sdk;
    using mod::mrissi_rules::Describe;
    using mod::mrissi_rules::Snapshot;

    Snapshot Following() {
        return {.initialized = true, .recruited = true, .teammate = true, .ordinaryPackage = true};
    }

    TEST_CASE("M'rissi commands use recruitment alias and authored waiting state") {
        auto snapshot = Following();
        CHECK(Describe(snapshot).state == State::Following);
        CHECK(Describe(snapshot).commands == AllCommands);
        snapshot.waiting = 1;
        const auto waiting = Describe(snapshot);
        CHECK(waiting.state == State::Waiting);
        CHECK(Supports(waiting, Command::Follow));
        CHECK(Supports(waiting, Command::Wait));
        CHECK_FALSE(Supports(waiting, Command::Dismiss));
    }

    TEST_CASE("M'rissi dismissal verifies alias release and respects pre-recruitment state") {
        Snapshot snapshot{.initialized = true};
        CHECK(Describe(snapshot).state == State::Inactive);
        snapshot.dismissed = true;
        CHECK(Reached(Describe(snapshot), Command::Dismiss));
        snapshot.dead = true;
        CHECK(Describe(snapshot).state == State::Inactive);
        CHECK(Describe({}).state == State::Unavailable);
    }

    TEST_CASE("M'rissi mismatched registrations and invalid waiting values fail closed") {
        auto snapshot = Following();
        snapshot.recruited = false;
        CHECK(Describe(snapshot).state == State::Unavailable);
        snapshot = Following();
        snapshot.teammate = false;
        CHECK(Describe(snapshot).state == State::Unavailable);
        snapshot = Following();
        snapshot.dismissed = true;
        CHECK(Describe(snapshot).state == State::Unavailable);
        snapshot = Following();
        for (const float waiting : {-1.0f, 0.5f, 2.0f, std::numeric_limits<float>::quiet_NaN()}) {
            snapshot.waiting = waiting;
            CHECK(Describe(snapshot).state == State::Unavailable);
            CHECK(Describe(snapshot).commands == 0);
        }
    }

    TEST_CASE("M'rissi yields to quests, broken behavior, scenes and mount transitions") {
        for (const auto field : {&Snapshot::questLocked, &Snapshot::broken, &Snapshot::scene, &Snapshot::incapacitated,
                                 &Snapshot::mounting, &Snapshot::pending}) {
            auto snapshot = Following();
            snapshot.*field = true;
            const auto state = Describe(snapshot);
            CHECK(state.state == State::Following);
            CHECK(state.commands == 0);
            CHECK(state.reason[0] != '\0');
        }
        auto snapshot = Following();
        snapshot.ordinaryPackage = false;
        CHECK(Describe(snapshot).commands == 0);
    }

    TEST_CASE("M'rissi restricted locations allow resuming but prevent waiting and dismissal") {
        auto snapshot = Following();
        snapshot.restrictedLocation = true;
        for (const float waiting : {0.0f, 1.0f}) {
            snapshot.waiting = waiting;
            const auto state = Describe(snapshot);
            CHECK(Supports(state, Command::Follow));
            CHECK_FALSE(Supports(state, Command::Wait));
            CHECK_FALSE(Supports(state, Command::Dismiss));
            CHECK(state.reason[0] != '\0');
        }
    }

}
