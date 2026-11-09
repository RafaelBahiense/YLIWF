#include <doctest/doctest.h>
#include "scenarios/DefenderSuite.h"
#include "testing/CaseSession.h"
#include <string_view>
#include <limits>
#include "testing/Participants.h"
#include "testing/Assessment.h"

using namespace mod::combat_tests;

TEST_CASE("Three-participant protection cannot pass without all opponents, ally pressure and intervention") {
    const auto* test = suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::GroupProtection));
    REQUIRE(test);
    const auto* requirements = test->Expectations<suite::Requirements>();
    REQUIRE(requirements);
    suite::Evidence evidence{true, true, false, 0, 5, 1, 1, true};
    tactics::Metrics metrics;
    metrics.maxThreats = 2;
    metrics.closest = 100;
    metrics.allyExposure = 1;
    metrics.firstIntervention = .5f;
    CHECK(suite::Evaluate(*requirements, evidence, metrics, {2, 2}).verdict == suite::Verdict::Passed);
    const auto absent = suite::Evaluate(*requirements, evidence, metrics, {2, 1});
    CHECK(absent.verdict == suite::Verdict::Inconclusive);
    CHECK(std::string_view(absent.reason).find("every declared opponent") != std::string_view::npos);
    metrics.maxThreats = 1;
    CHECK(suite::Evaluate(*requirements, evidence, metrics, {2, 2}).verdict == suite::Verdict::Inconclusive);
    metrics.maxThreats = 2;
    metrics.allyExposure = 0;
    CHECK(suite::Evaluate(*requirements, evidence, metrics, {2, 2}).verdict == suite::Verdict::Inconclusive);
    metrics.allyExposure = 1;
    metrics.firstIntervention = -1;
    CHECK(suite::Evaluate(*requirements, evidence, metrics, {2, 2}).verdict == suite::Verdict::Inconclusive);
    metrics.firstIntervention = .5f;
    evidence.completed = false;
    CHECK(suite::Evaluate(*requirements, evidence, metrics, {2, 2}).verdict == suite::Verdict::Failed);
}

TEST_CASE("Assessment behavior comes from declared requirements rather than case identity") {
    auto test = *suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::LowStamina));
    test.id = 900;
    suite::Evidence evidence{true, true, false, 0, 5, 1, 1, true};
    tactics::Metrics metrics;
    CHECK(suite::Evaluate(*test.Expectations<suite::Requirements>(), evidence, metrics).verdict ==
          suite::Verdict::Failed);
    test.Expect(suite::Requirements::Melee());
    CHECK(suite::Evaluate(*test.Expectations<suite::Requirements>(), evidence, metrics).verdict ==
          suite::Verdict::Passed);
    evidence.fixtureComplete = false;
    CHECK(suite::Evaluate(*test.Expectations<suite::Requirements>(), evidence, metrics).verdict ==
          suite::Verdict::Inconclusive);
    test.Expect(suite::Requirements::Tactical({suite::Criterion::OpponentAttack}));
    CHECK(suite::Evaluate(*test.Expectations<suite::Requirements>(), evidence, metrics).verdict ==
          suite::Verdict::Inconclusive);
    evidence.fixtureComplete = true;
    test.Expect(suite::Requirements::Tactical({}));
    CHECK(suite::Evaluate(*test.Expectations<suite::Requirements>(), evidence, metrics).verdict ==
          suite::Verdict::Inconclusive);
}

TEST_CASE("Declared intervention counts track phase edits without a case-ID switch") {
    auto test = *suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::RecoveryWaves));
    CHECK(test.Interventions() == 3);
    test.Phase(testing::Phase{"Extra window", 5}.OnEnter(testing::Action::StaminaFraction, .2f));
    CHECK(test.Interventions() == 4);
    test.phases.erase(test.phases.begin());
    CHECK(test.Interventions() == 3);
    test.id = 55;
    CHECK(test.Interventions() == 3);
}

TEST_CASE("Phase entry commands retain their cursor while awaiting a receipt") {
    testing::TestCase test{7, "Commands"};
    test.Phase(testing::Phase{"First", 1}
                   .OnEnter(testing::Action::StaminaFraction, .15f)
                   .OnEnter(testing::Action::Reposition, 200));
    test.Phase(testing::Phase{"Second", 1}.OnEnter(testing::Action::Reposition, -200));
    testing::Context context;
    unsigned stamina = 0, right = 0, left = 0;
    context.execute = [&](const testing::Command& command) {
        if (command.action == testing::Action::StaminaFraction)
            ++stamina;
        else if (command.value > 0) {
            if (++right == 1)
                return testing::Result::Pending();
        } else
            ++left;
        return testing::Result{};
    };
    testing::CaseSession session;
    REQUIRE(session.Begin(test, context));
    REQUIRE(session.Prepare().state == testing::State::Ready);
    CHECK(session.Enter().state == testing::State::Pending);
    CHECK(session.Enter().state == testing::State::Ready);
    CHECK(stamina == 1);
    CHECK(right == 2);
    CHECK(session.Tick(1).state == testing::State::Pending);
    CHECK(left == 1);
    CHECK(stamina == 1);
    CHECK(session.Tick(1).state == testing::State::Ready);
    CHECK(left == 1);
}

