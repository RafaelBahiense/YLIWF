#include <doctest/doctest.h>

#include "Rules.h"

#include <cstring>

namespace {
    using namespace yliwf::sdk;
    using mod::serana_rules::ControllerState;
    using mod::serana_rules::Describe;

    ControllerState FollowingSerana() {
        return {.following = true, .willingToWait = true, .canDismiss = true};
    }

    ControllerState DismissedSerana() {
        return {.dismissed = true, .willingToWait = true, .canDismiss = true};
    }

    TEST_CASE("Normal Follower Commands") {
        auto following = Describe(FollowingSerana());
        REQUIRE_MESSAGE((Supports(following, Command::Follow) && Supports(following, Command::Wait) &&
                         Supports(following, Command::Dismiss)),
                        "Normal Serana commands missing");

        const auto dismissed = Describe(DismissedSerana());
        REQUIRE_MESSAGE((dismissed.state == State::Inactive && !Supports(dismissed, Command::Follow)),
                        "Adapter can recruit inactive Serana");
        REQUIRE_MESSAGE((Reached(dismissed, Command::Dismiss) && !Reached(following, Command::Wait)),
                        "Wrong controller state acknowledged");
    }

    TEST_CASE("Quest Locks") {
        auto controller = FollowingSerana();
        controller.locked = true;
        const auto locked = Describe(controller);

        REQUIRE_MESSAGE((locked.state == State::Following && locked.commands == 0),
                        "Quest-locked Serana can be changed");
        REQUIRE_MESSAGE((std::strstr(locked.reason, "Follow, Wait and Dismiss")),
                        "Quest lock does not explain disabled commands");
    }

    TEST_CASE("Command Restrictions") {
        auto controller = FollowingSerana();
        controller.waiting = true;
        controller.willingToWait = false;
        controller.canDismiss = false;
        const auto restricted = Describe(controller);

        REQUIRE_MESSAGE((restricted.state == State::Waiting && Supports(restricted, Command::Follow) &&
                         !Supports(restricted, Command::Wait) && !Supports(restricted, Command::Dismiss)),
                        "Quest capabilities ignored");
        REQUIRE_MESSAGE((std::strstr(restricted.reason, "Wait is unavailable") &&
                         std::strstr(restricted.reason, "Dismiss is unavailable") &&
                         !std::strstr(restricted.reason, "Follow is unavailable")),
                        "Restrictions do not describe the actual disabled commands");

        controller.willingToWait = true;
        controller.canDismiss = true;
        controller.canFollow = false;
        const auto cannotFollow = Describe(controller);
        REQUIRE_MESSAGE((!Supports(cannotFollow, Command::Follow) && Supports(cannotFollow, Command::Wait)),
                        "CanFollow restriction ignored");
        REQUIRE_MESSAGE((std::strstr(cannotFollow.reason, "Follow is unavailable")),
                        "Follow restriction has no explanation");

        controller.waiting = false;
        controller.willingToWait = false;
        controller.canDismiss = false;
        const auto allRestricted = Describe(controller);
        REQUIRE_MESSAGE((std::strstr(allRestricted.reason, "Follow is unavailable") &&
                         std::strstr(allRestricted.reason, "Wait is unavailable") &&
                         std::strstr(allRestricted.reason, "Dismiss is unavailable") &&
                         std::strlen(allRestricted.reason) < sizeof(allRestricted.reason)),
                        "Combined restrictions overflow or omit a command");
    }

    TEST_CASE("Dialogue Recruitment Restrictions") {
        using mod::serana_rules::CanRecruitThroughDialogue;

        auto controller = DismissedSerana();
        REQUIRE_MESSAGE((CanRecruitThroughDialogue(controller, false)),
                        "Dismissed, eligible Serana cannot use recruitment dialogue");
        REQUIRE_MESSAGE((!CanRecruitThroughDialogue(controller, true)), "Recruitment bypasses TurnOffComeWithMe");

        controller.locked = true;
        REQUIRE_MESSAGE((!CanRecruitThroughDialogue(controller, false)), "Recruitment bypasses LockedIn");

        controller.locked = false;
        controller.canFollow = false;
        REQUIRE_MESSAGE((!CanRecruitThroughDialogue(controller, false)), "Recruitment bypasses CanFollow");

        auto inconsistent = DismissedSerana();
        inconsistent.waiting = true;
        REQUIRE_MESSAGE(
            (!CanRecruitThroughDialogue(FollowingSerana(), false) && !CanRecruitThroughDialogue(inconsistent, false)),
            "Active/inconsistent follower can be recruited again");
    }

