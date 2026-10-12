using ModTools.Building;

namespace ModTools.Tests;

public sealed class BuildTests : TestWorkspace
{
    private static string PrepareConfiguration(string root, string temporary)
    {
        var directory = Path.Combine(temporary, "build-options");
        Directory.CreateDirectory(Path.Combine(directory, ".tools"));
        Directory.CreateDirectory(Path.Combine(directory, "tools"));
        File.Copy(Path.Combine(root, "tools/paths.json"), Path.Combine(directory, "tools/paths.json"));
        File.WriteAllText(Path.Combine(directory, ".tools/local.json"),
            """{"CommonLib":"custom commonlib", "Imports":["custom imports"]}""");
        return directory;
    }

    [Fact]
    public void PathOverrides()
    {
        var directory = PrepareConfiguration(root, temporary);
        var arguments = new Dictionary<string, string> { ["--root"] = directory };
        var options = BuildOptions.Read(arguments, new HashSet<string>());
        Assert.True(options.Target == BuildTarget.All && options.CommonLib == Path.Combine(directory, "custom commonlib") &&
            options.Imports.SequenceEqual([Path.Combine(directory, "custom imports")]), "Local paths were not resolved relative to the build root");
        arguments["--commonlib"] = "override";
        Assert.True(BuildOptions.Read(arguments, new HashSet<string>()).CommonLib == Path.Combine(directory, "override"),
            "Explicit option did not override local paths");
    }

    [Fact]
    public void RepackagingOptions()
    {
        var directory = PrepareConfiguration(root, temporary);
        var arguments = new Dictionary<string, string>
        {
            ["--root"] = directory,
            ["--commonlib"] = "override",
            ["--dll"] = "external.dll"
        };
        TestSupport.RejectBuild(() => BuildOptions.Read(arguments, new HashSet<string>()));
        arguments["--target"] = "Repackage";
        TestSupport.RejectBuild(() => BuildOptions.Read(arguments, new HashSet<string>()));
        arguments["--source-archive"] = "matching.zip";
        Assert.True(BuildOptions.Read(arguments, new HashSet<string>()).Target == BuildTarget.Repackage,
            "Valid repackaging request rejected");
        TestSupport.RejectBuild(() => BuildOptions.Read(arguments, new HashSet<string> { "--clean" }));
        arguments["--typo"] = "value";
        TestSupport.RejectBuild(() => BuildOptions.Read(arguments, new HashSet<string>()));
    }

    [Fact]
    public void Configurations()
    {
        var directory = PrepareConfiguration(root, temporary);
        var arguments = new Dictionary<string, string> { ["--root"] = directory };
        Assert.True(BuildOptions.Read(arguments, new HashSet<string>()).Configuration == BuildConfiguration.Release,
            "Default build configuration changed");
        foreach (var configuration in Enum.GetValues<BuildConfiguration>())
        {
            arguments["--configuration"] = configuration.CMakeName();
            var options = BuildOptions.Read(arguments, new HashSet<string>());
            var expectedPreset = configuration switch
            {
                BuildConfiguration.Debug => "debug",
                BuildConfiguration.Release => "release",
                BuildConfiguration.RelWithDebInfo => "relwithdebinfo",
                _ => throw new InvalidOperationException("Unknown configuration")
            };
            Assert.True(options.Configuration == configuration && options.Configuration.Preset() == expectedPreset,
                "Build configuration does not select its CMake preset");
        }
        foreach (var invalid in new[] { "release", "0", "Unknown" })
        {
            arguments["--configuration"] = invalid;
            TestSupport.RejectBuild(() => BuildOptions.Read(arguments, new HashSet<string>()));
        }
    }

    [Fact]
    public void AddonDeclarations()
    {
        var directory = PrepareConfiguration(root, temporary);
        var addonDirectory = Path.Combine(directory, "src/addons");
        foreach (var name in new[] { "First", "Second" })
        {
            Directory.CreateDirectory(Path.Combine(addonDirectory, name));
            File.WriteAllText(Path.Combine(addonDirectory, name, "addon.json"), $$"""{"name":"{{name}}","version":"2.3.4","adapterApiVersion":1}""");
        }
        Assert.True(Addon.Read(directory).Select(addon => addon.DllFile).SequenceEqual([
            ModInfo.BinaryName + ".First.dll", ModInfo.BinaryName + ".Second.dll"]), "New add-ons need special-case tooling");
        var first = Addon.Read(directory)[0];
        Assert.True(first.Version == "2.3.4" && first.AdapterApiVersion == 1 &&
            first.ArchiveName == ModInfo.BinaryName + "-First-2.3.4.zip" &&
            first.SourceArchiveName == ModInfo.BinaryName + "-First-2.3.4-source.zip", "Add-on versions are coupled to the core release");
        var second = Path.Combine(addonDirectory, "Second/addon.json");
        File.WriteAllText(second, """{"name":"first","version":"1.0.0","adapterApiVersion":1}""");
        TestSupport.RejectBuild(() => Addon.Read(directory));
        File.WriteAllText(second, """{"name":"../escape"}""");
        TestSupport.RejectBuild(() => Addon.Read(directory));
        foreach (var invalid in new[]
        {
            """{"name":"Second","adapterApiVersion":1}""",
            """{"name":"Second","version":"1.2","adapterApiVersion":1}""",
            """{"name":"Second","version":"1.0.0"}""",
            """{"name":"Second","version":"1.0.0","adapterApiVersion":0}""",
            """{"name":"Second","version":"1.0.0","adapterApiVersion":"1"}"""
        })
        {
            File.WriteAllText(second, invalid);
            TestSupport.RejectBuild(() => Addon.Read(directory));
        }
    }
}
