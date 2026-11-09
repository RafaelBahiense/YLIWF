#include <doctest/doctest.h>
#include "diagnostics/SpellDiagnosticRules.h"
#include <string_view>
using namespace mod::combat_tests::spell_diagnostic_rules;

TEST_CASE("Spell diagnostic waits for summoned casters to arrive and load before casting") {
    CHECK(ObserveArrival(false, false, 0) == Arrival::Wait);
    CHECK(ObserveArrival(false, true, 1) == Arrival::Wait);
    CHECK(ObserveArrival(true, false, 1) == Arrival::Wait);
    CHECK(ObserveArrival(true, true, 1) == Arrival::Ready);
    CHECK(ObserveArrival(true, false, 15) == Arrival::Failed);
    CHECK(ObserveArrival(false, true, 15) == Arrival::Failed);
    CHECK(ObserveArrival(true, true, -1) == Arrival::Failed);
}

TEST_CASE("Spell diagnostic does not advance until its previous effect is cleared") {
    CHECK(ObserveCleanup(false, 0) == Cleanup::Wait);
    CHECK(ObserveCleanup(false, .9f) == Cleanup::Wait);
    CHECK(ObserveCleanup(true, .9f) == Cleanup::Complete);
    CHECK(ObserveCleanup(false, 1) == Cleanup::Failed);
    CHECK(ObserveCleanup(true, 1) == Cleanup::Complete);
    CHECK(ObserveCleanup(true, -1) == Cleanup::Failed);
}

TEST_CASE("Spell diagnostic compares both spells with protection on and off") {
    CHECK(Cases.size() == 4);
    CHECK(Cases[0].control);
    CHECK(Cases[0].invulnerable);
    CHECK_FALSE(Cases[1].control);
    CHECK(Cases[1].invulnerable);
    CHECK(Cases[2].control);
    CHECK_FALSE(Cases[2].invulnerable);
    CHECK_FALSE(Cases[3].control);
    CHECK_FALSE(Cases[3].invulnerable);
}

TEST_CASE("Spell diagnostic distinguishes detector misses from immunity and missing controls") {
    std::array<Evidence, 4> evidence{};
    CHECK(std::string_view(Finding(evidence)).starts_with("Incomplete"));
    for (auto& row : evidence)
        row.dispatched = true;
    CHECK(std::string_view(Finding(evidence)).starts_with("Control application missing"));
    evidence[0].hasEffect = evidence[2].hasEffect = true;
    CHECK(std::string_view(Finding(evidence)).starts_with("Control applies, but Challenge is absent"));
    evidence[3].baseEntry = true;
    CHECK(std::string_view(Finding(evidence)).find("detector missed") != std::string_view::npos);
    evidence[3].strictEntry = true;
    CHECK(std::string_view(Finding(evidence)).find("depends on invulnerability") != std::string_view::npos);
    evidence[1].hasEffect = evidence[1].strictEntry = true;
    CHECK(std::string_view(Finding(evidence)).find("both conditions") != std::string_view::npos);
}
