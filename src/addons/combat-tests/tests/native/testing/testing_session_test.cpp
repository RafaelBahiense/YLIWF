#include <doctest/doctest.h>
#include "testing/SuiteSession.h"
#include "testing/RunPlan.h"
#include "scenarios/DefenderSuite.h"
#include <array>
#include <limits>

using namespace mod::combat_tests::testing;

TEST_CASE("Suite definitions reject duplicate identities, unusable phases and conflicting actor keys") {
    Suite suite{"Examples"};
    CHECK_FALSE(suite.Valid());
    suite.Case(TestCase{0, "First"}.Phase({"Fight", 1}));
    CHECK(suite.Valid());
    suite.Case(TestCase{0, "Duplicate"}.Phase({"Fight", 1}));
    CHECK_FALSE(suite.Valid());
    suite.cases.pop_back();
    suite.cases[0].phases[0].seconds = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(suite.Valid());
    suite.cases[0].phases[0].seconds = 1;
    suite.cases[0].Actor({"opponent"}).Actor({"opponent"});
    CHECK_FALSE(suite.Valid());
}

TEST_CASE("Scoped cleanup is reversed and pending resources block parents without replaying completed cleanup") {
    Resources resources;
    std::vector<int> closed;
    bool ready = false;
    REQUIRE(resources.Own(Scope::Suite, "player", 1, [&] {
        closed.push_back(1);
        return Result{};
    }));
    REQUIRE(resources.Own(Scope::Case, "subject", 2, [&] {
        closed.push_back(2);
        return Result{};
    }));
    REQUIRE(resources.Own(Scope::Case, "opponent", 3, [&] { return ready ? Result{} : Result::Pending(); }));
    REQUIRE(resources.Own(Scope::Phase, "temporary", 4, [&] {
        closed.push_back(4);
        return Result{};
    }));
    CHECK(resources.Close(Scope::Case).state == State::Pending);
    CHECK(closed == std::vector<int>{4});
    CHECK(resources.Find(Scope::Suite, "player") == 1);
    ready = true;
    CHECK(resources.Close(Scope::Case).state == State::Ready);
    CHECK(closed == std::vector<int>{4, 2});
    CHECK(resources.Close(Scope::Case).state == State::Ready);
    CHECK(closed.size() == 2);
    CHECK(resources.Close(Scope::Suite).state == State::Ready);
    CHECK(closed == std::vector<int>{4, 2, 1});
}

TEST_CASE("Reset and destruction drop journal callbacks without issuing engine work") {
    int mutations = 0;
    {
        Resources resources;
        REQUIRE(resources.Own(Scope::Case, "actor", 1, [&] {
            ++mutations;
            return Result{};
        }));
        resources.ResetForLoad();
    }
    CHECK(mutations == 0);
}

TEST_CASE("Case phases preserve their encounter while each repetition receives fresh actors and copied settings") {
    struct Settings {
        int value;
    } settings{7};

    std::uint32_t created = 0, deleted = 0, entries = 0, suitePrepare = 0;
    Suite suite{"Defender"};
    suite.Prepare([&](Context&) {
        ++suitePrepare;
        return Result{};
    });
    suite.Case(TestCase{0, "Protection"}
                   .Actor({"enemy"})
                   .Phase(Phase{"Intercept", 1}.Prepare([&](Context& ctx) {
                       CHECK(ctx.Settings<Settings>()->value == 7);
                       ++entries;
                       return Result{};
                   }))
                   .Phase(Phase{"Protect", 1}.Prepare([&](Context&) {
                       ++entries;
                       return Result{};
                   })));
    SuiteSession session;
    session.context.execute = [&](const Command& command) {
        if (command.action != Action::Spawn)
            return Result{};
        const auto id = ++created;
        REQUIRE(session.context.resources.Own(command.scope, command.actor.key, id, [&] {
            ++deleted;
            return Result{};
        }));
        return Result{};
    };
    REQUIRE(session.Begin(suite, settings));
    settings.value = 99;
    for (const auto repetition : {1u, 2u}) {
        REQUIRE(session.BeginCase(0, repetition, repetition));
        REQUIRE(session.Prepare().state == State::Ready);
        REQUIRE(session.current.Prepare().state == State::Ready);
        REQUIRE(session.current.Enter().state == State::Ready);
        CHECK(session.current.Tick(1).state == State::Pending);
        CHECK(created == repetition);
        CHECK(deleted == repetition - 1);
        CHECK(session.current.Tick(1).state == State::Ready);
        CHECK(session.current.Done());
        CHECK(session.current.Results()[0].completed);
        CHECK(session.current.Results()[1].completed);
        CHECK(session.RestoreCase().state == State::Ready);
        CHECK(deleted == repetition);
    }
    CHECK(suitePrepare == 1);
    CHECK(entries == 4);
    CHECK(session.Close().state == State::Ready);
}

