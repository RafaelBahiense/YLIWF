internal static class RecordNames
{
    internal const string SkyrimMaster = "Skyrim.esm";
    internal const string CanRecruitMore = "YLIWF_CanRecruitMore";
    internal const string CurrentFollowerCount = "YLIWF_CurrentFollowerCount";
    internal const string FollowerSandbox = "YLIWF_FollowerSandbox";
    internal const string FollowerHomes = "YLIWF_FollowerHomes";
    internal const string FollowDistance = "YLIWF_FollowDistance";
    internal const string FollowDistanceChoice = "YLIWF_FollowDistanceChoice";
    internal const string ProtectionSpell = "YLIWF_CompanionsSafeSpell";
    internal const string HomeQuest = "YLIWF_HomeQuest";
    internal const string HomeFaction = "YLIWF_HomeFaction";

    internal static class Aliases
    {
        internal const string Follower = "Follower";
        internal const string Animal = "Animal";
        internal static string Extra(int number) => $"ExtraFollower{number:00}";
        internal static string Home(int index) => $"YLIWF_Home{index:00}";
        internal static string HomeMarker(int index) => $"YLIWF_HomeMarkerAlias{index:00}";
    }
}
