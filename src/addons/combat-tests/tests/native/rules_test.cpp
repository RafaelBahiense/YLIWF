#include <doctest/doctest.h>
#include "Rules.h"
#include <limits>
#include <array>
#include <string_view>

TEST_CASE("Test start remains available after a clean failure but blocks retained recovery with a reason") {
    using namespace mod::test_rules;
    CHECK(BlockStart(true, false, false, false) == StartBlock::None);
    CHECK(BlockStart(true, false, false, true) == StartBlock::Recovery);
    CHECK(BlockStart(true, true, false, true) == StartBlock::Active);
    CHECK(BlockStart(true, false, true, false) == StartBlock::Unsupported);
    CHECK(BlockStart(false, false, false, false) == StartBlock::Loading);
    CHECK(std::string_view(StartReason(StartBlock::Recovery)).find("pre-test save") != std::string_view::npos);
    CHECK(std::string_view(StartReason(StartBlock::None)).empty());
}

TEST_CASE("Combat recovery accepts the legacy prefix and validates native guard ownership") {
    using namespace mod::test_rules;
    Plan original{.subject = 1, .health = 100, .generation = 2};
    std::array<std::uint8_t, sizeof(Plan)> bytes{};
    std::memcpy(bytes.data(), &original, sizeof(original));
    const auto old = DecodePlan(1, std::span(bytes).first(44));
    REQUIRE(old);
    CHECK(old->subject == 1);
    CHECK(old->mode == 0);
    CHECK(old->guardPhase == 0);
    CHECK(Valid(*old));
    CHECK_FALSE(DecodePlan(1, bytes));
    CHECK_FALSE(DecodePlan(4, bytes));
    CHECK_FALSE(DecodePlan(2, std::span(bytes).first(44)));
    REQUIRE(DecodePlan(2, std::span(bytes).first(52)));
    REQUIRE(DecodePlan(3, bytes));
    auto guarded = original;
    guarded.mode = 3;
    guarded.guardPhase = 3;
    guarded.origin = 10;
    CHECK(Valid(guarded));
    guarded.mode = 1;
    CHECK_FALSE(Valid(guarded));
    guarded.mode = 3;
    guarded.guardPhase = 5;
    CHECK_FALSE(Valid(guarded));
}

TEST_CASE("Recovery rechecks require restored temporary changes and no pending combat callback") {
    using namespace mod::test_rules;
    CHECK(CanRecheckRecovery(true, false, false, true, true, false));
    CHECK_FALSE(CanRecheckRecovery(false, false, false, true, true, false));
    CHECK_FALSE(CanRecheckRecovery(true, true, false, true, true, false));
    CHECK_FALSE(CanRecheckRecovery(true, false, true, true, true, false));
    CHECK_FALSE(CanRecheckRecovery(true, false, false, false, true, false));
    CHECK_FALSE(CanRecheckRecovery(true, false, false, true, false, false));
    CHECK_FALSE(CanRecheckRecovery(true, false, false, true, true, true));
}

TEST_CASE("Combat hit observations isolate the test pair and identify interference") {
    using namespace mod::test_rules;
    CHECK(ClassifyHit(1, 2, 1, 2) == HitKind::Incoming);
    CHECK(ClassifyHit(1, 2, 2, 1) == HitKind::Outgoing);
    CHECK(ClassifyHit(1, 2, 1, 3) == HitKind::Foreign);
    CHECK(ClassifyHit(1, 2, 2, 3) == HitKind::Foreign);
    CHECK(ClassifyHit(1, 2, 3, 4) == HitKind::Ignore);
    CHECK(ClassifyHit(1, 2, 1, 0) == HitKind::Ignore);
    CHECK(ClassifyHit(0, 2, 0, 2) == HitKind::Ignore);
    CHECK(ClassifyHit(1, 1, 1, 1) == HitKind::Ignore);
}

