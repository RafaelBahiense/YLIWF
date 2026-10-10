#include "Rules.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
    using namespace yliwf::sdk;
    using mod::serana_rules::ControllerState;
    using mod::serana_rules::Describe;

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    ControllerState FollowingSerana() {
        return {.following = true, .willingToWait = true, .canDismiss = true};
    }

    ControllerState DismissedSerana() {
        return {.dismissed = true, .willingToWait = true, .canDismiss = true};
    }

    void CheckNormalFollowerCommands() {
        auto following = Describe(FollowingSerana());
        Check(Supports(following, Command::Follow) && Supports(following, Command::Wait) &&
                  Supports(following, Command::Dismiss),
              "Normal Serana commands missing");

        const auto dismissed = Describe(DismissedSerana());
        Check(dismissed.state == State::Inactive && !Supports(dismissed, Command::Follow),
              "Adapter can recruit inactive Serana");
        Check(Reached(dismissed, Command::Dismiss) && !Reached(following, Command::Wait),
              "Wrong controller state acknowledged");
    }

    void CheckQuestLocks() {
        auto controller = FollowingSerana();
        controller.locked = true;
        const auto locked = Describe(controller);

        Check(locked.state == State::Following && locked.commands == 0, "Quest-locked Serana can be changed");
        Check(std::strstr(locked.reason, "Follow, Wait and Dismiss"), "Quest lock does not explain disabled commands");
    }

    void CheckCommandRestrictions() {
        auto controller = FollowingSerana();
        controller.waiting = true;
        controller.willingToWait = false;
        controller.canDismiss = false;
        const auto restricted = Describe(controller);

        Check(restricted.state == State::Waiting && Supports(restricted, Command::Follow) &&
                  !Supports(restricted, Command::Wait) && !Supports(restricted, Command::Dismiss),
              "Quest capabilities ignored");
        Check(std::strstr(restricted.reason, "Wait is unavailable") &&
                  std::strstr(restricted.reason, "Dismiss is unavailable") &&
                  !std::strstr(restricted.reason, "Follow is unavailable"),
              "Restrictions do not describe the actual disabled commands");

        controller.willingToWait = true;
        controller.canDismiss = true;
        controller.canFollow = false;
        const auto cannotFollow = Describe(controller);
        Check(!Supports(cannotFollow, Command::Follow) && Supports(cannotFollow, Command::Wait),
              "CanFollow restriction ignored");
        Check(std::strstr(cannotFollow.reason, "Follow is unavailable"), "Follow restriction has no explanation");

        controller.waiting = false;
        controller.willingToWait = false;
        controller.canDismiss = false;
        const auto allRestricted = Describe(controller);
        Check(std::strstr(allRestricted.reason, "Follow is unavailable") &&
                  std::strstr(allRestricted.reason, "Wait is unavailable") &&
                  std::strstr(allRestricted.reason, "Dismiss is unavailable") &&
                  std::strlen(allRestricted.reason) < sizeof(allRestricted.reason),
              "Combined restrictions overflow or omit a command");
    }

    void CheckDialogueRecruitmentRestrictions() {
        using mod::serana_rules::CanRecruitThroughDialogue;

        auto controller = DismissedSerana();
        Check(CanRecruitThroughDialogue(controller, false),
              "Dismissed, eligible Serana cannot use recruitment dialogue");
        Check(!CanRecruitThroughDialogue(controller, true), "Recruitment bypasses TurnOffComeWithMe");

        controller.locked = true;
        Check(!CanRecruitThroughDialogue(controller, false), "Recruitment bypasses LockedIn");

        controller.locked = false;
        controller.canFollow = false;
        Check(!CanRecruitThroughDialogue(controller, false), "Recruitment bypasses CanFollow");

        auto inconsistent = DismissedSerana();
        inconsistent.waiting = true;
        Check(!CanRecruitThroughDialogue(FollowingSerana(), false) && !CanRecruitThroughDialogue(inconsistent, false),
              "Active/inconsistent follower can be recruited again");
    }

    void CheckFollowDistancePresets() {
        using mod::serana_rules::DistanceFlags;

        const auto following = FollowingSerana();
        Check(DistanceFlags(following, FollowDistance::Close) == std::array{false, false, true},
              "Close does not select exactly one distance flag");
        Check(DistanceFlags(following, FollowDistance::Normal) == std::array{false, true, false},
              "Normal does not select medium distance");
        Check(DistanceFlags(following, FollowDistance::Far) == std::array{true, false, false},
              "Far does not select far distance");
        Check(!DistanceFlags(following, static_cast<FollowDistance>(3)), "Invalid distance accepted");
    }

    void CheckCombatProtectionRestrictions() {
        using mod::serana_rules::CanReceiveCombatProtection;
        auto controller = FollowingSerana();
        Check(CanReceiveCombatProtection(controller), "Following Serana cannot receive combat protection");
        controller.waiting = true;
        Check(CanReceiveCombatProtection(controller), "Waiting removes combat protection");
        controller.locked = true;
        Check(!CanReceiveCombatProtection(controller), "Protection bypasses Dawnguard quest ownership");
        controller.locked = false;
        controller.canFollow = false;
        Check(!CanReceiveCombatProtection(controller), "Protection bypasses CanFollow");
        Check(!CanReceiveCombatProtection(DismissedSerana()), "Dismissed Serana retains protection");
        controller = FollowingSerana();
        controller.dismissed = true;
        Check(!CanReceiveCombatProtection(controller), "Inconsistent controller receives protection");
    }

    void CheckFollowDistanceRestrictions() {
        using mod::serana_rules::DistanceFlags;

        auto controller = FollowingSerana();
        controller.locked = true;
        Check(!DistanceFlags(controller, FollowDistance::Close), "Distance bypasses quest lock");
        Check(!DistanceFlags(DismissedSerana(), FollowDistance::Close), "Distance changes a dismissed follower");

        controller.locked = false;
        controller.canFollow = false;
        Check(!DistanceFlags(controller, FollowDistance::Close), "Distance bypasses CanFollow");

        controller.canFollow = true;
        controller.waiting = true;
        Check(DistanceFlags(controller, FollowDistance::Far) == std::array{true, false, false},
              "Waiting follower cannot choose spacing without resuming");
    }

}

int main() {
    CheckNormalFollowerCommands();
    CheckQuestLocks();
    CheckCommandRestrictions();
    CheckDialogueRecruitmentRestrictions();
    CheckFollowDistancePresets();
    CheckFollowDistanceRestrictions();
    CheckCombatProtectionRestrictions();

    std::cout << "Serana command, quest restriction, recruitment, and distance tests passed.\n";
}
