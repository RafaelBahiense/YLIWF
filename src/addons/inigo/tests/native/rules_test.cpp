#include <doctest/doctest.h>
#include "Rules.h"
#include "MovementRules.h"
#include <limits>

namespace {
    using namespace yliwf::sdk;
    using mod::inigo_rules::Describe;
    using mod::inigo_rules::Snapshot;
    using MovementSnapshot = mod::inigo_movement_rules::Snapshot;

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

    mod::inigo_movement_rules::Snapshot Walking() {
        return {.following = true,
                .controllerAllows = true,
                .loaded = true,
                .ordinaryOnFoot = true,
                .ordinaryPassive = true,
                .normalMovement = true};
    }

    TEST_CASE("Inigo formation release returns to the selected distance") {
        using namespace mod::inigo_movement_rules;
        const auto walking = Walking();
        CHECK(Select(walking, true, false) == Selection::Distance);
        CHECK(Select(walking, true, true) == Selection::Formation);
        CHECK(Select(walking, true, false) == Selection::Distance);
        CHECK(Select(walking, false, false) == Selection::Owner);
    }

    TEST_CASE("Inigo movement yields to combat scenes horses and foreign interrupts") {
        using namespace mod::inigo_movement_rules;
        for (const auto field : {&MovementSnapshot::pending, &MovementSnapshot::combat, &MovementSnapshot::scene,
                                 &MovementSnapshot::ridingRequested, &MovementSnapshot::foreignInterrupt}) {
            auto walking = Walking();
            walking.*field = true;
            CHECK(Select(walking, true, false) == Selection::Owner);
            CHECK(Select(walking, true, true) == Selection::Owner);
            walking.*field = false;
            CHECK(Select(walking, true, false) == Selection::Distance);
        }
    }

    TEST_CASE("Inigo movement requires ordinary loaded following and controller permission") {
        using namespace mod::inigo_movement_rules;
        for (const auto field :
             {&MovementSnapshot::following, &MovementSnapshot::controllerAllows, &MovementSnapshot::loaded,
              &MovementSnapshot::ordinaryOnFoot, &MovementSnapshot::normalMovement}) {
            auto walking = Walking();
            walking.*field = false;
            CHECK_FALSE(Permitted(walking));
            CHECK(Select(walking, true, true) == Selection::Owner);
        }
    }

    TEST_CASE("Inigo stand down overrides travel and remains available in combat") {
        using namespace mod::inigo_movement_rules;
        auto snapshot = Walking();
        snapshot.combat = true;
        CHECK(Select(snapshot, true, true) == Selection::Owner);
        CHECK(Select(snapshot, true, true, true) == Selection::StandDown);
        snapshot.combat = false;
        CHECK(Select(snapshot, true, true, true) == Selection::StandDown);
        CHECK(Select(snapshot, true, true, false) == Selection::Formation);
        CHECK(Select(snapshot, true, false, false) == Selection::Distance);
    }

    TEST_CASE("Inigo stand down preserves waiting without resuming travel") {
        using namespace mod::inigo_movement_rules;
        auto snapshot = Walking();
        snapshot.following = false;
        snapshot.waiting = true;
        snapshot.ordinaryOnFoot = false;
        CHECK(Select(snapshot, true, true, true) == Selection::StandDown);
        CHECK(Select(snapshot, true, true, false) == Selection::Owner);
        snapshot.waiting = false;
        CHECK(Select(snapshot, true, true, true) == Selection::Owner);
    }

    TEST_CASE("Inigo stand down yields to controller restrictions and foreign activities") {
        using namespace mod::inigo_movement_rules;
        for (const auto field : {&MovementSnapshot::pending, &MovementSnapshot::scene,
                                 &MovementSnapshot::ridingRequested, &MovementSnapshot::foreignInterrupt}) {
            auto snapshot = Walking();
            snapshot.*field = true;
            CHECK(Select(snapshot, true, true, true) == Selection::Owner);
        }
        for (const auto field : {&MovementSnapshot::controllerAllows, &MovementSnapshot::loaded,
                                 &MovementSnapshot::ordinaryPassive, &MovementSnapshot::normalMovement}) {
            auto snapshot = Walking();
            snapshot.*field = false;
            CHECK(Select(snapshot, true, true, true) == Selection::Owner);
        }
    }
}
