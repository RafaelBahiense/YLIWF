#include <doctest/doctest.h>
#include "testing/Participants.h"
#include "testing/SuiteSession.h"
#include "scenarios/DefenderSuite.h"

using namespace mod::combat_tests::testing;

TEST_CASE("Named participants reject identity reuse and resolve teams and target edges independently of order") {
    Participants actors;
    const auto attacker = ActorSpawner{"swordsman"}.Target("archer").Spec();
    const auto flanker = ActorSpawner{"flanker"}.Target("subject").Spec();
    const auto ally = ActorSpawner{"archer"}.Role(ActorRole::Ally).Target("swordsman").Spec();
    REQUIRE(actors.Track(ally, 30, 300));
    REQUIRE(actors.Track(flanker, 20, 200));
    REQUIRE(actors.Track(attacker, 10, 100));
    CHECK(actors.Track(attacker, 10, 100));
    CHECK_FALSE(actors.Track(attacker, 11, 100));
    CHECK_FALSE(actors.Track(attacker, 10, 101));
    CHECK_FALSE(actors.Track(ActorSpawner{"replacement"}.Spec(), 10, 100));
    CHECK_FALSE(actors.Track(ActorSpawner{"swordsman"}.Role(ActorRole::Ally).Spec(), 10, 100));
    CHECK(actors.Entries().size() == 3);
    CHECK(actors.Target(*actors.Find("swordsman"), 1) == 30);
    CHECK(actors.Target(*actors.Find("flanker"), 1) == 1);
    CHECK(actors.Target(*actors.Find("archer"), 1) == 10);
    CHECK(actors.Opposed(10, 30, 1));
    CHECK(actors.Opposed(1, 20, 1));
    CHECK(actors.Opposed(20, 1, 1));
    CHECK_FALSE(actors.Opposed(1, 30, 1));
    CHECK_FALSE(actors.Opposed(10, 20, 1));
    CHECK_FALSE(actors.Opposed(10, 99, 1));
}

TEST_CASE("Cases reject missing targets, same-team targets and reserved subject identities") {
    Suite suite{"Actors"};
    suite.Case(TestCase{42, "Protect"}
                   .Actor(ActorSpawner{"swordsman"}.Target("archer"))
                   .Actor(ActorSpawner{"archer"}.Role(ActorRole::Ally).Target("swordsman"))
                   .Phase({"Fight", 1}));
    REQUIRE(suite.Valid());
    suite.cases[0].SubjectTarget("archer");
    CHECK_FALSE(suite.Valid());
    suite.cases[0].SubjectTarget("swordsman");
    CHECK(suite.Valid());
    suite.cases[0].actors[0].Target("missing");
    CHECK_FALSE(suite.Valid());
    suite.cases[0].actors[0].Target("swordsman");
    CHECK_FALSE(suite.Valid());
    suite.cases[0].actors[0].Target("archer");
    suite.cases[0].Actor(ActorSpawner{"other"}.Target("swordsman"));
    CHECK_FALSE(suite.Valid());
    suite.cases[0].actors.pop_back();
    suite.cases[0].Actor({"subject"});
    CHECK_FALSE(suite.Valid());
}

