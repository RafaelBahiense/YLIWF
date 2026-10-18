using System.Reflection;
using System.Text.RegularExpressions;
using ModTools.Packaging;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using static ModTools.Tests.TestSupport;

namespace ModTools.Tests;

public sealed partial class PluginTests : TestWorkspace
{
    [Fact]
    public void PluginIdentity()
    {
        var esp = CreatePlugin();
        Artifacts.ValidateEsp(esp);
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(esp, SkyrimRelease.SkyrimSE);
        Assert.Equal(127, loaded.EnumerateMajorRecords().Count());
        Assert.True(loaded.Factions.Single(faction => faction.FormKey == new FormKey(loaded.ModKey, 0x995)).Flags.HasFlag(Faction.FactionFlag.HiddenFromPC));
        Assert.Equal(ModInfo.Identity.Author, loaded.ModHeader.Author);
        Assert.Equal("DialogueFollower", loaded.Quests.Single(q => q.FormKey == FormKey.Factory("0750BA:Skyrim.esm")).EditorID);
    }

    [Fact]
    public void CombatRoleMetadata()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var faction = loaded.Factions.Single(record => record.FormKey == new FormKey(loaded.ModKey, 0x996));
        Assert.Equal("YLIWF_CombatRole", faction.EditorID);
        Assert.Equal(Faction.FactionFlag.HiddenFromPC, faction.Flags);
        Assert.Empty(faction.Relations);
    }

    [Fact]
    public void HealerPackages()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var quest = loaded.Quests.Single(record => record.FormKey == FormKey.Factory("0750BA:Skyrim.esm"));
        var overridesKey = new FormKey(loaded.ModKey, 0xA09);
        var activityKey = new FormKey(loaded.ModKey, 0xA08);
        var overrides = loaded.FormLists.Single(record => record.FormKey == overridesKey);
        Assert.Equal(26, overrides.Items.Count);
        Assert.Equal(FormKey.Factory("109124:Skyrim.esm"), overrides.Items[24].FormKey);
        Assert.Equal(FormKey.Factory("05C851:Skyrim.esm"), overrides.Items[25].FormKey);
        Assert.Equal(Faction.FactionFlag.HiddenFromPC, loaded.Factions.Single(record => record.FormKey == activityKey).Flags);
        Assert.All(quest.Aliases.Where(alias => alias.ID != 1), alias => Assert.Equal(overridesKey, alias.CombatOverridePackageList.FormKey));
        Assert.Equal(FormKey.Null, quest.Aliases.Single(alias => alias.ID == 1).CombatOverridePackageList.FormKey);
        var markers = loaded.EnumerateMajorRecords().OfType<IPlacedObjectGetter>().ToArray();
        for (var slot = 0; slot < 8; ++slot)
        {
            var marker = new FormKey(loaded.ModKey, (uint)(0xA00 + slot));
            Assert.NotEqual(0, (markers.Single(record => record.FormKey == marker).MajorRecordFlagsRaw & 0x400));
            foreach (var start in new uint[] { 0x9D0, 0x9E0 })
            {
                var travel = loaded.Packages.Single(record => record.FormKey == new FormKey(loaded.ModKey, start + (uint)slot));
                CheckFormationHold(travel);
                Assert.True(travel.Flags.HasFlag(Package.Flag.IgnoreCombat));
                Assert.True(travel.Flags.HasFlag(Package.Flag.PreferredSpeed));
                Assert.Equal(3, travel.Unknown);
                Assert.All(quest.Aliases.Where(alias => alias.ID != 1), alias => Assert.Contains(alias.PackageData, link => link.FormKey == travel.FormKey));
                var activeCondition = travel.Conditions.Cast<IConditionFloatGetter>().Last();
                Assert.Equal(activityKey, ((IGetFactionRankConditionDataGetter)activeCondition.Data).Faction.Link.FormKey);
                Assert.Equal(slot + (start == 0x9E0 ? 8 : 0), activeCondition.ComparisonValue);
                var location = (ILocationTargetRadiusGetter)((IPackageDataLocationGetter)travel.Data[0]).Location;
                Assert.Equal(marker, ((ILocationTargetGetter)location.Target).Link.FormKey);
            }
            var cast = loaded.Packages.Single(record => record.FormKey == new FormKey(loaded.ModKey, (uint)(0x9F0 + slot)));
            Assert.Equal(FormKey.Factory("0504F5:Skyrim.esm"), cast.PackageTemplate.FormKey);
            Assert.True(cast.Flags.HasFlag(Package.Flag.IgnoreCombat));
            Assert.False(cast.Flags.HasFlag(Package.Flag.MustComplete));
            Assert.Equal(PackageDataTarget.Types.Target, ((IPackageDataTargetGetter)cast.Data[3]).Type);
            Assert.IsType<IPackageTargetObjectIDGetter>(((IPackageDataTargetGetter)cast.Data[3]).Target, exactMatch: false);
            Assert.Equal(PackageDataTarget.Types.SingleRef, ((IPackageDataTargetGetter)cast.Data[4]).Type);
            Assert.IsType<IPackageTargetSpecificReferenceGetter>(((IPackageDataTargetGetter)cast.Data[4]).Target, exactMatch: false);
            Assert.Equal(1u, ((IPackageDataIntGetter)cast.Data[10]).Data);
            Assert.Equal(1u, ((IPackageDataIntGetter)cast.Data[11]).Data);
            Assert.False(((IPackageDataBoolGetter)cast.Data[12]).Data);
        }
    }

    [Fact]
    public void DllValidation()
    {
        var dll = Path.Combine(temporary, Artifacts.Dll);
        File.WriteAllBytes(dll, DllFixture());
        Artifacts.ValidateDll(dll);
        foreach (var blob in new[] { "\u007fELF"u8.ToArray(), [77, 90], DllFixture()[..600] })
        {
            File.WriteAllBytes(dll, blob);
            Fails(() => Artifacts.ValidateDll(dll));
        }
        var invalid = DllFixture();
        invalid[132] = 0x4c;
        invalid[133] = 1;
        File.WriteAllBytes(dll, invalid);
        Fails(() => Artifacts.ValidateDll(dll));
        invalid = DllFixture();
        invalid[608] = (byte)'X';
        File.WriteAllBytes(dll, invalid);
        Fails(() => Artifacts.ValidateDll(dll));
        File.WriteAllBytes(dll, DllFixture());
    }

    [Fact]
    public void DistancePackages()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var quest = loaded.Quests.Single(q => q.FormKey == FormKey.Factory("0750BA:Skyrim.esm"));
        var distanceGlobal = FormKey.Factory("000993:" + ModInfo.PluginFile);
        var distanceFaction = FormKey.Factory("000994:" + ModInfo.PluginFile);
        Assert.True(loaded.Factions.Single(faction => faction.FormKey == distanceFaction).Flags.HasFlag(Faction.FactionFlag.HiddenFromPC), "Distance marker is not hidden metadata");
        Assert.True(((IGlobalShortGetter)loaded.Globals.Single(global => global.FormKey == distanceGlobal)).Data == 1, "Distance does not default to Normal");
        float[] minimumRadii = [128, 256, 512], maximumRadii = [192, 384, 768];
        for (var preset = 0; preset < 3; ++preset)
        {
            var packageKey = new FormKey(loaded.ModKey, (uint)(0x990 + preset));
            var package = loaded.Packages.Single(p => p.FormKey == packageKey);
            Assert.True(package.PackageTemplate.FormKey == FormKey.Factory("0D530D:Skyrim.esm"), "Preset does not use the vanilla follow/wait template");
            Assert.True(((IPackageDataFloatGetter)package.Data[1]).Data == minimumRadii[preset] &&
                ((IPackageDataFloatGetter)package.Data[2]).Data == maximumRadii[preset], "Preset follow radii are incorrect");
            var conditions = package.Conditions.Cast<IConditionFloatGetter>().ToArray();
            Assert.True(conditions.Length == 2 && conditions[0].ComparisonValue == preset && conditions[1].ComparisonValue == preset + 3 &&
                conditions[0].Flags.HasFlag(Condition.Flag.OR) && !conditions[1].Flags.HasFlag(Condition.Flag.OR) &&
                conditions.All(condition => ((IGetFactionRankConditionDataGetter)condition.Data).Faction.Link.FormKey == distanceFaction),
                "Distance packages do not select individual/party actor ranks independently");
            Assert.True(quest.Aliases.Where(alias => alias.ID != 1).All(alias => alias.PackageData.Count(p => p.FormKey == packageKey) == 1), "Human alias is missing a distance preset");
            Assert.False(quest.Aliases.Single(alias => alias.ID == 1).PackageData.Any(p => p.FormKey == packageKey), "Distance preset affects the animal alias");
        }
    }

    [Fact]
    public void FormationPackages()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var quest = loaded.Quests.Single(q => q.FormKey == FormKey.Factory("0750BA:Skyrim.esm"));
        var targets = loaded.EnumerateMajorRecords().OfType<IPlacedObjectGetter>().ToArray();
        for (var slot = 0; slot < 8; ++slot)
        {
            CheckFormationPackage(loaded, quest, targets, slot, alternate: false);
            CheckFormationPackage(loaded, quest, targets, slot, alternate: true);
        }
    }

    private static void CheckFormationPackage(ISkyrimModGetter loaded, IQuestGetter quest,
        IPlacedObjectGetter[] targets, int slot, bool alternate)
    {
        var packageKey = new FormKey(loaded.ModKey, (uint)((alternate ? 0x9C0 : 0x9A0) + slot));
        var targetKey = new FormKey(loaded.ModKey, (uint)(0x9B0 + slot));
        var package = loaded.Packages.Single(p => p.FormKey == packageKey);
        CheckFormationHold(package);
        Assert.True(package.PreferredSpeed == Package.Speed.Run && package.Flags.HasFlag(Package.Flag.PreferredSpeed),
            "Formation running speed is missing or its override is disabled");
        var location = (ILocationTargetRadiusGetter)((IPackageDataLocationGetter)package.Data[0]).Location;
        Assert.True(((ILocationTargetGetter)location.Target).Link.FormKey == targetKey && location.Radius == 64,
            "Formation slot has the wrong destination or arrival tolerance");
        Assert.True((targets.Single(target => target.FormKey == targetKey).MajorRecordFlagsRaw & 0x400) != 0,
            "Formation destination is not persistent");
        var conditions = package.Conditions.Cast<IConditionFloatGetter>().ToArray();
        Assert.True(conditions.Length == 3 && conditions[0].ComparisonValue == (alternate ? 24 : 16) + slot &&
            ((IGetFactionRankConditionDataGetter)conditions[0].Data).Faction.Link.FormKey == new FormKey(loaded.ModKey, 0x995) &&
            conditions[1].Data is IGetPlayerTeammateConditionDataGetter && conditions[1].ComparisonValue == 1 &&
            conditions[2].Data is IGetActorValueConditionDataGetter value && value.ActorValue == ActorValue.WaitingForPlayer &&
            conditions[2].ComparisonValue == 0, "Formation bypasses its native gate, dismissal or waiting");
        foreach (var alias in quest.Aliases.Where(alias => alias.ID != 1))
        {
            var packages = alias.PackageData.Select(p => p.FormKey).ToList();
            Assert.True(packages.Count(p => p == packageKey) == 1 &&
                packages.IndexOf(packageKey) < packages.IndexOf(new FormKey(loaded.ModKey, 0x990)),
                "Formation package is missing or shadowed by ordinary following");
        }
        Assert.False(quest.Aliases.Single(alias => alias.ID == 1).PackageData.Any(p => p.FormKey == packageKey), "Formation affects the animal alias");
    }

    private static void CheckFormationHold(IPackageGetter package)
    {
        var tree = package.ProcedureTree;
        Assert.True(package.PackageTemplate.IsNull && tree is { Count: 3 },
            "Formation must define its travel-and-hold procedure");
        Assert.True(tree![0].BranchType == "Sequence" &&
            tree[0].Root is { BranchCount: 2, Flags: PackageRoot.Flag.RepeatWhenComplete },
            "Formation stops after arriving instead of repeating its hold sequence");
        Assert.True(tree[1].ProcedureType == "Travel" && tree[1].Flags == 0 &&
            tree[1].DataInputIndices.SequenceEqual(new byte[] { 0, 2, 4 }),
            "Travel completes the package or uses the wrong destination inputs");
        Assert.True(tree[2].ProcedureType == "Wait" && tree[2].Flags == 0 &&
            tree[2].DataInputIndices.SequenceEqual(new byte[] { 5, 6 }) &&
            package.Data[5] is IPackageDataFloatGetter { Data: 1 } &&
            package.Data[6] is IPackageDataBoolGetter { Data: true },
            "Formation does not hold still between travel checks");
    }

    [Fact]
    public void NativeNames()
    {
        // Both languages consume fixed engine names. Detect drift across that boundary.
        CheckNameConstants(root, "src/native/common/PapyrusNames.h", typeof(PapyrusNames.Properties), propertiesOnly: true);
        CheckNameConstants(root, "src/native/common/RecordNames.h", typeof(RecordNames));
    }

    private static void CheckNameConstants(string root, string header, Type definitions, bool propertiesOnly = false)
    {
        var native = File.ReadAllText(Path.Combine(root, header));
        if (propertiesOnly)
        {
            native = NativePropertiesRegex().Match(native).Groups[1].Value;
        }
        else
        {
            native = native.Split("namespace aliases", StringSplitOptions.None)[0];
        }
        var declarations = NativeNamesRegex().Matches(native);
        Assert.True(declarations.Count > 0, "No native name definitions found");
        foreach (Match declaration in declarations)
        {
            var name = declaration.Groups[1].Value;
            var field = definitions.GetField(name, BindingFlags.NonPublic | BindingFlags.Static);
            Assert.True(field is { IsLiteral: true } && field.FieldType == typeof(string) &&
                (string)field.GetRawConstantValue()! == declaration.Groups[2].Value,
                $"Native and ESP names disagree: {name}");
        }
    }

    [GeneratedRegex("constexpr char (\\w+)\\[\\] = \"([^\"]+)\";")]
    private static partial Regex NativeNamesRegex();

    [GeneratedRegex(@"namespace properties\s*\{([^}]+)\}")]
    private static partial Regex NativePropertiesRegex();

    [Fact]
    public void ScriptBindings()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var quest = loaded.Quests.Single(q => q.FormKey == FormKey.Factory("0750BA:Skyrim.esm"));
        var script = quest.VirtualMachineAdapter!.Scripts.Single(s => s.Name == "DialogueFollowerScript");
        Assert.False(script.Properties.Any(p => p.Name is "ExtraAliases" or "CanRecruitMore" or "CurrentFollowerCount"), "Native bindings leaked back into the vanilla facade");
        Assert.True(((IScriptObjectPropertyGetter)script.Properties.Single(p => p.Name == "pPlayerFollowerCount")).Object.FormKey == FormKey.Factory("0BCC98:Skyrim.esm"), "Vanilla follower count binding changed");
        Assert.True(quest.VirtualMachineAdapter.Aliases.Count == 1 && quest.VirtualMachineAdapter.Aliases.Single().Property.Alias == 1 &&
            quest.VirtualMachineAdapter.Aliases.Single().Scripts.Single().Name == "TrainedAnimalScript", "Animal listener lost or humanoid Papyrus listeners retained");
    }

    [Fact]
    public void FollowerAliases()
    {
        using var loaded = SkyrimMod.CreateFromBinaryOverlay(CreatePlugin(), SkyrimRelease.SkyrimSE);
        var quest = loaded.Quests.Single(q => q.FormKey == FormKey.Factory("0750BA:Skyrim.esm"));
        foreach (var id in new uint[] { 0, 2, 3, 4, 5, 6, 7, 8 })
        {
            var alias = quest.Aliases.Single(a => a.ID == id);
            Assert.True(alias.Name == (id == 0 ? "Follower" : $"ExtraFollower{id - 1:00}"), "Alias identity changed");
            Assert.True(alias.PackageData.Count >= 3, "Follower packages missing");
            foreach (var package in alias.PackageData.Where(p => p.FormKey.ModKey == loaded.ModKey))
            {
                Assert.True(loaded.Packages.Any(p => p.FormKey == package.FormKey), "Missing alias package record");
            }
        }
    }
}
