using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private static void AddGlobals(SkyrimMod mod)
    {
        mod.Globals.Add(CreateCanRecruitMore());
        mod.Globals.Add(CreateCurrentFollowerCount());
        mod.Globals.Add(CreateFollowerHomesEnabled());
        mod.Globals.Add(CreateFollowerSandboxEnabled());
        mod.Globals.Add(new GlobalShort(FollowDistanceGlobalForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.FollowDistance,
            Data = 1
        });
    }

    private static GlobalShort CreateCanRecruitMore()
    {
        var canRecruitMore = new GlobalShort(OwnForm(0x000001), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.CanRecruitMore,
            Data = 1
        };
        return canRecruitMore;
    }

    private static GlobalShort CreateCurrentFollowerCount()
    {
        var currentFollowerCount = new GlobalShort(OwnForm(0x000002), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.CurrentFollowerCount,
            Data = 0
        };
        return currentFollowerCount;
    }

    private static GlobalShort CreateFollowerHomesEnabled()
    {
        var followerHomesEnabled = new GlobalShort(OwnForm(0x000986), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.FollowerHomes,
            Data = 1
        };
        return followerHomesEnabled;
    }

    private static GlobalShort CreateFollowerSandboxEnabled()
    {
        var followerSandboxEnabled = new GlobalShort(OwnForm(0x000805), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.FollowerSandbox,
            Data = 0
        };
        return followerSandboxEnabled;
    }
}
