#include <doctest/doctest.h>
#include "testing/CaseCatalog.h"
#include "testing/RunPlan.h"

using namespace mod::combat_tests::testing;

TEST_CASE("Case catalogs preserve declaration order without dense case IDs") {
    Suite suite{"Sparse"};
    suite.Case(TestCase{17, "First"}.Phase({"Fight", 1}));
    suite.Case(TestCase{2, "Second"}.Phase({"Fight", 2}));
    suite.Case(TestCase{30, "Third"}.Phase({"Fight", 3}));
    CaseCatalog catalog{suite, 31};
    REQUIRE(catalog.Valid());
    CHECK(catalog.Order() == std::vector<std::uint32_t>{17, 2, 30});
    CHECK(catalog.Mask() == ((1u << 17) | (1u << 2) | (1u << 30)));
    CHECK(catalog.Find(17) == &suite.cases[0]);
    CHECK(catalog.Find(1) == nullptr);
    CHECK(catalog.Find(UINT32_MAX) == nullptr);
    RunPlan plan{suite, catalog.Order(), 1, catalog.Mask()};
    CHECK(plan.Runs() == 3);
    CHECK(plan.At(1).id == 17);
    CHECK(plan.At(2).id == 2);
    CHECK(plan.At(3).id == 30);
}

TEST_CASE("Invalid catalog identities fail without exposing a partially built selection") {
    for (const auto id : {31u, 32u, UINT32_MAX}) {
        Suite suite{"Invalid"};
        suite.Case(TestCase{2, "Valid prefix"}.Phase({"Fight", 1}));
        suite.Case(TestCase{id, "Outside selection"}.Phase({"Fight", 1}));
        CaseCatalog catalog{suite, 31};
        CHECK_FALSE(catalog.Valid());
        CHECK(catalog.Mask() == 0);
        CHECK(catalog.Order().empty());
        CHECK(catalog.Find(2) == nullptr);
    }
    Suite duplicate{"Duplicate"};
    duplicate.Case(TestCase{2, "First"}.Phase({"Fight", 1}));
    duplicate.Case(TestCase{2, "Second"}.Phase({"Fight", 1}));
    CHECK_FALSE(CaseCatalog{duplicate}.Valid());
}

TEST_CASE("Per-case run counts follow declared variations for any selected case") {
    Suite suite{"Variations"};
    suite.Case(TestCase{3, "Unrelated"}.Phase({"Fight", 1}));
    suite.Case(TestCase{19, "Comparison"}.Repeat(3).Variation(8, "A").Variation(9, "B").Phase({"Fight", 1}));
    CaseCatalog catalog{suite};
    const auto mask = catalog.Mask();
    RunPlan expanded{suite, catalog.Order(), 2, mask, true};
    CHECK(expanded.CaseRuns(3) == 2);
    CHECK(expanded.CaseRuns(19) == 6);
    CHECK(expanded.Runs() == 8);
    CHECK(expanded.CaseRuns(1) == 0);
    CHECK(expanded.CaseRuns(UINT32_MAX) == 0);
    RunPlan unexpanded{suite, catalog.Order(), 2, mask, false};
    CHECK(unexpanded.CaseRuns(19) == 3);
    RunPlan deselected{suite, catalog.Order(), 2, 1u << 3, true};
    CHECK(deselected.CaseRuns(19) == 0);
}