TEST_CASE("Ranged scenario verdict needs useful movement and actual threat coverage") {
    tactics::Metrics metrics;
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Archer))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.Observe(.1f, 1, 3, 1, 1200, 0, 0);
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Archer))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.Observe(.1f, 1, 3, 1, 180, 100, 0);
    CHECK_FALSE(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Archer))
                                    ->Expectations<suite::Requirements>(),
                               metrics, 1));
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Projectile))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.Observe(.1f, 6, 4, 1, 180, 200, 0);
    metrics.casts = 1;
    CHECK_FALSE(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Projectile))
                                    ->Expectations<suite::Requirements>(),
                               metrics, 1));
}

TEST_CASE("Earlier caster coverage cannot mask terminal inactivity or fixture friendly fire") {
    tactics::Metrics metrics;
    metrics.casts = 1;
    metrics.Observe(.1f, 6, 5, 1, 200, 0, 0);
    metrics.Observe(.1f, 6, 5, 1, 180, 100, 0);
    CHECK_FALSE(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Concentration))
                                    ->Expectations<suite::Requirements>(),
                               metrics, 1));
    for (int i = 0; i < 110; ++i)
        metrics.Observe(.1f, 6, 5, 1, 180, 100, 0);
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Concentration))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.Contacts(1, 0);
    metrics.Observe(.1f, 6, 5, 1, 180, 100, 0);
    CHECK_FALSE(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Concentration))
                                    ->Expectations<suite::Requirements>(),
                               metrics, 1));
    metrics.friendlyHits = 1;
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Concentration))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.friendlyHits = 0;
    metrics.casts = 0;
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Concentration))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
}

TEST_CASE("Heavy and multiple attacker coverage cannot pass from elapsed time alone") {
    tactics::Metrics metrics;
    metrics.Observe(.1f, 6, 2, 1, 100, 0, 0);
    CHECK(suite::Missing(
        *suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Heavy))->Expectations<suite::Requirements>(),
        metrics, 1));
    metrics.powerWindups = 1;
    CHECK_FALSE(suite::Missing(
        *suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Heavy))->Expectations<suite::Requirements>(),
        metrics, 1));
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Multiple))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.Observe(.1f, 6, 1, 2, 100, 10, 0);
    CHECK_FALSE(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::Multiple))
                                    ->Expectations<suite::Requirements>(),
                               metrics, 1));
}

TEST_CASE("Ranged ally protection measures finite health and requires an intervention") {
    const auto* test = suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::AllyPressure));
    REQUIRE(test);
    const auto* ally = test->FirstActor(testing::ActorRole::Ally);
    const auto* opponent = test->FirstActor(testing::ActorRole::Opponent);
    REQUIRE(ally);
    REQUIRE(opponent);
    CHECK(ally->y < opponent->y);
    CHECK(testing::ParticipantHealth(*ally, 630) == doctest::Approx(472.5f));
    CHECK(testing::ParticipantHealth(*ally, 100) == 300);
    CHECK(testing::ParticipantHealth(*ally, 5000) == 900);
    CHECK(testing::ParticipantHealth(*ally, std::numeric_limits<float>::quiet_NaN()) == 0);
    CHECK(testing::Defeated(true, false, 10));
    CHECK(testing::Defeated(false, true, 10));
    CHECK(testing::Defeated(false, false, 0));
    CHECK_FALSE(testing::Defeated(false, false, 100));
    tactics::Metrics metrics;
    metrics.Observe(.1f, 1, 1, 1, 180, 0, 0);
    metrics.ObserveAlly(0, 0, 472.5f, true, false);
    metrics.ObserveAlly(1, 1, 450, true, false);
    CHECK(metrics.allyInitialHealth == doctest::Approx(472.5f));
    CHECK(metrics.allyDamage == doctest::Approx(22.5f));
    CHECK(metrics.allyExposure == 1);
    CHECK(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::AllyPressure))
                              ->Expectations<suite::Requirements>(),
                         metrics, 1));
    metrics.ObserveAlly(2, 1, 455, false, true);
    CHECK(metrics.firstIntervention == 2);
    CHECK(metrics.allyDamage == doctest::Approx(22.5f));
    CHECK_FALSE(suite::Missing(*suite::Definition.Find(static_cast<std::uint32_t>(suite::Fixture::AllyPressure))
                                    ->Expectations<suite::Requirements>(),
                               metrics, 1));
    metrics.ObserveAlly(3, 1, -20, true, false);
    CHECK(metrics.allyHealth == 0);
    CHECK(metrics.allyDamage == doctest::Approx(477.5f));
}
