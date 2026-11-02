#include <doctest/doctest.h>
#include "combat/actions/Attack.h"

using namespace mod::combat_actions;
namespace rules = mod::combat_actions::attack_rules;

namespace {
    const AttackSample idle{.idle = true, .recoveryReadable = true};
    const AttackSample swinging{.normalMelee = true, .recoveryReadable = true};

    Receipt Accept(Intent intent) {
        CHECK(intent == Intent::RightAttack);
        return {Dispatch::Accepted};
    }
}

TEST_CASE("Right-hand normal intent rejects every other resolved action") {
    CHECK(AllowedRightAttackEvent("attackStart"));
    CHECK_FALSE(AllowedRightAttackEvent("bashStart"));
    CHECK_FALSE(AllowedRightAttackEvent("bashPowerStart"));
    CHECK_FALSE(AllowedRightAttackEvent("attackPowerStartRight"));
    CHECK_FALSE(AllowedRightAttackEvent("attackStartSprint"));
    CHECK_FALSE(AllowedRightAttackEvent("attackStartLeftHand"));
    CHECK_FALSE(AllowedRightAttackEvent("attackStartH2HRight"));
    CHECK_FALSE(AllowedRightAttackEvent(""));
}

TEST_CASE("Synchronous normal attack graph evidence survives dispatch and needs stable cleanup") {
    RightAttackExecution action;
    action.Start(1, 2, 3, [&](Intent intent) {
        CHECK(action.Request(1, 2) == 3);
        action.Record(1, 2, 3, AttackEvent::Started);
        action.Record(1, 2, 3, AttackEvent::Ended);
        return Accept(intent);
    });
    const auto started = action.Observe(idle, 0.1f);
    CHECK(started.step == rules::Step::Observed);
    CHECK(started.graphStart);
    CHECK(started.graphEnd);
    CHECK_FALSE(started.nativeStart);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    const auto done = action.Observe(idle, 0.1f);
    CHECK(done.step == rules::Step::Finished);
    CHECK_FALSE(done.recoveryCompletion);
    CHECK(action.Request(1, 2) == 0);
}

TEST_CASE("Explicit final animation and stable idle do not require an MCO variable") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    action.Record(1, 2, 3, AttackEvent::Started);
    action.Observe({}, 0.1f);
    action.Record(1, 2, 3, AttackEvent::Ended);
    CHECK(action.Observe({.idle = true}, 0.1f).step == rules::Step::None);
    CHECK(action.Observe({.idle = true}, 0.1f).step == rules::Step::Finished);
}

TEST_CASE("Native-only acknowledgement needs a witnessed recovery cycle before idle can complete") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    CHECK(action.Observe(swinging, 0.1f).step == rules::Step::Observed);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    action.Observe({.recoveryReadable = true, .recovering = true}, 0.1f);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    const auto result = action.Observe(idle, 0.1f);
    CHECK(result.step == rules::Step::Finished);
    CHECK(result.recoveryCompletion);
}

TEST_CASE("A preexisting recovery flag alone cannot complete a native attack") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    action.Observe({.normalMelee = true, .recoveryReadable = true, .recovering = true}, 0.1f);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    CHECK(action.Observe(idle, rules::CompletionTimeout).step == rules::Step::Fault);
    CHECK(action.Request(1, 2) == 3);
}

TEST_CASE("Graph-owned attacks require a final event even when recovery and idle look complete") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    action.Record(1, 2, 3, AttackEvent::Started);
    action.Observe(swinging, 0.1f);
    action.Observe({.recoveryReadable = true, .recovering = true}, 0.1f);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    CHECK(action.Observe(idle, rules::CompletionTimeout).step == rules::Step::Fault);
}

TEST_CASE("A bash or power attack taking over cannot acknowledge a normal attack") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    action.Record(1, 2, 3, AttackEvent::ConflictingStarted);
    CHECK(action.Observe({.conflicting = true}, 0.1f).step == rules::Step::None);
    action.Record(1, 2, 3, AttackEvent::ConflictingEnded);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::Interrupted);
    CHECK(action.Phase() == rules::Phase::Idle);
}

TEST_CASE("An unrelated conflicting stop cannot finish a normal attack") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    action.Record(1, 2, 3, AttackEvent::Started);
    action.Observe(swinging, 0.1f);
    CHECK_FALSE(action.Record(1, 2, 3, AttackEvent::ConflictingEnded));
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::None);
}

TEST_CASE("Attack interruption and teardown retain evidence until cleanup") {
    RightAttackExecution action;
    action.Start(1, 2, 3, Accept);
    action.Teardown();
    action.Record(1, 2, 3, AttackEvent::Started);
    action.Observe(swinging, 0.1f);
    action.Record(1, 2, 3, AttackEvent::Ended);
    action.Observe(idle, 0.1f);
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::Interrupted);
    CHECK(action.Request(1, 2) == 0);
}

TEST_CASE("Unknown attack dispatch and start timeouts retain ownership and never replay") {
    RightAttackExecution action;
    unsigned calls = 0;
    action.Start(1, 2, 3, [&](Intent) {
        ++calls;
        return Receipt{Dispatch::Unconfirmed};
    });
    CHECK(action.Observe(idle, 0.1f).step == rules::Step::Fault);
    CHECK(action
              .Start(1, 2, 4,
                     [&](Intent) {
                         ++calls;
                         return Receipt{Dispatch::Accepted};
                     })
              .outcome == Dispatch::Rejected);
    CHECK(calls == 1);
    CHECK(action.Request(1, 2) == 3);
    action.Reset();
    action.Start(1, 9, 1, Accept);
    CHECK_FALSE(action.Record(1, 2, 3, AttackEvent::Started));
    CHECK(action.Observe(idle, rules::StartTimeout).step == rules::Step::Fault);
    CHECK(action.Request(1, 9) == 1);
}
