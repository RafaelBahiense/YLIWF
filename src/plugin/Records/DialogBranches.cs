using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private static void AddDialogBranches(SkyrimMod mod)
    {
        mod.DialogBranches.Add(CreateBladesBranch());
        mod.DialogBranches.Add(CreateHomeAssignBranch());
        mod.DialogBranches.Add(CreateHomeClearBranch());
    }

    private static DialogBranch CreateBladesBranch()
    {
        var bladesBranch = new DialogBranch(OwnForm(0x000900), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_BladesBranch",
            Quest = new FormLink<IQuestGetter>(DialogueFollowerQuestForm),
            Category = DialogBranch.CategoryType.Player,
            Flags = (DialogBranch.Flag)0,
            StartingTopic = new FormLinkNullable<IDialogTopicGetter>(OwnForm(0x000911))
        };
        return bladesBranch;
    }

    private static DialogBranch CreateHomeAssignBranch()
    {
        var homeAssignBranch = new DialogBranch(OwnForm(0x000980), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeAssignBranch",
            Quest = new FormLink<IQuestGetter>(DialogueFollowerQuestForm),
            Category = DialogBranch.CategoryType.Player,
            Flags = DialogBranch.Flag.TopLevel,
            StartingTopic = new FormLinkNullable<IDialogTopicGetter>(OwnForm(0x000982))
        };
        return homeAssignBranch;
    }

    private static DialogBranch CreateHomeClearBranch()
    {
        var homeClearBranch = new DialogBranch(OwnForm(0x000981), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeClearBranch",
            Quest = new FormLink<IQuestGetter>(DialogueFollowerQuestForm),
            Category = DialogBranch.CategoryType.Player,
            Flags = DialogBranch.Flag.TopLevel,
            StartingTopic = new FormLinkNullable<IDialogTopicGetter>(OwnForm(0x000983))
        };
        return homeClearBranch;
    }
}
