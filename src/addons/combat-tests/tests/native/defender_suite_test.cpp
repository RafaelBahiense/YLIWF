#include <doctest/doctest.h>
#include "DefenderSuite.h"
#include <string_view>
#include <ostream>

TEST_CASE("Defender suite assigns the configured repetitions to every phase") {
    using namespace mod::combat_tests::suite;
    for (const auto repeats : {1u, 2u, 3u, 5u, MaxRepeats}) {
        CHECK_FALSE(ForRun(0, repeats));
        CHECK_FALSE(ForRun(Runs(repeats) + 1, repeats));
        for (std::uint32_t phase = 0; phase < Phases.size(); ++phase)
            for (std::uint32_t repeat = 1; repeat <= repeats; ++repeat) {
                const auto run = phase * repeats + repeat;
                CHECK(PhaseIndex(run, repeats) == phase);
                CHECK(Repetition(run, repeats) == repeat);
                REQUIRE(ForRun(run, repeats));
                CHECK(ForRun(run, repeats)->fixture == Phases[phase].fixture);
            }
    }
    CHECK(DefaultRepeats == 1);
    CHECK(Runs(DefaultRepeats) == 13);
    CHECK(ActiveSeconds(DefaultRepeats) == 735);
    CHECK(ActiveSeconds(3) == 2205);
    CHECK(ActiveSeconds(5) == 3675);
    for (const auto invalid : {0u, MaxRepeats + 1}) {
        CHECK_FALSE(ValidRepeats(invalid));
        CHECK(Runs(invalid) == 0);
        CHECK_FALSE(ForRun(1, invalid));
        CHECK(ActiveSeconds(invalid) == 0);
    }
}

TEST_CASE("Selected phases retain fixture identities, repetition order and exact duration") {
    using namespace mod::combat_tests::suite;
    for (const auto selection :
         {Select(Fixture::Area), Select(Fixture::Archer) | Select(Fixture::Projectile), RangedPhases}) {
        REQUIRE(ValidSelection(selection));
        for (const auto repeats : {1u, 2u, 5u}) {
            std::uint32_t run = 1;
            float duration = 0;
            for (std::uint32_t phase = 0; phase < Phases.size(); ++phase) {
                if (!(selection & (1u << phase)))
                    continue;
                for (std::uint32_t repetition = 1; repetition <= repeats; ++repetition) {
                    REQUIRE(ForRun(run, repeats, selection));
                    CHECK(ForRun(run, repeats, selection)->fixture == Phases[phase].fixture);
                    CHECK(PhaseIndex(run, repeats, selection) == phase);
                    CHECK(Repetition(run, repeats) == repetition);
                    duration += Phases[phase].seconds;
                    ++run;
                }
            }
            CHECK(Runs(repeats, selection) == run - 1);
            CHECK(ActiveSeconds(repeats, selection) == duration);
            CHECK_FALSE(ForRun(run, repeats, selection));
        }
    }
    CHECK(Runs(1, RangedPhases) == 4);
    CHECK(ActiveSeconds(1, RangedPhases) == 180);
    for (const auto selection : {0u, AllPhases + 1}) {
        CHECK_FALSE(ValidSelection(selection));
        CHECK(Runs(1, selection) == 0);
        CHECK(ActiveSeconds(1, selection) == 0);
        CHECK_FALSE(ForRun(1, 1, selection));
    }
}

TEST_CASE("Failed tactical execution never receives a successful coverage explanation") {
    using namespace mod::combat_tests::suite;
    Evidence evidence{true, true, true};
    CHECK(AssessTactical(evidence, nullptr) == Verdict::Failed);
    CHECK(std::string_view(TacticalReason(evidence, nullptr)).find("suspended") != std::string_view::npos);
    evidence.suspended = false;
    evidence.completed = false;
    CHECK(AssessTactical(evidence, nullptr) == Verdict::Failed);
    CHECK(std::string_view(TacticalReason(evidence, nullptr)).find("did not complete") != std::string_view::npos);
    evidence.completed = true;
    CHECK(AssessTactical(evidence, "Missing cast") == Verdict::Inconclusive);
    CHECK(std::string_view(TacticalReason(evidence, "Missing cast")) == "Missing cast");
    CHECK_FALSE(BashCoverage(Fixture::LowStamina));
    CHECK_FALSE(BashCoverage(Fixture::RecoveryWaves));
    CHECK(BashCoverage(Fixture::Sustained));
    CHECK(Template(Loadout::Projectile) != Template(Loadout::Sword));
}

TEST_CASE("Suite fixtures dispatch once per scheduled step and never clamp during ordinary phases") {
    using namespace mod::combat_tests::suite;
    CHECK(StepDue(Fixture::LowStamina, 0, 0));
    CHECK_FALSE(StepDue(Fixture::LowStamina, 1, 50));
    CHECK_FALSE(StepDue(Fixture::RecoveryWaves, 1, 19.9f));
    CHECK(StepDue(Fixture::RecoveryWaves, 1, 20));
    CHECK(StepDue(Fixture::RecoveryWaves, 2, 40));
    CHECK_FALSE(StepDue(Fixture::RecoveryWaves, 3, 60));
    CHECK_FALSE(StepDue(Fixture::Reposition, 0, 0));
    CHECK(StepDue(Fixture::Reposition, 0, 20));
    CHECK(StepDue(Fixture::Reposition, 1, 40));
    CHECK_FALSE(StepDue(Fixture::Reposition, 2, 60));
    CHECK_FALSE(StepDue(Fixture::Sustained, 0, 60));
    CHECK_FALSE(StepDue(Fixture::Approach, 0, 60));
    CHECK_FALSE(StepDue(Fixture::Endurance, 0, 120));
}

TEST_CASE("Suite verdict requires defense evidence and verified restoration") {
    using namespace mod::combat_tests::suite;
    Evidence evidence{true, true, false, 0, 5, 1, 1, true};
    CHECK(Assess(evidence) == Verdict::Passed);
    evidence.blocked = 0;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
    evidence.suspended = true;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.suspended = false;
    evidence.restored = false;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.restored = true;
    evidence.timeouts = 1;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.timeouts = 0;
    evidence.completed = false;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.completed = true;
    evidence.blocked = 1;
    evidence.readable = false;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
    evidence.readable = true;
    evidence.guards = 0;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
    evidence.guards = 1;
    evidence.fixtureComplete = false;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
}

TEST_CASE("Suite permits only explicitly recovered interruption timeouts") {
    using namespace mod::combat_tests::suite;
    Evidence evidence{true, true, false, 1, 5, 1, 1, true};
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.recoveredInterruptTimeouts = 1;
    CHECK(Assess(evidence) == Verdict::Passed);
    evidence.timeouts = 2;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.timeouts = 1;
    evidence.suspended = true;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.suspended = false;
    evidence.restored = false;
    CHECK(Assess(evidence) == Verdict::Failed);
    evidence.restored = true;
    evidence.recoveredInterruptTimeouts = 2;
    CHECK(Assess(evidence) == Verdict::Failed);
}

TEST_CASE("Enabled defense experiments require actual coverage before passing") {
    using namespace mod::combat_tests::suite;
    Evidence evidence{true, true, false, 0, 5, 1, 1, true};
    evidence.nativeBashRequired = true;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
    evidence.completedBashes = 1;
    CHECK(Assess(evidence) == Verdict::Passed);
    evidence.budgetRequired = true;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
    evidence.budgetDecisions = 1;
    CHECK(Assess(evidence) == Verdict::Passed);
}
