#include <doctest/doctest.h>
#include "PrimitiveDriver.h"
#include <initializer_list>

using namespace primitive_test;
using namespace mod::combat_tests::testing;
using yliwf::testing::DefenderObservation;

TEST_CASE("Guard phases emit each request once and pass only with owned release evidence") {
    Driver test;
    REQUIRE(test.Begin(Kind::GuardRelease));
    REQUIRE(test.requests.size() == 1);
    CHECK(test.requests[0] == Action::Guard);
    DefenderObservation evidence;
    evidence.guardAccepted = 1;
    test.Tick(.1f, evidence);
    CHECK_FALSE(test.session.context.completion);
    evidence.guardObserved = 1;
    evidence.guardPhase = 2;
    test.Tick(.1f, evidence);
    CHECK(test.session.current.Index() == 1);
    test.Tick(1, evidence);
    REQUIRE(test.requests.size() == 2);
    CHECK(test.requests[1] == Action::Release);
    evidence.releaseAccepted = 1;
    test.Tick(.1f, evidence);
    CHECK_FALSE(test.session.context.completion);
    evidence.releaseObserved = 1;
    evidence.guardPhase = 0;
    test.Tick(.1f, evidence);
    REQUIRE(test.session.context.completion);
    CHECK(test.session.context.completion->verdict == Verdict::Passed);
    CHECK(test.session.current.Done());
    REQUIRE(test.session.RestoreCase().state == State::Ready);
    CHECK(test.disposed == 1);
}

TEST_CASE("Bash phases require owned completion rather than acceptance or raw graph counters") {
    for (const auto kind : {Kind::Bash, Kind::PowerBash}) {
        Driver test;
        REQUIRE(test.Begin(kind));
        DefenderObservation evidence;
        evidence.guardObserved = 1;
        evidence.guardPhase = 2;
        test.Tick(.1f, evidence);
        test.Tick(1, evidence);
        REQUIRE(test.requests.size() == 2);
        CHECK(test.requests[1] == (kind == Kind::Bash ? Action::Bash : Action::PowerBash));
        evidence.nativeBashAccepted = evidence.nativeBashObserved = evidence.bashGraphStarts = evidence.bashGraphEnds =
            1;
        test.Tick(.1f, evidence);
        CHECK_FALSE(test.session.context.completion);
        evidence.nativeBashCompleted = 1;
        test.Tick(.1f, evidence);
        REQUIRE(test.session.context.completion);
        CHECK(test.session.context.completion->verdict == Verdict::Passed);
    }
}

TEST_CASE("Unavailable and rejected guards end inconclusively without issuing a follow-up or replay") {
    for (const bool rejected : {false, true}) {
        Driver test;
        REQUIRE(test.Begin(Kind::GuardRelease));
        DefenderObservation evidence;
        evidence.guardRejected = rejected;
        for (int i = 0; i < 8; ++i)
            test.Tick(1, evidence);
        REQUIRE(test.session.context.completion);
        CHECK(test.session.context.completion->verdict == Verdict::Inconclusive);
        CHECK(test.session.current.Done());
        CHECK(test.requests.size() == 1);
        REQUIRE(test.session.RestoreCase().state == State::Ready);
        CHECK(test.disposed == 1);
        REQUIRE(test.session.BeginCase(0, 2, 2));
        CHECK_FALSE(test.session.context.completion);
    }
}

TEST_CASE("Guard ownership loss and host rejection stop later phase requests") {
    Driver lost;
    REQUIRE(lost.Begin(Kind::Bash));
    DefenderObservation evidence;
    evidence.guardObserved = 1;
    evidence.guardPhase = 2;
    lost.Tick(.1f, evidence);
    evidence.guardPhase = 0;
    lost.Tick(.1f, evidence);
    REQUIRE(lost.session.context.completion);
    CHECK(lost.session.context.completion->verdict == Verdict::Inconclusive);
    CHECK(lost.requests.size() == 1);
    Driver rejected;
    rejected.reject = true;
    REQUIRE(rejected.Begin(Kind::Bash));
    rejected.Tick(.1f, {});
    CHECK(rejected.session.current.Done());
    CHECK(rejected.requests.size() == 1);
    REQUIRE(rejected.session.RestoreCase().state == State::Ready);
    CHECK(rejected.disposed == 1);
}

TEST_CASE("Unresolved action ownership fails instead of treating an idle phase as completion") {
    Driver test;
    REQUIRE(test.Begin(Kind::GuardRelease));
    DefenderObservation evidence;
    evidence.actionFault = 1;
    test.Tick(.1f, evidence);
    REQUIRE(test.session.context.completion);
    CHECK(test.session.context.completion->verdict == Verdict::Failed);
    CHECK(test.requests.size() == 1);
}
