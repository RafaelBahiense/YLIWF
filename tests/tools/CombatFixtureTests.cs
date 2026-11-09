using Mutagen.Bethesda.Skyrim;
using Yliwf.Addons.CombatTests;

namespace ModTools.Tests;

public sealed class CombatFixtureTests : TestWorkspace
{
    [Fact]
    public void SpellDiagnosticUsesAnIndependentStableTargetAndHarmlessDurationControl()
    {
        var mod = CombatArena.Create();
        var target = mod.Npcs.Single(npc => npc.FormKey.ID == 0x857);
        Assert.Equal("YLIWFSpellDiagnosticTarget", target.EditorID);
        Assert.False(target.Configuration!.Flags.HasFlag(NpcConfiguration.Flag.Invulnerable));
        Assert.False(target.Configuration.TemplateFlags.HasFlag(NpcConfiguration.TemplateFlag.BaseData));
        var control = mod.Spells.Single(spell => spell.FormKey.ID == 0x858);
        Assert.Equal(TargetType.TargetActor, control.TargetType);
        var effect = Assert.Single(control.Effects);
        Assert.Equal(2, effect.Data!.Duration);
        var magic = mod.MagicEffects.Single(item => item.FormKey == effect.BaseEffect.FormKey);
        Assert.False(magic.Flags.HasFlag(MagicEffect.Flag.Hostile));
        Assert.False(magic.Flags.HasFlag(MagicEffect.Flag.Detrimental));
        Assert.Equal(ActorValue.WaterBreathing, magic.Archetype.ActorValue);
        Assert.Equal(MagicEffectArchetype.TypeEnum.ValueModifier, Assert.IsType<MagicEffectArchetype>(magic.Archetype).Type);
    }

    [Fact]
    public void ControlledFixturesDeclareHostilityToParticipantsWithoutMakingTheObserverAnEnemy()
    {
        var mod = CombatArena.Create();
        var opponent = mod.Factions.Single(faction => faction.EditorID == "YLIWFCombatTestOpponentFaction");
        var subject = mod.Factions.Single(faction => faction.EditorID == "YLIWFCombatTestSubject");
        Assert.Equal(CombatReaction.Enemy, opponent.Relations.Single(relation => relation.Target.FormKey == subject.FormKey).Reaction);
        Assert.Equal(CombatReaction.Enemy, subject.Relations.Single(relation => relation.Target.FormKey == opponent.FormKey).Reaction);
        Assert.DoesNotContain(opponent.Relations, relation => relation.Target.FormKey.ID == 0xDB1);
    }

    [Fact]
    public void ControlledOpponentBasesHaveStablePluginIdentitiesAndSafeReloadDefaults()
    {
        var path = Path.Combine(temporary, CombatArena.Plugin);
        CombatArena.Create().BeginWrite.ToPath(path).WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder().NoMastersListContentCheck().Write();
        using var mod = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
        Assert.Equal(CombatArena.FixtureBaseCount + 1, mod.Npcs.Count);
        for (var index = 0; index < CombatArena.FixtureBaseCount; ++index)
        {
            var npc = mod.Npcs.Single(npc => npc.FormKey.ID == CombatArena.FixtureBaseStart + (uint)index);
            Assert.Equal(mod.ModKey, npc.FormKey.ModKey);
            var ally = index == CombatArena.AllyFixtureIndex;
            Assert.Equal(ally ? "YLIWFCombatTestArcherAlly" : "YLIWFCombatTestOpponent" + index, npc.EditorID);
            Assert.Equal(ally ? "YLIWF Combat Test Archer Ally" : "YLIWF Combat Test Opponent", npc.Name?.String);
            Assert.False(npc.Configuration.Flags.HasFlag(NpcConfiguration.Flag.Invulnerable));
            Assert.False(npc.Configuration.Flags.HasFlag(NpcConfiguration.Flag.Essential));
            Assert.False(npc.Configuration.Flags.HasFlag(NpcConfiguration.Flag.Protected));
            Assert.Equal(index >= 3 && !ally ? 0x44CD7u : 0xC3CA0u, npc.Template.FormKey.ID);
            Assert.Equal("Skyrim.esm", npc.Template.FormKey.ModKey.FileName.String);
            Assert.True(npc.Configuration.TemplateFlags.HasFlag(NpcConfiguration.TemplateFlag.Traits));
            Assert.True(npc.Configuration.TemplateFlags.HasFlag(NpcConfiguration.TemplateFlag.Stats));
            Assert.False(npc.Configuration.TemplateFlags.HasFlag(NpcConfiguration.TemplateFlag.SpellList));
            Assert.False(npc.Configuration.TemplateFlags.HasFlag(NpcConfiguration.TemplateFlag.Script));
            Assert.False(npc.Configuration.TemplateFlags.HasFlag(NpcConfiguration.TemplateFlag.Inventory));
            Assert.True(npc.ActorEffect is null || npc.ActorEffect.Count == 0);
            Assert.Null(npc.VirtualMachineAdapter);
        }
        Assert.All(mod.EnumerateMajorRecords(), record => Assert.Equal(mod.ModKey, record.FormKey.ModKey));
    }
}
