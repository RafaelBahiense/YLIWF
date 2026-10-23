#include <doctest/doctest.h>
#include "Rules.h"
#include <limits>

namespace {
    using namespace yliwf::sdk;
    using mod::inigo_rules::Describe;
    using mod::inigo_rules::Snapshot;

    Snapshot Following() {
        return {.initialized = true, .teammate = true, .waiting = 0.0f, .ordinaryPackage = true};
    }

    TEST_CASE("Inigo recognizes following, waiting and relaxing") {
        auto snapshot = Following();
        auto state = Describe(snapshot);
        CHECK(state.state == State::Following);
        CHECK(state.commands == AllCommands);
        for (float waiting : {1.0f, 2.0f}) {
            snapshot.waiting = waiting;
            state = Describe(snapshot);
            CHECK(state.state == State::Waiting);
            CHECK(Supports(state, Command::Follow));
            CHECK(Supports(state, Command::Dismiss));
        }
    }

    TEST_CASE("Inigo dismissal keeps teammate status without remaining active") {
        auto snapshot = Following();
        snapshot.waiting = -1.0f;
        const auto state = Describe(snapshot);
        CHECK(state.state == State::Inactive);
        CHECK(state.commands == 0);
        CHECK(Reached(state, Command::Dismiss));
    }

    TEST_CASE("Inigo commands preserve quest and scene ownership") {
        for (const auto field : {&Snapshot::questLocked, &Snapshot::scene, &Snapshot::incapacitated}) {
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
        snapshot.waiting = 1.0f;
        CHECK(Describe(snapshot).state == State::Waiting);
        CHECK_FALSE(Supports(Describe(snapshot), Command::Follow));
    }

    TEST_CASE("Inigo mismatched and unknown actor values never authorize repair") {
        auto snapshot = Following();
        snapshot.teammate = false;
        CHECK(Describe(snapshot).state == State::Unavailable);
        CHECK(Describe(snapshot).commands == 0);
        snapshot.teammate = true;
        for (float waiting : {-2.0f, 0.5f, 3.0f, std::numeric_limits<float>::quiet_NaN()}) {
            snapshot.waiting = waiting;
            CHECK(Describe(snapshot).state == State::Unavailable);
            CHECK(Describe(snapshot).commands == 0);
        }
    }

    TEST_CASE("Inigo cannot be recruited by an external command") {
        auto snapshot = Following();
        snapshot.initialized = false;
        CHECK(Describe(snapshot).state == State::Inactive);
        CHECK(Describe(snapshot).commands == 0);
        snapshot = Following();
        snapshot.dead = true;
        CHECK(Describe(snapshot).state == State::Inactive);
        CHECK(Describe(snapshot).commands == 0);
    }

    TEST_CASE("Inigo keeps a native distance preference for owner-controlled movement") {
        using mod::inigo_rules::DistanceRank;
        CHECK(DistanceRank(FollowDistance::Close) == 0);
        CHECK(DistanceRank(FollowDistance::Normal) == 0);
        CHECK(DistanceRank(FollowDistance::Far) == 1);
        CHECK_FALSE(DistanceRank(static_cast<FollowDistance>(3)));
    }

}
