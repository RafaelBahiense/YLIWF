using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static partial class FollowerPlugin
{
    private static void AddFactions(SkyrimMod mod)
    {
        mod.Factions.Add(new Faction(OwnForm(0x000996), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.CombatRole,
            Flags = Faction.FactionFlag.HiddenFromPC
        });
        mod.Factions.Add(CreateHomeFaction());
        mod.Factions.Add(new Faction(FollowDistanceFactionForm, SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.FollowDistanceChoice,
            Flags = Faction.FactionFlag.HiddenFromPC
        });
        mod.Factions.Add(new Faction(OwnForm(0x000995), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.FormationSlot,
            Flags = Faction.FactionFlag.HiddenFromPC
        });
    }

    private static Faction CreateHomeFaction()
    {
        var homeFaction = new Faction(OwnForm(0x000960), SkyrimRelease.SkyrimSE)
        {
            EditorID = RecordNames.HomeFaction,
            Name = new TranslatedString(Language.English, $"{ModInfo.ShortName} Home")
        };
        var crimeValues = new CrimeValues
        {
            Arrest = true,
            AttackOnSight = true
        };
        homeFaction.CrimeValues = crimeValues;
        var vendorValues = new VendorValues();
        homeFaction.VendorValues = vendorValues;
        return homeFaction;
    }
}