TEST_CASE("Partial actor preparation still cleans the actors already created") {
    int created = 0, deleted = 0;
    Suite suite{"Partial"};
    suite.Case(TestCase{0, "Creation failure"}.Actor({"first"}).Actor({"second"}).Phase({"Fight", 1}));
    SuiteSession session;
    session.context.execute = [&](const Command& command) {
        if (command.actor.key == "second")
            return Result::Failed("Creation rejected");
        const auto id = static_cast<std::uint32_t>(++created);
        session.context.resources.Own(command.scope, command.actor.key, id, [&] {
            ++deleted;
            return Result{};
        });
        return Result{};
    };
    REQUIRE(session.Begin(suite, 0));
    REQUIRE(session.BeginCase(0, 1, 1));
    CHECK(session.current.Prepare().state == State::Failed);
    CHECK(created == 1);
    CHECK(session.RestoreCase().state == State::Ready);
    CHECK(deleted == 1);
}

TEST_CASE("Pending actor setup does not rerun completed case preparation and commands carry their case token") {
    int preparations = 0, polls = 0;
    Suite suite{"Pending"};
    suite.Case(TestCase{0, "Actors"}
                   .Prepare([&](Context&) {
                       ++preparations;
                       return Result{};
                   })
                   .Actor({"enemy"})
                   .Phase({"Fight", 1}));
    SuiteSession session;
    session.context.execute = [&](const Command& command) {
        CHECK(command.generation == 42);
        return ++polls == 1 ? Result::Pending() : Result{};
    };
    REQUIRE(session.Begin(suite, 0));
    REQUIRE(session.BeginCase(0, 1, 42));
    CHECK(session.current.Prepare().state == State::Pending);
    CHECK(session.current.Prepare().state == State::Ready);
    CHECK(preparations == 1);
    CHECK(polls == 2);
}

TEST_CASE("Failed phase commands are not retried by subsequent execution ticks") {
    int attempts = 0;
    Context context;
    TestCase test{0, "Failure"};
    test.Phase(Phase{"Enter", 1}.Prepare([&](Context&) {
        ++attempts;
        return Result::Failed("Rejected");
    }));
    CaseSession session;
    REQUIRE(session.Begin(test, context));
    REQUIRE(session.Prepare().state == State::Ready);
    CHECK(session.Tick(.1f).state == State::Failed);
    CHECK(session.Tick(.1f).state == State::Failed);
    CHECK(attempts == 1);
}

TEST_CASE("Pending phase cleanup never advances or replays its completed exit hook") {
    Context context;
    int entered = 0, exited = 0;
    bool cleaned = false;
    TestCase test{0, "Phases"};
    test.Phase(Phase{"First", 1}
                   .Prepare([&](Context& ctx) {
                       ++entered;
                       ctx.resources.Own(Scope::Phase, "effect", 1,
                                         [&] { return cleaned ? Result{} : Result::Pending(); });
                       return Result{};
                   })
                   .Cleanup([&](Context&) {
                       ++exited;
                       return Result{};
                   }));
    test.Phase({"Second", 1});
    CaseSession run;
    REQUIRE(run.Begin(test, context));
    REQUIRE(run.Prepare().state == State::Ready);
    CHECK(run.Tick(1).state == State::Pending);
    CHECK(run.Tick(1).state == State::Pending);
    CHECK(run.Index() == 0);
    CHECK(entered == 1);
    CHECK(exited == 1);
    CHECK(run.Results()[0].elapsed == 1);
    cleaned = true;
    CHECK(run.Tick(0).state == State::Pending);
    CHECK(run.Index() == 1);
    CHECK(exited == 1);
}