TEST_CASE("Custom factories use captured configuration and must register successful actors for cleanup") {
    struct Settings {
        std::uint32_t archetype;
    };

    Context context;
    context.configuration = Settings{2};
    context.generation = 72;
    std::uint32_t calls = 0, creations = 0, cleanups = 0;
    context.execute = [&](const Command& command) {
        CHECK(command.generation == 72);
        CHECK(command.actor.archetype == 2);
        if (!context.resources.Find(command.scope, command.actor.key)) {
            ++creations;
            REQUIRE(context.resources.Own(command.scope, command.actor.key, 12, [&] {
                ++cleanups;
                return Result{};
            }));
        }
        return ++calls == 1 ? Result::Pending() : Result{};
    };
    auto factory = ActorSpawner{"custom"}.Using([](Context& ctx, const ActorSpec& spec) {
        auto configured = spec;
        configured.archetype = ctx.Settings<Settings>()->archetype;
        return ctx.Spawn(configured);
    });
    TestCase test{42, "Custom"};
    test.Actor(factory).Phase({"Fight", 1});
    CaseSession run;
    REQUIRE(run.Begin(test, context));
    CHECK(run.Prepare().state == State::Pending);
    CHECK(run.Prepare().state == State::Ready);
    CHECK(run.Prepare().state == State::Ready);
    CHECK(creations == 1);
    CHECK(calls == 2);
    CHECK(run.Restore().state == State::Ready);
    CHECK(cleanups == 1);

    auto untracked = ActorSpawner{"untracked"}.Using([](Context&, const ActorSpec&) { return Result{}; });
    CHECK(untracked.Spawn(context).state == State::Failed);
}

TEST_CASE("Three declared actors share phases and are all cleaned before another repetition") {
    using namespace mod::combat_tests;
    const auto id = static_cast<std::uint32_t>(suite::Fixture::GroupProtection);
    const auto* definition = suite::Definition.Find(id);
    REQUIRE(definition);
    REQUIRE(definition->actors.size() == 3);
    SuiteSession session;
    Participants actors;
    std::uint32_t sequence = 100;
    std::vector<std::string> removed;
    session.context.execute = [&](const Command& command) {
        if (command.action != Action::Spawn)
            return Result{};
        const auto ref = ++sequence;
        REQUIRE(actors.Track(command.actor, ref, 700 + command.actor.archetype));
        REQUIRE(session.context.resources.Own(command.scope, command.actor.key, ref, [&, key = command.actor.key] {
            removed.push_back(key);
            return Result{};
        }));
        return Result{};
    };
    REQUIRE(session.Begin(suite::Definition, 0));
    for (const auto repetition : {1u, 2u}) {
        actors.Clear();
        REQUIRE(session.BeginCase(id, repetition, repetition));
        REQUIRE(session.current.Prepare().state == State::Ready);
        CHECK(actors.Entries().size() == 3);
        CHECK(sequence == 100 + 3 * repetition);
        REQUIRE(session.current.Enter().state == State::Ready);
        session.current.Tick(.1f);
        CHECK(actors.Target(*actors.Find("attacker"), 1) == actors.Find("ally")->id);
        // Abort in the measured phase, rather than waiting for normal completion.
        REQUIRE(session.RestoreCase().state == State::Ready);
        CHECK(removed.size() == 3 * repetition);
        CHECK(removed[removed.size() - 3] == "ally");
        CHECK(removed[removed.size() - 2] == "flanker");
        CHECK(removed.back() == "attacker");
    }
}

TEST_CASE("A partial three-actor failure retains cleanup until its asynchronous disposal is acknowledged") {
    bool disposed = false;
    unsigned created = 0;
    Suite suite{"Partial"};
    suite.Case(TestCase{42, "Partial"}.Actor({"first"}).Actor({"second"}).Actor({"third"}).Phase({"Fight", 1}));
    SuiteSession session;
    session.context.execute = [&](const Command& command) {
        if (command.actor.key == "third")
            return Result::Failed("Rejected");
        const auto id = ++created;
        REQUIRE(session.context.resources.Own(command.scope, command.actor.key, id,
                                              [&] { return disposed ? Result{} : Result::Pending(); }));
        return Result{};
    };
    REQUIRE(session.Begin(suite, 0));
    REQUIRE(session.BeginCase(42, 1, 1));
    CHECK(session.current.Prepare().state == State::Failed);
    CHECK(created == 2);
    CHECK(session.RestoreCase().state == State::Pending);
    CHECK_FALSE(session.BeginCase(42, 2, 2));
    disposed = true;
    CHECK(session.RestoreCase().state == State::Ready);
    CHECK(session.BeginCase(42, 2, 2));
}
