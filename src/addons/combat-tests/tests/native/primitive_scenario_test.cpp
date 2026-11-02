#include <doctest/doctest.h>
#include <initializer_list>
#include "PrimitiveScenario.h"

using namespace mod::combat_tests::primitive;
using yliwf::testing::DefenderObservation;

TEST_CASE("Direct guard scenario emits each request once and passes only with release evidence") {
    Scenario scenario;
    DefenderObservation evidence;
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Guard);
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Wait);
    evidence.guardAccepted = 1;
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Wait);
    evidence.guardObserved = 1;
    evidence.guardPhase = 2;
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Wait);
    CHECK(scenario.Advance(1, Kind::GuardRelease, evidence) == Step::Release);
    evidence.releaseAccepted = 1;
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Wait);
    evidence.releaseObserved = 1;
    evidence.guardPhase = 0;
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Passed);
}

TEST_CASE("Direct bash scenario requires owned completion rather than acceptance or raw graph counters") {
    for (const auto kind : {Kind::Bash, Kind::PowerBash}) {
        Scenario scenario;
        DefenderObservation evidence;
        scenario.Advance(0.1f, kind, evidence);
        evidence.guardObserved = 1;
        evidence.guardPhase = 2;
        scenario.Advance(0.1f, kind, evidence);
        CHECK(scenario.Advance(1, kind, evidence) == Step::Bash);
        evidence.nativeBashAccepted = evidence.nativeBashObserved = evidence.bashGraphStarts = 1;
        CHECK(scenario.Advance(0.1f, kind, evidence) == Step::Wait);
        evidence.bashGraphEnds = 1;
        CHECK(scenario.Advance(0.1f, kind, evidence) == Step::Wait);
        evidence.nativeBashCompleted = 1;
        CHECK(scenario.Advance(0.1f, kind, evidence) == Step::Passed);
    }
}

TEST_CASE("Rejected or unavailable direct guard cannot become a pass or automatic retry") {
    for (const bool rejected : {false, true}) {
        Scenario scenario;
        DefenderObservation evidence;
        scenario.Advance(0.1f, Kind::GuardRelease, evidence);
        evidence.guardRejected = rejected;
        CHECK(scenario.Advance(8, Kind::GuardRelease, evidence) == Step::Inconclusive);
        CHECK(scenario.Advance(1, Kind::GuardRelease, evidence) == Step::Inconclusive);
    }
}

TEST_CASE("Interruption, ownership loss and host rejection remain inconclusive") {
    Scenario scenario;
    DefenderObservation evidence;
    scenario.Advance(0.1f, Kind::Bash, evidence);
    evidence.guardObserved = 1;
    evidence.guardPhase = 2;
    scenario.Advance(0.1f, Kind::Bash, evidence);
    scenario.Advance(1, Kind::Bash, evidence);
    evidence.nativeBashInterrupted = 1;
    CHECK(scenario.Advance(0.1f, Kind::Bash, evidence) == Step::Inconclusive);
    scenario = {};
    scenario.Advance(0.1f, Kind::Bash, evidence);
    scenario.Advance(0.1f, Kind::Bash, evidence);
    evidence.guardPhase = 0;
    CHECK(scenario.Advance(1, Kind::Bash, evidence) == Step::Inconclusive);
    scenario = {};
    scenario.Reject();
    CHECK(scenario.Advance(1, Kind::Bash, evidence) == Step::Inconclusive);
}

TEST_CASE("Unresolved primitive ownership fails instead of treating an idle phase as completion") {
    Scenario scenario;
    DefenderObservation evidence;
    scenario.Advance(0.1f, Kind::GuardRelease, evidence);
    evidence.actionFault = 1;
    CHECK(scenario.Advance(0.1f, Kind::GuardRelease, evidence) == Step::Failed);
    CHECK(scenario.Advance(8, Kind::GuardRelease, {}) == Step::Failed);
}
