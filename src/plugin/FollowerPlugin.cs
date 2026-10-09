using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private static readonly ModKey OwnMod = ModKey.FromFileName(ModInfo.PluginFile);
    private static readonly ModKey SkyrimMaster = ModKey.FromFileName(RecordNames.SkyrimMaster);
    private const string CustomTopicSubtype = "CUST";
    private static FormKey OwnForm(uint id) => new(OwnMod, id);
    private static FormKey SkyrimForm(uint id) => new(SkyrimMaster, id);

    private static readonly FormKey DialogueFollowerQuestForm = SkyrimForm(0x0750BA);
    private static readonly FormKey PlayerReferenceForm = SkyrimForm(0x000014);
    private static readonly FormKey CurrentFollowerFactionForm = SkyrimForm(0x05C84E);
    private static readonly FormKey PlayerFollowerCountForm = SkyrimForm(0x0BCC98);
    private static readonly FormKey CurrentHirelingFactionForm = SkyrimForm(0x0BD738);
    private static readonly FormKey BladesQuestForm = SkyrimForm(0x0E38C9);

    public static SkyrimMod Create()
    {
        var skyrimMod = new SkyrimMod(OwnMod, SkyrimRelease.SkyrimSE);
        AddHeader(skyrimMod);
        AddCells(skyrimMod);
        AddDialogBranches(skyrimMod);
        AddDialogTopics(skyrimMod);
        AddFactions(skyrimMod);
        AddGlobals(skyrimMod);
        AddMagicEffects(skyrimMod);
        AddPackages(skyrimMod);
        AddPerks(skyrimMod);
        AddQuests(skyrimMod);
        AddSpells(skyrimMod);
        return skyrimMod;
    }
}
