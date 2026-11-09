#include <doctest/doctest.h>
#include "scenarios/Encounters.h"
#include "testing/RunPlan.h"
#include "testing/SuiteSession.h"
#include <cstdint>
#include <vector>

TEST_CASE("Every encounter resolves to its owning suite with the declared roles and timing") {
    using namespace mod::test_rules;
    using namespace mod::combat_tests::testing;
    for (std::uint32_t id = 0; id < Scenarios.size(); ++id) {
        const auto& navigation = Scenarios[id];
        CHECK(navigation.id == id);
        const auto& catalog = EncounterCatalog(id);
        REQUIRE(catalog.Valid());
        CHECK(&catalog.Definition() == navigation.suite);
        REQUIRE(catalog.Find(navigation.caseID));
        RunPlan plan{catalog, {3, CaseMask(navigation.caseID), false}};
        CHECK(plan.Valid());
        CHECK(plan.Runs() == 3);
        const auto seconds = DirectEncounter(id) ? 20 : id == 0 ? 45 : 60;
        CHECK(plan.Seconds() == seconds * 3);
        CHECK(EncounterRole(id) == navigation.role);
        CHECK(EquipmentRole(id) == navigation.equipmentRole);
    }
    CHECK(EncounterCatalog(2).Definition().name == "Action primitives");
    CHECK(EncounterCatalog(1).Definition().name == "Healer");
    CHECK(&EncounterCatalog(6).Definition() == &mod::combat_tests::suite::Definition);
}

TEST_CASE("Healer phases produce five spaced injury waves and a fresh scoped opponent per repetition") {
    using namespace mod::combat_tests;
    testing::SuiteSession session;
    std::vector<float> injuries;
    unsigned spawned = 0, disposed = 0, observerMoves = 0;
    session.context.execute = [&](const testing::Command& command) {
        if (command.action == testing::Action::Spawn) {
            ++spawned;
            CHECK(command.actor.target == "subject");
            CHECK(command.actor.protectedHealth);
            CHECK(command.actor.x == -512);
            CHECK(command.actor.y == 1280);
            CHECK(session.context.resources.Own(command.scope, command.actor.key, spawned, [&] {
                ++disposed;
                return testing::Result{};
            }));
        } else if (command.action == testing::Action::InjurePlayer) {
            CHECK(command.value == doctest::Approx(.45f));
            injuries.push_back(session.context.elapsed);
        } else if (command.action == testing::Action::PreparePlayer)
            ++observerMoves;
        return testing::Result{};
    };
    REQUIRE(session.Begin(healer::Definition, 0));
    REQUIRE(session.Prepare().state == testing::State::Ready);
    for (unsigned repetition = 1; repetition <= 2; ++repetition) {
        REQUIRE(session.BeginCase(0, repetition, repetition));
        REQUIRE(session.current.Prepare().state == testing::State::Ready);
        REQUIRE(session.current.Enter().state == testing::State::Ready);
        for (unsigned tick = 1; tick <= 60; ++tick) {
            session.context.elapsed = static_cast<float>(tick);
            session.current.Tick(1);
        }
        CHECK(session.current.Done());
        CHECK_FALSE(session.context.completion);
        REQUIRE(session.RestoreCase().state == testing::State::Ready);
        CHECK(disposed == repetition);
    }
    CHECK(injuries == std::vector<float>{0, 12, 24, 36, 48, 0, 12, 24, 36, 48});
    CHECK(spawned == 2);
    CHECK(observerMoves == 0);
    CHECK(session.context.resources.Empty());
}

TEST_CASE("Leveled encounters declare killable natural opponents and use scoped factory cleanup") {
    using namespace mod::combat_tests;
    testing::SuiteSession session;
    unsigned disposed = 0;
    session.context.execute = [&](const testing::Command& command) {
        if (command.action == testing::Action::Spawn) {
            CHECK(command.actor.preserveLoadout);
            CHECK_FALSE(command.actor.protectedHealth);
            CHECK(command.actor.archetype == static_cast<unsigned>(fixtures::Archetype::LeveledMage));
            CHECK(command.actor.y == 1500);
            CHECK(session.context.resources.Own(command.scope, command.actor.key, 12, [&] {
                ++disposed;
                return testing::Result{};
            }));
        }
        return testing::Result{};
    };
    REQUIRE(session.Begin(suite::Definition, 0));
    REQUIRE(session.BeginCase(static_cast<unsigned>(suite::Fixture::LeveledMage), 1, 1));
    REQUIRE(session.current.Prepare().state == testing::State::Ready);
    REQUIRE(session.current.Enter().state == testing::State::Ready);
    session.current.Tick(1);
    CHECK_FALSE(session.current.Done());
    REQUIRE(session.RestoreCase().state == testing::State::Ready);
    CHECK(disposed == 1);
}

TEST_CASE("Early case completion waits for phase cleanup and never enters the following phase") {
    using namespace mod::combat_tests::testing;
    Suite suite{"Early completion"};
    unsigned requests = 0, leave = 0;
    suite.Case(TestCase{0, "Case"}
                   .Phase(Phase{"Evidence", 20}
                              .Observe([](Context& context) {
                                  context.Finish(Verdict::Passed, "Acknowledged.");
                                  return Result{};
                              })
                              .Cleanup([&](Context&) { return ++leave == 1 ? Result::Pending("Waiting.") : Result{}; }))
                   .Phase(Phase{"Must not execute", 20}.Prepare([&](Context&) {
                       ++requests;
                       return Result{};
                   })));
    SuiteSession session;
    REQUIRE(session.Begin(suite, 0));
    REQUIRE(session.BeginCase(0, 1, 1));
    REQUIRE(session.current.Prepare().state == State::Ready);
    CHECK(session.current.Tick(.1f).state == State::Pending);
    CHECK_FALSE(session.current.Done());
    CHECK_FALSE(session.BeginCase(0, 2, 2));
    CHECK(session.current.Tick(.1f).state == State::Ready);
    CHECK(session.current.Done());
    CHECK(requests == 0);
    CHECK(session.current.Results()[0].completed);
    CHECK_FALSE(session.current.Results()[1].entered);
    REQUIRE(session.RestoreCase().state == State::Ready);
    REQUIRE(session.BeginCase(0, 2, 2));
    CHECK_FALSE(session.context.completion);
    CHECK_FALSE(session.context.advancePhase);
}
