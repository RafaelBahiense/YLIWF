using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

internal static partial class FollowerPlugin
{
    private static void AddDefenderTaunt(SkyrimMod mod)
    {
        mod.MagicEffects.Add(new MagicEffect(OwnForm(0xA1E), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_DefenderChallengeEffect",
            Name = "Defender's Challenge",
            Description = "Challenged by a defender protecting an ally.",
            Flags = MagicEffect.Flag.Hostile | MagicEffect.Flag.Detrimental | MagicEffect.Flag.NoMagnitude,
            Archetype = new MagicEffectArchetype(MagicEffectArchetype.TypeEnum.Script),
            CastType = CastType.FireAndForget,
            TargetType = TargetType.TargetActor,
            ResistValue = ActorValue.None,
            Sounds = []
        });
        var challenge = new Spell(OwnForm(0xA1D), SkyrimRelease.SkyrimSE)
        {
            EditorID = "YLIWF_DefenderChallenge",
            Name = "Defender's Challenge",
            Description = "Challenge one nearby enemy threatening an ally.",
            Type = SpellType.LesserPower,
            CastType = CastType.FireAndForget,
            TargetType = TargetType.TargetActor,
            BaseCost = 0
        };
        challenge.Effects.Add(new Effect
        {
            BaseEffect = new FormLinkNullable<IMagicEffectGetter>(OwnForm(0xA1E)),
            Data = new EffectData { Duration = 4, Magnitude = 0, Area = 0 }
        });
        mod.Spells.Add(challenge);
    }
}
