#include <doctest/doctest.h>
#include "scenarios/DefenderSuite.h"
#include "testing/RunPlan.h"
#include <string_view>
#include <ostream>

using namespace mod::combat_tests::suite;
using namespace mod::combat_tests::testing;

TEST_CASE("Leveled mage is a defender case with outcome assessment outside the default selections") {
    const auto id = static_cast<std::uint32_t>(Fixture::LeveledMage);
    const auto* test = Catalog.Find(id);
    REQUIRE(test);
    REQUIRE(test->Expectations<OutcomeAssessment>());
    CHECK_FALSE(test->Expectations<Requirements>());
    const auto* opponent = test->FirstActor(ActorRole::Opponent);
    REQUIRE(opponent);
    CHECK(opponent->preserveLoadout);
    CHECK_FALSE(opponent->protectedHealth);
    for (const auto* selection : {&DefenderValidation, &DefenderAcceptance, &ProtectionReview})
        CHECK((Catalog.Select(*selection).selection & CaseMask(id)) == 0);
    RunPlan mixed{Catalog, {2, CaseMask(static_cast<std::uint32_t>(Fixture::Sustained)) | CaseMask(id)}};
    CHECK(mixed.Runs() == 4);
    CHECK(mixed.Seconds() == 210);
    CHECK_FALSE(mixed.ForRun(2)->Expectations<OutcomeAssessment>());
    REQUIRE(mixed.ForRun(3)->Expectations<OutcomeAssessment>());
}

TEST_CASE("Tagged defender acceptance retains coverage and comparison timing") {
    const auto options = Catalog.Select(DefenderAcceptance);
    RunPlan run{Catalog, options};
    CHECK(run.Valid());
    CHECK(run.Runs() == 10);
    CHECK(run.Seconds() == 465);
    CHECK((options.selection & Catalog.Tagged(tags::Ranged)) == Catalog.Tagged(tags::Ranged));
    CHECK((options.selection & CaseMask(static_cast<std::uint32_t>(Fixture::Multiple))) != 0);
    CHECK(options.expandVariations);
}

TEST_CASE("Shared run plans assign configured repetitions without duplicating phase runs") {
    for (const auto repeats : {1u, 2u, 3u, 5u, MaxRepeats}) {
        RunPlan run{Catalog, {repeats, Catalog.Mask()}};
        CHECK_FALSE(run.ForRun(0));
        CHECK_FALSE(run.ForRun(run.Runs() + 1));
        for (std::uint32_t index = 0; index < Catalog.Order().size(); ++index)
            for (std::uint32_t repetition = 1; repetition <= repeats; ++repetition) {
                const auto ordinal = index * repeats + repetition;
                CHECK(run.At(ordinal).id == Catalog.Order()[index]);
                CHECK(run.At(ordinal).repetition == repetition);
            }
    }
    CHECK(DefaultRepeats == 1);
    CHECK(RunPlan{Catalog, {1, Catalog.Mask()}}.Runs() == 15);
    CHECK(RunPlan{Catalog, {1, Catalog.Mask()}}.Seconds() == 825);
    CHECK(RunPlan{Catalog, {3, Catalog.Mask()}}.Seconds() == 2475);
    CHECK(RunPlan{Catalog, {5, Catalog.Mask()}}.Seconds() == 4125);
    RunPlan essentials{Catalog, Catalog.Select(DefenderValidation)};
    CHECK(essentials.Runs() == 4);
    CHECK(essentials.Seconds() == 195);
    CHECK(essentials.At(1).id == static_cast<std::uint32_t>(Fixture::Sustained));
    CHECK(essentials.At(2).id == static_cast<std::uint32_t>(Fixture::Heavy));
    CHECK(essentials.At(3).id == static_cast<std::uint32_t>(Fixture::LowStamina));
    CHECK(essentials.At(4).id == static_cast<std::uint32_t>(Fixture::AllyPressure));
    for (const auto invalid : {0u, MaxRepeats + 1}) {
        CHECK_FALSE(ValidRepeats(invalid));
        RunPlan run{Catalog, {invalid, Catalog.Mask()}};
        CHECK(run.Runs() == 0);
        CHECK(run.Seconds() == 0);
        CHECK_FALSE(run.ForRun(1));
    }
}