    TEST_CASE("Follow Distance Presets") {
        using mod::serana_rules::DistanceFlags;

        const auto following = FollowingSerana();
        REQUIRE_MESSAGE((DistanceFlags(following, FollowDistance::Close) == std::array{false, false, true}),
                        "Close does not select exactly one distance flag");
        REQUIRE_MESSAGE((DistanceFlags(following, FollowDistance::Normal) == std::array{false, true, false}),
                        "Normal does not select medium distance");
        REQUIRE_MESSAGE((DistanceFlags(following, FollowDistance::Far) == std::array{true, false, false}),
                        "Far does not select far distance");
        REQUIRE_MESSAGE((!DistanceFlags(following, static_cast<FollowDistance>(3))), "Invalid distance accepted");
    }

    TEST_CASE("Combat Protection Restrictions") {
        using mod::serana_rules::CanReceiveCombatProtection;
        auto controller = FollowingSerana();
        REQUIRE_MESSAGE((CanReceiveCombatProtection(controller)), "Following Serana cannot receive combat protection");
        controller.waiting = true;
        REQUIRE_MESSAGE((CanReceiveCombatProtection(controller)), "Waiting removes combat protection");
        controller.locked = true;
        REQUIRE_MESSAGE((!CanReceiveCombatProtection(controller)), "Protection bypasses Dawnguard quest ownership");
        controller.locked = false;
        controller.canFollow = false;
        REQUIRE_MESSAGE((!CanReceiveCombatProtection(controller)), "Protection bypasses CanFollow");
        REQUIRE_MESSAGE((!CanReceiveCombatProtection(DismissedSerana())), "Dismissed Serana retains protection");
        controller = FollowingSerana();
        controller.dismissed = true;
        REQUIRE_MESSAGE((!CanReceiveCombatProtection(controller)), "Inconsistent controller receives protection");
    }

    TEST_CASE("Follow Distance Restrictions") {
        using mod::serana_rules::DistanceFlags;

        auto controller = FollowingSerana();
        controller.locked = true;
        REQUIRE_MESSAGE((!DistanceFlags(controller, FollowDistance::Close)), "Distance bypasses quest lock");
        REQUIRE_MESSAGE((!DistanceFlags(DismissedSerana(), FollowDistance::Close)),
                        "Distance changes a dismissed follower");

        controller.locked = false;
        controller.canFollow = false;
        REQUIRE_MESSAGE((!DistanceFlags(controller, FollowDistance::Close)), "Distance bypasses CanFollow");

        controller.canFollow = true;
        controller.waiting = true;
        REQUIRE_MESSAGE((DistanceFlags(controller, FollowDistance::Far) == std::array{true, false, false}),
                        "Waiting follower cannot choose spacing without resuming");
    }

}

namespace {
    TEST_CASE("Travel Formation Restrictions") {
        using mod::serana_rules::CanTravelInFormation;
        mod::serana_rules::ControllerState state{.following = true};
        REQUIRE_MESSAGE((CanTravelInFormation(state)), "Ordinary following cannot form up");
        state.waiting = true;
        REQUIRE_MESSAGE((!CanTravelInFormation(state)), "Waiting follower received travel movement");
        state.waiting = false;
        state.locked = true;
        REQUIRE_MESSAGE((!CanTravelInFormation(state)), "Formation bypassed the quest lock");
        state.locked = false;
        state.canFollow = false;
        REQUIRE_MESSAGE((!CanTravelInFormation(state)), "Formation bypassed CanFollow");
        state.canFollow = true;
        state.dismissed = true;
        REQUIRE_MESSAGE((!CanTravelInFormation(state)), "Dismissed follower received travel movement");
        state.dismissed = false;
        state.following = false;
        REQUIRE_MESSAGE((!CanTravelInFormation(state)), "Inactive follower received travel movement");
    }
}
