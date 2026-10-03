using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Records = Mutagen.Bethesda.Plugins.Records;

internal static partial class FollowerPlugin
{
    private static void AddHeader(SkyrimMod mod)
    {
        mod.ModHeader.Flags = SkyrimModHeader.HeaderFlag.Small;
        mod.ModHeader.Author = ModInfo.Identity.Author;
        var skyrimMaster = new Records.MasterReference
        {
            Master = SkyrimMaster,
            FileSize = 0
        };
        mod.ModHeader.MasterReferences.Add(skyrimMaster);
        var updateMaster = new Records.MasterReference
        {
            Master = ModKey.FromFileName("Update.esm"),
            FileSize = 0
        };
        mod.ModHeader.MasterReferences.Add(updateMaster);
        var dawnguardMaster = new Records.MasterReference
        {
            Master = ModKey.FromFileName("Dawnguard.esm"),
            FileSize = 0
        };
        mod.ModHeader.MasterReferences.Add(dawnguardMaster);
        var hearthFiresMaster = new Records.MasterReference
        {
            Master = ModKey.FromFileName("HearthFires.esm"),
            FileSize = 0
        };
        mod.ModHeader.MasterReferences.Add(hearthFiresMaster);
        var dragonbornMaster = new Records.MasterReference
        {
            Master = ModKey.FromFileName("Dragonborn.esm"),
            FileSize = 0
        };
        mod.ModHeader.MasterReferences.Add(dragonbornMaster);
        mod.ModHeader.INTV = 1;
    }
}