TEST_CASE("Declared Challenge configurations alternate without encoding variants in case selection") {
    for (const auto repeats : {1u, 2u, 3u, MaxRepeats}) {
        RunPlan run{Catalog, {repeats, CaseMask(static_cast<std::uint32_t>(Fixture::AllyPressure)), true}};
        CHECK(run.Runs() == repeats * 2);
        CHECK(run.Seconds() == repeats * 90);
        for (std::uint32_t pair = 0; pair < repeats; ++pair) {
            const auto first = run.At(pair * 2 + 1), second = run.At(pair * 2 + 2);
            CHECK(first.parameterized);
            CHECK(second.parameterized);
            CHECK(first.id == second.id);
            CHECK(first.repetition == pair + 1);
            yliwf::testing::DefenderTuning on, off;
            REQUIRE(run.Configure(pair * 2 + 1, on));
            REQUIRE(run.Configure(pair * 2 + 2, off));
            CHECK((on.challengeEnabled != 0) == (pair % 2 == 0));
            CHECK(off.challengeEnabled != on.challengeEnabled);
        }
        CHECK_FALSE(run.ForRun(repeats * 2 + 1));
    }
    RunPlan review{Catalog, Catalog.Select(ProtectionReview)};
    CHECK(review.Runs() == 6);
    CHECK(review.Seconds() == 270);
    CHECK_FALSE(review.At(3).parameterized);
    CHECK(review.At(3).id == static_cast<std::uint32_t>(Fixture::Archer));
    CHECK(review.At(6).id == static_cast<std::uint32_t>(Fixture::Multiple));
}

TEST_CASE("Tag selections preserve declaration order and exact duration") {
    for (const auto selection : {CaseMask(static_cast<std::uint32_t>(Fixture::Area)),
                                 CaseMask(static_cast<std::uint32_t>(Fixture::Archer)) |
                                     CaseMask(static_cast<std::uint32_t>(Fixture::Projectile)),
                                 Catalog.Tagged(tags::Ranged), Catalog.Tagged(tags::Validation)}) {
        for (const auto repeats : {1u, 2u, 5u}) {
            RunPlan plan{Catalog, {repeats, selection}};
            REQUIRE(plan.Valid());
            std::uint32_t ordinal = 1;
            float seconds = 0;
            for (const auto id : Catalog.Order()) {
                if (!(selection & CaseMask(id)))
                    continue;
                for (std::uint32_t repetition = 1; repetition <= repeats; ++repetition) {
                    REQUIRE(plan.ForRun(ordinal));
                    CHECK(plan.At(ordinal).id == id);
                    CHECK(plan.At(ordinal).repetition == repetition);
                    seconds += plan.ForRun(ordinal)->Seconds();
                    ++ordinal;
                }
            }
            CHECK(plan.Runs() == ordinal - 1);
            CHECK(plan.Seconds() == seconds);
        }
    }
    RunPlan ranged{Catalog, {1, Catalog.Tagged(tags::Ranged)}};
    CHECK(ranged.Runs() == 4);
    CHECK(ranged.Seconds() == 180);
    CHECK_FALSE(RunPlan{Catalog, {1, 0}}.Valid());
    CHECK_FALSE(RunPlan{Catalog, {1, 1u << 31}}.Valid());
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
    CHECK(AssessTactical(evidence, nullptr) == Verdict::Passed);
    evidence.budgetRequired = true;
    CHECK(Assess(evidence) == Verdict::Inconclusive);
    CHECK(AssessTactical(evidence, nullptr) == Verdict::Inconclusive);
    CHECK(std::string_view(TacticalReason(evidence, nullptr)).find("UND budget") != std::string_view::npos);
    evidence.budgetDecisions = 1;
    CHECK(Assess(evidence) == Verdict::Passed);
    CHECK(AssessTactical(evidence, nullptr) == Verdict::Passed);
}

TEST_CASE("Low stamina validation requires a completed recovery instead of survival alone") {
    using namespace mod::combat_tests::suite;
    Evidence evidence{true, true, false, 0, 5, 1, 1, true};
    evidence.recoveryRequired = true;
    CHECK(Assess(evidence) == Verdict::Failed);
    CHECK(std::string_view(Reason(evidence)).find("recovery never exited") != std::string_view::npos);
    evidence.recoveryExits = 1;
    CHECK(Assess(evidence) == Verdict::Passed);
}
