#pragma once

namespace mod::papyrus_names {
    inline constexpr char DialogueFollower[] = "DialogueFollowerScript";

    namespace properties {
        inline constexpr char FollowerAlias[] = "pFollowerAlias";
        inline constexpr char AnimalAlias[] = "pAnimalAlias";
        inline constexpr char PlayerFollowerCount[] = "pPlayerFollowerCount";
        inline constexpr char PlayerAnimalCount[] = "pPlayerAnimalCount";
        inline constexpr char CurrentHireling[] = "pCurrentHireling";
        inline constexpr char DismissedFollower[] = "pDismissedFollower";
        inline constexpr char FollowerDismiss[] = "iFollowerDismiss";
        inline constexpr char HuntingBow[] = "FollowerHuntingBow";
        inline constexpr char IronArrow[] = "FollowerIronArrow";
        inline constexpr char HirelingRehire[] = "HirelingRehireScript";
        inline constexpr char AnimalDismissMessage[] = "AnimalDismissMessage";
        inline constexpr char FollowerDismissMessage[] = "FollowerDismissMessage";
        inline constexpr char WeddingDismissMessage[] = "FollowerDismissMessageWedding";
        inline constexpr char CompanionsDismissMessage[] = "FollowerDismissMessageCompanions";
        inline constexpr char CompanionsMaleDismissMessage[] = "FollowerDismissMessageCompanionsMale";
        inline constexpr char CompanionsFemaleDismissMessage[] = "FollowerDismissMessageCompanionsFemale";
        inline constexpr char WaitDismissMessage[] = "FollowerDismissMessageWait";
    }

    // Auto-property backing variables exposed by the VM.
    namespace variables {
        inline constexpr char FollowerAlias[] = "::pFollowerAlias_var";
        inline constexpr char PlayerFollowerCount[] = "::pPlayerFollowerCount_var";
    }
}
