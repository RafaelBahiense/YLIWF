#include <doctest/doctest.h>
#include "combat/actions/AnimationEvidence.h"

using mod::combat_actions::AnimationEvidenceMailbox;
using Event = mod::combat_actions::AnimationEvent;

TEST_CASE("Bash evidence rejects events from another actor test generation or request") {
    AnimationEvidenceMailbox box;
    CHECK_FALSE(box.Record(1, 2, 3, Event::Started));
    box.Arm(1, 2, 3);
    CHECK(box.Request(1, 2) == 3);
    CHECK(box.Request(9, 2) == 0);
    CHECK(box.Request(1, 9) == 0);
    CHECK_FALSE(box.Record(9, 2, 3, Event::Started));
    CHECK_FALSE(box.Record(1, 9, 3, Event::Started));
    CHECK_FALSE(box.Record(1, 2, 9, Event::Started));
    CHECK_FALSE(box.Read(1, 2, 3).graphStarted);
    REQUIRE(box.Record(1, 2, 3, Event::Started));
    CHECK(box.Read(1, 2, 3).graphStarted);
}

TEST_CASE("Synchronous bash animation evidence remains available until the main thread observes it") {
    AnimationEvidenceMailbox box;
    box.Arm(1, 2, 3);
    box.Record(1, 2, 3, Event::Ended);
    CHECK_FALSE(box.Read(1, 2, 3).ended);
    box.Record(1, 2, 3, Event::Started);
    box.Record(1, 2, 3, Event::Ended);
    const auto evidence = box.Read(1, 2, 3);
    CHECK(evidence.graphStarted);
    CHECK(evidence.ended);
    CHECK_FALSE(evidence.nativeStarted);
    CHECK_FALSE(evidence.interrupted);
    box.Record(1, 2, 3, Event::Started);
    box.Record(1, 2, 3, Event::Interrupted);
    CHECK(box.Read(1, 2, 3).ended);
    CHECK_FALSE(box.Read(1, 2, 3).interrupted);
}

TEST_CASE("An interrupted bash retains its end requirement and cannot count as normal completion") {
    AnimationEvidenceMailbox box;
    box.Arm(1, 2, 3);
    box.Record(1, 2, 3, Event::Interrupted);
    CHECK_FALSE(box.Read(1, 2, 3).interrupted);
    box.Record(1, 2, 3, Event::Started);
    box.Record(1, 2, 3, Event::Interrupted);
    CHECK(box.Read(1, 2, 3).interrupted);
    CHECK_FALSE(box.Read(1, 2, 3).ended);
    box.Record(1, 2, 3, Event::Ended);
    CHECK(box.Read(1, 2, 3).ended);
    CHECK(box.Read(1, 2, 3).interrupted);
}

TEST_CASE("Rearming and clearing bash evidence invalidates an in-flight old callback key") {
    AnimationEvidenceMailbox box;
    box.Arm(1, 2, 3);
    const auto oldRequest = box.Request(1, 2);
    box.Arm(1, 2, 4);
    CHECK_FALSE(box.Record(1, 2, oldRequest, Event::Started));
    box.NativeStart(1, 2, oldRequest);
    CHECK_FALSE(box.Read(1, 2, 4).nativeStarted);
    box.NativeStart(1, 2, 4);
    box.Record(1, 2, 4, Event::Ended);
    CHECK(box.Read(1, 2, 4).ended);
    box.Arm(1, 9, 1);
    CHECK_FALSE(box.Record(1, 2, 4, Event::Started));
    box.Clear();
    CHECK(box.Request(1, 9) == 0);
    CHECK_FALSE(box.Record(1, 9, 1, Event::Started));
}

TEST_CASE("Teardown before a delayed bash starts marks its eventual cleanup as interruption") {
    AnimationEvidenceMailbox box;
    box.Arm(1, 2, 3);
    box.Teardown(1, 2, 3);
    CHECK(box.Read(1, 2, 3).interrupted);
    CHECK_FALSE(box.Read(1, 2, 3).ended);
    box.Record(1, 2, 3, Event::Started);
    box.Record(1, 2, 3, Event::Ended);
    CHECK(box.Read(1, 2, 3).interrupted);
    CHECK(box.Read(1, 2, 3).ended);
}
