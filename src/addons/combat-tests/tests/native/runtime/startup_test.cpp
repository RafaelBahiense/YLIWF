#include <doctest/doctest.h>
#include "runtime/Rules.h"
#include "runtime/StartupCombatRules.h"
#include "scenarios/DefenderSuite.h"
#include "testing/RunPlan.h"

using namespace mod::test_rules;

TEST_CASE("Ally startup validates hostility toward the ally being attacked rather than the defender") {
    using namespace mod::combat_tests::startup_combat;
    Snapshot state{1, 2, 3, 4, 2, 3, 2, true, true, false, true, false, true};
    CHECK(Observe(state) == State::Ready);
    CHECK(ObserveStart(1, true, true, true, Observe(state) == State::Ready) == StartDecision::Ready);
    state.opponentHostileToAlly = false;
    CHECK(Observe(state) == State::Waiting);
    state.opponentHostileToAlly = true;
    state.allyReady = false;
    CHECK(Observe(state) == State::Waiting);
    state.allyReady = true;
    state.opponentTarget = 0;
    CHECK(Observe(state) == State::Waiting);
    state.opponentTarget = 3;
    state.auxiliary = 0;
    CHECK(Observe(state) != State::Ready);
}

TEST_CASE("Ally startup accepts an early defender interception only with observed hostility") {
    using namespace mod::combat_tests::startup_combat;
    Snapshot state{1, 2, 3, 4, 2, 1, 2, true, true, false, true, false, true};
    CHECK(Observe(state) == State::Waiting);
    state.opponentHostileToSubject = true;
    CHECK(Observe(state) == State::Ready);
    state.subjectHostile = false;
    CHECK(Observe(state) == State::Waiting);
}

TEST_CASE("Startup rejects observer, unrelated and friendly targets despite valid hostility") {
    using namespace mod::combat_tests::startup_combat;
    const Snapshot valid{1, 2, 3, 4, 2, 3, 2, true, true, false, true, false, true};
    for (auto field : {&Snapshot::subjectTarget, &Snapshot::opponentTarget, &Snapshot::auxiliaryTarget})
        for (const auto target : {4u, 99u}) {
            auto state = valid;
            state.*field = target;
            CHECK(Observe(state) == State::UnsafeTarget);
        }
    auto state = valid;
    state.subjectTarget = 3;
    CHECK(Observe(state) == State::UnsafeTarget);
    state = valid;
    state.auxiliaryTarget = 1;
    CHECK(Observe(state) == State::UnsafeTarget);
}

TEST_CASE("Normal and two-attacker startup preserve their hostility gate and natural encounter exemption") {
    using namespace mod::combat_tests::startup_combat;
    Snapshot state{1, 2, 0, 4, 2, 1, 0, false, false, false, true, true, false};
    CHECK(Observe(state) == State::Ready);
    state.opponentHostileToSubject = false;
    CHECK(Observe(state) == State::Waiting);
    state.natural = true;
    CHECK(Observe(state) == State::Ready);
    state.opponentTarget = 4;
    CHECK(Observe(state) == State::UnsafeTarget);
    state = {1, 2, 3, 4, 3, 1, 1, false, false, false, true, true, false};
    CHECK(Observe(state) == State::Ready);
}

TEST_CASE("Failed suite phases advance only after restoration and preserve the remaining schedule") {
    using namespace mod::test_rules;
    CHECK(ContinueSeries(7, 20, RepeatableRun(false, false, false, true), true, 100));
    CHECK_FALSE(ContinueSeries(7, 20, RepeatableRun(false, false, false, true), false, 100));
    CHECK_FALSE(ContinueSeries(7, 20, RepeatableRun(false, false, true, true), true, 100));
    CHECK_FALSE(ContinueSeries(20, 20, RepeatableRun(false, false, false, true), true, 100));
    CHECK_FALSE(ContinueSeries(7, 20, RepeatableRun(false, false, false), true, 100));
    using namespace mod::combat_tests::suite;
    const mod::combat_tests::testing::RunPlan run{Catalog, Catalog.Select(DefenderAcceptance, 2)};
    CHECK(run.At(8).id == static_cast<std::uint32_t>(Fixture::AllyPressure));
    CHECK(run.At(11).id == static_cast<std::uint32_t>(Fixture::Archer));
    CHECK(run.At(20).id == static_cast<std::uint32_t>(Fixture::Area));
}

TEST_CASE("Combat startup requires observed hostility and allows it to settle within the existing deadline") {
    CHECK(ObserveStart(0, true, true, true, false) == StartDecision::Wait);
    CHECK(ObserveStart(StartTimeout - .1f, true, true, true, false) == StartDecision::Wait);
    CHECK(ObserveStart(StartTimeout, true, true, true, false) == StartDecision::Failed);
    CHECK(ObserveStart(1, true, true, true, true) == StartDecision::Ready);
    CHECK(ObserveStart(1, true, true, false, true) == StartDecision::Wait);
    CHECK(ObserveStart(1, false, true, true, true) == StartDecision::Wait);
}

TEST_CASE("Arena startup waits for transient scenes or combat without bypassing availability") {
    StartupState subject{true, true, true};
    CHECK(Startup(subject, 0) == StartupDecision::Ready);
    subject.scene = true;
    CHECK(Startup(subject, 0) == StartupDecision::WaitScene);
    CHECK(Startup(subject, StartupSettleTimeout - .1f) == StartupDecision::WaitScene);
    CHECK(Startup(subject, StartupSettleTimeout) == StartupDecision::Reject);
    subject.combat = true;
    CHECK(Startup(subject, 1) == StartupDecision::WaitScene);
    subject.scene = false;
    CHECK(Startup(subject, 1) == StartupDecision::WaitCombat);
    CHECK(Startup(subject, StartupSettleTimeout) == StartupDecision::Reject);
    subject.combat = false;
    CHECK(Startup(subject, StartupSettleTimeout) == StartupDecision::Ready);
}

TEST_CASE("Scene waiting never authorizes dead, downed, unavailable or quest-restricted subjects") {
    const StartupState healthy{true, true, true};
    for (auto field : {&StartupState::dead, &StartupState::disabled, &StartupState::mounted, &StartupState::unconscious,
                       &StartupState::downed}) {
        auto subject = healthy;
        subject.scene = true;
        subject.*field = true;
        CHECK(Startup(subject, 0) == StartupDecision::Reject);
    }
    for (auto field : {&StartupState::exists, &StartupState::managed, &StartupState::allowed}) {
        auto subject = healthy;
        subject.scene = true;
        subject.*field = false;
        CHECK(Startup(subject, 0) == StartupDecision::Reject);
    }
}
