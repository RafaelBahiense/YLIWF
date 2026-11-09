#include <doctest/doctest.h>
#include "testing/Selection.h"
#include "testing/Definition.h"
#include "testing/CaseCatalog.h"
#include "testing/RunPlan.h"
#include "testing/SuiteSession.h"

using namespace mod::combat_tests::testing;

TEST_CASE("Tags select cases independently of their IDs and named selections carry separate run options") {
    const Tag ranged{"ranged"}, acceptance{"acceptance"}, missing{"missing"};
    Suite suite{"Custom"};
    suite.Case(TestCase{31, "Last bit"}.Tag(ranged).Tag(ranged).Tag(acceptance).Phase({"Fight", 1}));
    suite.Case(TestCase{7, "Melee"}.Tag(acceptance).Phase({"Fight", 2}));
    suite.Case(TestCase{2, "Other"}.Phase({"Fight", 3}));
    CaseCatalog catalog{suite};
    REQUIRE(catalog.Valid());
    CHECK(suite.cases[0].tags.size() == 2);
    CHECK(catalog.Tagged(ranged) == (1u << 31));
    CHECK(catalog.Tagged(acceptance) == ((1u << 31) | (1u << 7)));
    CHECK(catalog.Tagged(missing) == 0);
    const auto options = catalog.Select({"Acceptance", acceptance, true}, 3);
    CHECK(options.repeats == 3);
    CHECK(options.selection == catalog.Tagged(acceptance));
    CHECK(options.expandVariations);
    RunPlan run{catalog, options};
    CHECK(run.Valid());
    CHECK(run.Runs() == 6);
    CHECK(run.At(1).id == 31);
    CHECK_FALSE(run.At(1).parameterized);
}

TEST_CASE("A last-bit case can expand its own variations without another selected case") {
    Suite suite{"Last bit"};
    suite.Case(TestCase{31, "Comparison"}.Variation(4, "A").Variation(8, "B").Phase({"Fight", 2}));
    CaseCatalog catalog{suite};
    REQUIRE(catalog.Valid());
    CHECK(catalog.HasVariations(1u << 31));
    CHECK_FALSE(catalog.HasVariations(0));
    RunPlan expanded{catalog, {2, 1u << 31, true}};
    RunPlan single{catalog, {2, 1u << 31, false}};
    CHECK(expanded.Runs() == 4);
    CHECK(expanded.Seconds() == 8);
    CHECK(expanded.CaseRuns(31) == 4);
    CHECK(single.Runs() == 2);
    CHECK(single.Seconds() == 4);
    CHECK(single.CaseRepeats(31) == 2);
    CHECK(expanded.At(1).variant == 4);
    CHECK(expanded.At(2).variant == 8);
    REQUIRE(expanded.VariantForRun(2));
    CHECK(expanded.VariantForRun(2)->name == "B");
    CHECK(single.VariantForRun(1) == nullptr);
}

TEST_CASE("Declared variation configuration starts from the captured baseline on every repetition") {
    struct Settings {
        int value = 10;
    };

    Suite suite{"Configuration"};
    suite.Case(TestCase{7, "First"}
                   .Variation<Settings>(1, "Increment", [](Settings& settings) { ++settings.value; })
                   .Variation<Settings>(2, "Replace", [](Settings& settings) { settings.value = 20; })
                   .Phase({"Fight", 1}));
    suite.Case(TestCase{31, "Unchanged"}.Phase({"Fight", 1}));
    CaseCatalog catalog{suite};
    RunPlan run{catalog, {2, catalog.Mask(), true}};
    SuiteSession session;
    Settings baseline;
    REQUIRE(session.Begin(suite, baseline));
    baseline.value = 900;
    for (const auto ordinal : {1u, 2u, 3u, 4u, 5u}) {
        const auto next = run.At(ordinal);
        REQUIRE(session.BeginCase(next.id, next.repetition, ordinal,
                                  next.parameterized ? std::optional{next.variant} : std::nullopt));
        CHECK(session.context.Settings<Settings>()->value == (ordinal == 5 ? 10 : ordinal % 2 ? 11 : 20));
        REQUIRE(session.RestoreCase().state == State::Ready);
    }
    Settings settings;
    REQUIRE(run.Configure(1, settings));
    CHECK(settings.value == 11);
    int wrongType = 4;
    CHECK_FALSE(run.Configure(1, wrongType));
    CHECK(wrongType == 4);
    SuiteSession wrong;
    REQUIRE(wrong.Begin(suite, 4));
    CHECK_FALSE(wrong.BeginCase(7, 1, 1, 1));
    CHECK_FALSE(session.BeginCase(7, 1, 7, 999));
}

TEST_CASE("Empty tag declarations fail catalog validation") {
    Suite suite{"Invalid tag"};
    suite.Case(TestCase{0, "Case"}.Tag(Tag{""}).Phase({"Fight", 1}));
    CHECK_FALSE(suite.Valid());
    CaseCatalog catalog{suite};
    CHECK_FALSE(catalog.Valid());
    CHECK(catalog.Tagged(Tag{""}) == 0);
}
