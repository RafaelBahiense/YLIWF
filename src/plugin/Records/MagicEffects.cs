using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static partial class FollowerPlugin
{
    private static void AddMagicEffects(SkyrimMod mod)
    {
        mod.MagicEffects.Add(CreateCompanionProtectionEffect());
    }

    private static MagicEffect CreateCompanionProtectionEffect()
    {
        var companionProtectionEffect = new MagicEffect(OwnForm(0x000801), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_CompanionsSafeEffect",
            Name = new TranslatedString(Language.English, "Companion's Insight"),
            Flags = MagicEffect.Flag.NoMagnitude | MagicEffect.Flag.HideInUI,
            Archetype = new MagicEffectArchetype(MagicEffectArchetype.TypeEnum.Script)
            {
                Type = MagicEffectArchetype.TypeEnum.Script
            },
            DualCastScale = 1.0f,
            PerkToApply = new FormLink<IPerkGetter>(OwnForm(0x000802)),
            Sounds = [],
            Description = new(Language.English, "Your attacks, shouts, and destruction spells do no damage to your followers when in combat.")
        };
        return companionProtectionEffect;
    }
}
