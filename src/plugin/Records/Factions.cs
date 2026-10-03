using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static partial class FollowerPlugin
{
    private static void AddFactions(SkyrimMod mod)
    {
        mod.Factions.Add(CreateHomeFaction());
    }

    private static Faction CreateHomeFaction()
    {
        var homeFaction = new Faction(OwnForm(0x000960), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_HomeFaction",
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
