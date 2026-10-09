#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace mod::record_names {
    inline constexpr char SkyrimMaster[] = "Skyrim.esm";
    inline constexpr char CanRecruitMore[] = "YLIWF_CanRecruitMore";
    inline constexpr char CurrentFollowerCount[] = "YLIWF_CurrentFollowerCount";
    inline constexpr char FollowerSandbox[] = "YLIWF_FollowerSandbox";
    inline constexpr char FollowerHomes[] = "YLIWF_FollowerHomes";
    inline constexpr char FollowDistance[] = "YLIWF_FollowDistance";
    inline constexpr char FollowDistanceChoice[] = "YLIWF_FollowDistanceChoice";
    inline constexpr char ProtectionSpell[] = "YLIWF_CompanionsSafeSpell";
    inline constexpr char HomeQuest[] = "YLIWF_HomeQuest";
    inline constexpr char HomeFaction[] = "YLIWF_HomeFaction";

    namespace aliases {
        inline constexpr char Follower[] = "Follower";
        inline constexpr char Animal[] = "Animal";
        inline constexpr char ExtraPrefix[] = "ExtraFollower";
        inline constexpr std::array<std::string_view, 7> Extras{"ExtraFollower01", "ExtraFollower02", "ExtraFollower03",
                                                                "ExtraFollower04", "ExtraFollower05", "ExtraFollower06",
                                                                "ExtraFollower07"};
        inline constexpr char HomePrefix[] = "YLIWF_Home";
        inline constexpr char HomeMarkerPrefix[] = "YLIWF_HomeMarkerAlias";

        inline std::string FollowerSlot(std::uint32_t aliasID) {
            return aliasID == 0
                       ? std::string(Follower)
                       : std::string(ExtraPrefix) + (aliasID - 1 < 10 ? "0" : "") + std::to_string(aliasID - 1);
        }

        inline std::string Home(std::uint32_t aliasID) {
            const auto index = aliasID < 8 ? aliasID : aliasID - 8;
            return std::string(aliasID < 8 ? HomePrefix : HomeMarkerPrefix) + (index < 10 ? "0" : "") +
                   std::to_string(index);
        }
    }
}