TEST_CASE("Combat startup waits for deferred equipment and both combat acknowledgements") {
    using namespace mod::test_rules;
    CHECK(ObserveStart(0, false, false, false) == StartDecision::Wait);
    CHECK(ObserveStart(1, false, true, true) == StartDecision::Wait);
    CHECK(ObserveStart(1, true, false, true) == StartDecision::Wait);
    CHECK(ObserveStart(1, true, true, false) == StartDecision::Wait);
    CHECK(ObserveStart(1, true, true, true) == StartDecision::Ready);
    CHECK(ObserveStart(StartTimeout, false, true, true) == StartDecision::Failed);
    CHECK(ObserveStart(StartTimeout, true, false, true) == StartDecision::Failed);
    CHECK(ObserveStart(StartTimeout, true, true, false) == StartDecision::Failed);
    CHECK(ObserveStart(StartTimeout, true, true, true) == StartDecision::Ready);
    CHECK(ObserveStart(std::numeric_limits<float>::quiet_NaN(), true, true, true) == StartDecision::Failed);
}

TEST_CASE("Combat tests count only bounded active time") {
    CHECK(mod::test_rules::ActiveDelta(0.1f, false) == doctest::Approx(0.1));
    CHECK(mod::test_rules::ActiveDelta(10, false) == doctest::Approx(0.5));
    CHECK(mod::test_rules::ActiveDelta(10, true) == 0);
    CHECK(mod::test_rules::ActiveDelta(-1, false) == 0);
    CHECK(mod::test_rules::ActiveDelta(std::numeric_limits<float>::quiet_NaN(), false) == 0);
}

TEST_CASE("Opponent protection requires unchanged health and a living invulnerable actor") {
    using mod::test_rules::ProtectionPassed;
    CHECK(ProtectionPassed(true, true, 100, 100));
    CHECK_FALSE(ProtectionPassed(true, true, 100, 99));
    CHECK_FALSE(ProtectionPassed(true, false, 100, 100));
    CHECK_FALSE(ProtectionPassed(false, true, 100, 100));
    CHECK_FALSE(ProtectionPassed(true, true, 0, 0));
    CHECK_FALSE(ProtectionPassed(true, true, 100, std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("Recovery plans reject invalid actors roles flags and resource values") {
    using mod::test_rules::Valid;
    mod::test_rules::Plan plan{.subject = 0x123, .health = 100, .stamina = 50, .magicka = 20, .generation = 1};
    CHECK(Valid(plan));
    plan.enemy = plan.subject;
    CHECK_FALSE(Valid(plan));
    plan.enemy = 0;
    plan.role = 7;
    CHECK_FALSE(Valid(plan));
    plan.role = 0;
    plan.restoreIssued = 2;
    CHECK_FALSE(Valid(plan));
    plan.restoreIssued = 0;
    plan.magicka = -1;
    CHECK_FALSE(Valid(plan));
    plan.magicka = 20;
    plan.origin = plan.subject;
    CHECK_FALSE(Valid(plan));
}

TEST_CASE("Combat continuations require the current active unstopped session") {
    using mod::test_rules::CurrentTicket;
    CHECK(CurrentTicket(2, 2, true, true, false));
    CHECK_FALSE(CurrentTicket(2, 3, true, true, false));
    CHECK_FALSE(CurrentTicket(0, 0, true, true, false));
    CHECK_FALSE(CurrentTicket(2, 2, false, true, false));
    CHECK_FALSE(CurrentTicket(2, 2, true, false, false));
    CHECK_FALSE(CurrentTicket(2, 2, true, true, true));
}

TEST_CASE("Position restoration waits for an issued move without replaying it") {
    using namespace mod::test_rules;
    CHECK(RestorePosition(false, false) == PositionAction::Move);
    CHECK(RestorePosition(false, true) == PositionAction::Wait);
    CHECK(RestorePosition(true, true) == PositionAction::Complete);
    CHECK(RestorePosition(true, false) == PositionAction::Complete);
}

TEST_CASE("Role restoration preserves changes made by the user during the test") {
    CHECK(mod::test_rules::RestoreRole(true, 5));
    CHECK_FALSE(mod::test_rules::RestoreRole(false, 5));
    CHECK_FALSE(mod::test_rules::RestoreRole(true, 2));
}
