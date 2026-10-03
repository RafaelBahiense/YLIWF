using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Strings;

internal static partial class FollowerPlugin
{
    private static void AddSpells(SkyrimMod mod)
    {
        mod.Spells.Add(CreateCompanionProtectionAbility());
    }

    private static Spell CreateCompanionProtectionAbility()
    {
        var companionProtectionAbility = new Spell(OwnForm(0x000800), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_CompanionsSafeSpell",
            Name = new TranslatedString(Language.English, "Companion's Insight"),
            EquipmentType = new FormLinkNullable<IEquipTypeGetter>(SkyrimForm(0x013F44)),
            Description = new TranslatedString(Language.English, ""),
            Type = SpellType.Ability
        };
        var protectionSpellEffect = new Effect
        {
            BaseEffect = new FormLinkNullable<IMagicEffectGetter>(OwnForm(0x000801))
        };
        var protectionEffectData = new EffectData();
        protectionSpellEffect.Data = protectionEffectData;
        companionProtectionAbility.Effects.Add(protectionSpellEffect);
        return companionProtectionAbility;
    }
}