TEST_CASE("A failed phase preparation runs cleanup and prevents the next case until resources are restored") {
    bool ready = false;
    int exits = 0;
    Suite suite{"Failures"};
    suite.Case(TestCase{0, "Failure"}.Phase(Phase{"Prepare", 1}
                                                .Prepare([&](Context& ctx) {
                                                    ctx.resources.Own(Scope::Phase, "resource", 1, [&] {
                                                        return ready ? Result{} : Result::Pending();
                                                    });
                                                    return Result::Failed("Preparation failed");
                                                })
                                                .Cleanup([&](Context&) {
                                                    ++exits;
                                                    return Result{};
                                                })));
    SuiteSession run;
    REQUIRE(run.Begin(suite, 0));
    REQUIRE(run.BeginCase(0, 1, 1));
    REQUIRE(run.current.Prepare().state == State::Ready);
    CHECK(run.current.Enter().state == State::Failed);
    CHECK(run.RestoreCase().state == State::Pending);
    CHECK_FALSE(run.BeginCase(0, 2, 2));
    CHECK(run.Close().state == State::Failed);
    ready = true;
    CHECK(run.RestoreCase().state == State::Ready);
    CHECK(exits == 1);
    CHECK(run.BeginCase(0, 2, 2));
}

TEST_CASE("Run plans support case repeats and alternating variants without repeating individual phases") {
    Suite suite{"Plan"};
    suite.Case(TestCase{0, "Shared encounter"}.Repeat(2).Phase({"A", 1}).Phase({"B", 2}));
    suite.Case(
        TestCase{1, "Comparison"}.Variation(10, "On").Variation(20, "Off").AlternateVariations().Phase({"Fight", 4}));
    const std::array order{0u, 1u};
    RunPlan plan{suite, order, 2, 3, true};
    CHECK(plan.Runs() == 6);
    CHECK(plan.Seconds() == 22);
    CHECK(plan.At(3).variant == 10);
    CHECK(plan.At(4).variant == 20);
    CHECK(plan.At(5).variant == 20);
    CHECK(plan.At(6).variant == 10);
    CHECK(plan.At(7).id == UINT32_MAX);
}

TEST_CASE("Defender recovery and flank phases issue interventions at boundaries within one encounter") {
    using namespace mod::combat_tests;
    REQUIRE(suite::Definition.Valid());
    for (const auto fixture : {suite::Fixture::RecoveryWaves, suite::Fixture::Reposition}) {
        Context context;
        std::vector<std::pair<Action, float>> interventions;
        int spawns = 0;
        context.execute = [&](const Command& command) {
            if (command.action == Action::Spawn)
                ++spawns;
            if (command.action == Action::StaminaFraction || command.action == Action::Reposition)
                interventions.emplace_back(command.action, command.value);
            return Result{};
        };
        CaseSession run;
        REQUIRE(run.Begin(*suite::Definition.Find(static_cast<std::uint32_t>(fixture)), context));
        REQUIRE(run.Prepare().state == State::Ready);
        REQUIRE(run.Enter().state == State::Ready);
        for (int tick = 0; tick < 60; ++tick)
            run.Tick(1);
        CHECK(run.Done());
        CHECK(spawns == 1);
        CHECK(interventions.size() == (fixture == suite::Fixture::RecoveryWaves ? 3 : 2));
        if (fixture == suite::Fixture::Reposition) {
            CHECK(interventions[0].second == -220);
            CHECK(interventions[1].second == 220);
        }
    }
}
