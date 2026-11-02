#include <doctest/doctest.h>
#include "combat/actions/DispatchRules.h"

using namespace mod::combat_actions;

TEST_CASE("Left action classification separates shield starts from bashes and unknown events") {
    CHECK(ClassifyLeftAction("blockStart") == LeftAction::Block);
    CHECK(ClassifyLeftAction("BLOCKSTART") == LeftAction::Block);
    CHECK(ClassifyLeftAction("bashStart") == LeftAction::Bash);
    CHECK(ClassifyLeftAction("BashPowerStart") == LeftAction::Bash);
    CHECK(ClassifyLeftAction("") == LeftAction::Unknown);
    CHECK(ClassifyLeftAction("blockStop") == LeftAction::Unknown);
    CHECK(ClassifyLeftAction("bashRelease") == LeftAction::Unknown);
    CHECK(ClassifyLeftAction("customAttack") == LeftAction::Unknown);
}

TEST_CASE("Offense suppression preserves native blocks while own guards require a resolved block") {
    CHECK_FALSE(RejectLeftAction(LeftAction::Block, false, true));
    CHECK_FALSE(RejectLeftAction(LeftAction::Block, true, true));
    CHECK(RejectLeftAction(LeftAction::Bash, false, true));
    CHECK_FALSE(RejectLeftAction(LeftAction::Bash, false, false));
    CHECK(RejectLeftAction(LeftAction::Bash, true, false));
    CHECK(RejectLeftAction(LeftAction::Unknown, true, false));
    CHECK_FALSE(RejectLeftAction(LeftAction::Unknown, false, true));
}

TEST_CASE("Completed engine rejection is distinct from accepted and unconfirmed dispatch") {
    CHECK(ClassifyDispatch(true, true, 0, 0) == Dispatch::Accepted);
    CHECK(ClassifyDispatch(true, false, 0, 0) == Dispatch::Rejected);
    CHECK(ClassifyDispatch(false, false, 0, 0) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, 2, 0) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, true, 2, 0) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, true, 0, 1) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, -1, 0) == Dispatch::Unconfirmed);
}

TEST_CASE("Pre-dispatch rejection requires observed engine evidence and a completed failure") {
    CHECK(ClassifyDispatch(true, false, -1, 0, true) == Dispatch::Rejected);
    CHECK(ClassifyDispatch(true, false, -1, 0, false) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(false, false, -1, 0, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, true, -1, 0, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, -1, 1, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, -1, 2, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, 1, 0, true) == Dispatch::Unconfirmed);
    CHECK(ClassifyDispatch(true, false, 2, 0, true) == Dispatch::Unconfirmed);
}

TEST_CASE("Bash intent requires its exact resolved event") {
    CHECK(AllowedBashEvent("bashStart", Intent::Bash));
    CHECK(AllowedBashEvent("bashPowerStart", Intent::PowerBash));
    CHECK_FALSE(AllowedBashEvent("bashPowerStart", Intent::Bash));
    CHECK_FALSE(AllowedBashEvent("bashStart", Intent::PowerBash));
    CHECK_FALSE(AllowedBashEvent("blockStart", Intent::Bash));
    CHECK_FALSE(AllowedBashEvent("bashStart", Intent::Guard));
    CHECK_FALSE(AllowedBashEvent("bashStart", Intent::ReleaseGuard));
    CHECK_FALSE(AllowedBashEvent("", Intent::PowerBash));
}
