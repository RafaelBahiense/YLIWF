using ModTools.Packaging;
using System.Reflection;
using System.Text.RegularExpressions;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;
using static TestSupport;

internal static partial class PluginTests
{
    public static string CreateDll(string temporary)
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

        return dll;
    }

    public static string Run(string root, string temporary)
    {
        var mod = FollowerPlugin.Create();
        CheckNativeNames(root);
        var esp = Path.Combine(temporary, Artifacts.Plugin);
        mod.BeginWrite.ToPath(esp).WithLoadOrderFromHeaderMasters().WithNoDataFolder().NoMastersListContentCheck().Write();
        Artifacts.ValidateEsp(esp);
        BuildReceipt.Write(root, BuildComponent.Plugin, BuildReceipt.CaptureInputs(root, BuildComponent.Plugin), [esp], BuildReceipt.PluginPath(esp));
        using (var loaded = SkyrimMod.CreateFromBinaryOverlay(esp, SkyrimRelease.SkyrimSE))
        {
            Check(loaded.EnumerateMajorRecords().Count() == 67, "Record count changed");
            Check(loaded.ModHeader.Author == ModInfo.Identity.Author, "Plugin header did not use centralized author");
            var quest = loaded.Quests.Single(q => q.FormKey == FormKey.Factory("0750BA:Skyrim.esm"));
            Check(quest.EditorID == "DialogueFollower", "Wrong follower quest");
            CheckDistancePackages(loaded, quest);
            CheckScriptBindings(quest);
            CheckFollowerAliases(loaded, quest);
        }
        return esp;
    }

    private static void CheckDistancePackages(ISkyrimModGetter loaded, IQuestGetter quest)
    {
        var distanceGlobal = FormKey.Factory("000993:" + ModInfo.PluginFile);
        var distanceFaction = FormKey.Factory("000994:" + ModInfo.PluginFile);
        Check(loaded.Factions.Single(faction => faction.FormKey == distanceFaction).Flags.HasFlag(Faction.FactionFlag.HiddenFromPC), "Distance marker is not hidden metadata");
        Check(((IGlobalShortGetter)loaded.Globals.Single(global => global.FormKey == distanceGlobal)).Data == 1, "Distance does not default to Normal");
        float[] minimumRadii = [128, 256, 512], maximumRadii = [192, 384, 768];
        for (var preset = 0; preset < 3; ++preset)
        {
            var packageKey = new FormKey(loaded.ModKey, (uint)(0x990 + preset));
            var package = loaded.Packages.Single(p => p.FormKey == packageKey);
            Check(package.PackageTemplate.FormKey == FormKey.Factory("0D530D:Skyrim.esm"), "Preset does not use the vanilla follow/wait template");
            Check(((IPackageDataFloatGetter)package.Data[1]).Data == minimumRadii[preset] &&
                ((IPackageDataFloatGetter)package.Data[2]).Data == maximumRadii[preset], "Preset follow radii are incorrect");
            var conditions = package.Conditions.Cast<IConditionFloatGetter>().ToArray();
            Check(conditions.Length == 2 && conditions[0].ComparisonValue == preset && conditions[1].ComparisonValue == preset + 3 &&
                conditions[0].Flags.HasFlag(Condition.Flag.OR) && !conditions[1].Flags.HasFlag(Condition.Flag.OR) &&
                conditions.All(condition => ((IGetFactionRankConditionDataGetter)condition.Data).Faction.Link.FormKey == distanceFaction),
                "Distance packages do not select individual/party actor ranks independently");
            Check(quest.Aliases.Where(alias => alias.ID != 1).All(alias => alias.PackageData.Count(p => p.FormKey == packageKey) == 1), "Human alias is missing a distance preset");
            Check(!quest.Aliases.Single(alias => alias.ID == 1).PackageData.Any(p => p.FormKey == packageKey), "Distance preset affects the animal alias");
        }
    }

    private static void CheckNativeNames(string root)
    {
        // Both languages consume fixed engine names. Detect drift across that boundary.
        CheckNameConstants(root, "include/PapyrusNames.h", typeof(PapyrusNames.Properties), propertiesOnly: true);
        CheckNameConstants(root, "include/RecordNames.h", typeof(RecordNames));
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
        Check(declarations.Count > 0, "No native name definitions found");
        foreach (Match declaration in declarations)
        {
            var name = declaration.Groups[1].Value;
            var field = definitions.GetField(name, BindingFlags.NonPublic | BindingFlags.Static);
            Check(field is { IsLiteral: true } && field.FieldType == typeof(string) &&
                (string)field.GetRawConstantValue()! == declaration.Groups[2].Value,
                $"Native and ESP names disagree: {name}");
        }
    }

    [GeneratedRegex("constexpr char (\\w+)\\[\\] = \"([^\"]+)\";")]
    private static partial Regex NativeNamesRegex();

    [GeneratedRegex(@"namespace properties\s*\{([^}]+)\}")]
    private static partial Regex NativePropertiesRegex();

    private static void CheckScriptBindings(IQuestGetter quest)
    {
        var script = quest.VirtualMachineAdapter!.Scripts.Single(s => s.Name == "DialogueFollowerScript");
        Check(!script.Properties.Any(p => p.Name is "ExtraAliases" or "CanRecruitMore" or "CurrentFollowerCount"), "Native bindings leaked back into the vanilla facade");
        Check(((IScriptObjectPropertyGetter)script.Properties.Single(p => p.Name == "pPlayerFollowerCount")).Object.FormKey == FormKey.Factory("0BCC98:Skyrim.esm"), "Vanilla follower count binding changed");
        Check(quest.VirtualMachineAdapter.Aliases.Count == 1 && quest.VirtualMachineAdapter.Aliases.Single().Property.Alias == 1 &&
            quest.VirtualMachineAdapter.Aliases.Single().Scripts.Single().Name == "TrainedAnimalScript", "Animal listener lost or humanoid Papyrus listeners retained");
    }

    private static void CheckFollowerAliases(ISkyrimModGetter loaded, IQuestGetter quest)
    {
        foreach (var id in new uint[] { 0, 2, 3, 4, 5, 6, 7, 8 })
        {
            var alias = quest.Aliases.Single(a => a.ID == id);
            Check(alias.Name == (id == 0 ? "Follower" : $"ExtraFollower{id - 1:00}"), "Alias identity changed");
            Check(alias.PackageData.Count >= 3, "Follower packages missing");
            foreach (var package in alias.PackageData.Where(p => p.FormKey.ModKey == loaded.ModKey))
            {
                Check(loaded.Packages.Any(p => p.FormKey == package.FormKey), "Missing alias package record");
            }
        }
    }
}
