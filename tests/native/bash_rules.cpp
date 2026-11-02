#include <doctest/doctest.h>
#include "combat/actions/BashRules.h"
#include <limits>

using namespace mod::combat_actions::bash_rules;

TEST_CASE("Accepted bashes require observation and two idle samples before returning control") {
    CHECK(Observe(Phase::Pending, .2f, false, true, false, 10) == Step::None);
    CHECK(Observe(Phase::Pending, StartTimeout, false, true, false, 10) == Step::Fault);
    CHECK(Observe(Phase::Pending, .2f, true, false, false, 0) == Step::Observed);
    CHECK(Observe(Phase::Active, .2f, false, true, false, 1) == Step::None);
    CHECK(Observe(Phase::Active, .3f, false, true, true, 2) == Step::None);
    CHECK(Observe(Phase::Active, .4f, false, true, false, 2) == Step::Finished);
    CHECK(Observe(Phase::Active, CompletionTimeout, true, false, false, 0) == Step::Fault);
    CHECK(Observe(Phase::Fault, 0, false, true, false, 10) == Step::Fault);
}

TEST_CASE("Graph bash start acknowledges execution even when native bash state is missed") {
    AnimationEvidence animation;
    animation.graphStarted = true;
    CHECK(Observe(Phase::Pending, .2f, false, true, false, 2, animation) == Step::Observed);
    CHECK(Observe(Phase::Active, .3f, false, true, false, 2, animation) == Step::None);
    animation.ended = true;
    CHECK(Observe(Phase::Active, .8f, false, true, false, 1, animation) == Step::None);
    CHECK(Observe(Phase::Active, .8f, false, true, false, 2, animation) == Step::Finished);
}

TEST_CASE("A complete bash between polls is acknowledged before completion and still requires stable idle") {
    AnimationEvidence animation{true, false, true, false};
    CHECK(Observe(Phase::Pending, .2f, false, true, false, 2, animation) == Step::Observed);
    CHECK(Observe(Phase::Active, .1f, false, true, true, 2, animation) == Step::None);
    CHECK(Observe(Phase::Active, .2f, false, true, false, 1, animation) == Step::None);
    CHECK(Observe(Phase::Active, .3f, false, true, false, 2, animation) == Step::Finished);
}

TEST_CASE("Graph-confirmed bashes cannot complete from premature native idle or replay after missing an end") {
    AnimationEvidence animation{true, false, false, false};
    CHECK(Observe(Phase::Active, .3f, false, true, false, 20, animation) == Step::None);
    CHECK(Observe(Phase::Active, CompletionTimeout, false, true, false, 20, animation) == Step::Fault);
    animation.ended = true;
    CHECK(Observe(Phase::Fault, CompletionTimeout, false, true, false, 20, animation) == Step::Fault);
}

TEST_CASE("Interrupted bash cleanup is separate from successful completion") {
    AnimationEvidence animation{true, false, false, true};
    CHECK(Observe(Phase::Active, .3f, false, true, false, 2, animation) == Step::None);
    animation.ended = true;
    CHECK(Observe(Phase::Active, .4f, false, true, false, 2, animation) == Step::Interrupted);
}
