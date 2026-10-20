using System.Globalization;
using System.IO.Compression;
using System.Text.Json;
using ModTools.Building;
using ModTools.Packaging;
using ModTools.Papyrus;
using static ModTools.Tests.TestSupport;

namespace ModTools.Tests;

public sealed class PapyrusTests : TestWorkspace
{
    public static string PrepareScripts(string root, string temporary)
    {
        var papyrus = Path.Combine(temporary, "papyrus");
        Scripts(Path.Combine(root, "src/papyrus/core"), Path.Combine(papyrus, "Scripts"));
        BuildReceipt.Write(root, BuildComponent.PapyrusCore, BuildReceipt.CaptureInputs(root, BuildComponent.PapyrusCore),
            Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/core"), Path.Combine(papyrus, "Scripts")), Path.Combine(papyrus, "Scripts", BuildReceipt.ScriptsFile));
        File.WriteAllText(Path.Combine(papyrus, "Scripts/Game.pex"), "stale import");
        foreach (var addon in Addon.Read(root).Where(addon => Directory.Exists(addon.PapyrusSource)))
        {
            var output = Path.Combine(papyrus, "addons", addon.Name, "Scripts");
            Scripts(addon.PapyrusSource, output);
            BuildReceipt.Write(root, BuildComponent.PapyrusAddons, BuildReceipt.CaptureInputs(root, BuildComponent.PapyrusAddons),
                Artifacts.ScriptOutputs(addon.PapyrusSource, output), Path.Combine(output, BuildReceipt.ScriptsFile));
        }
        return papyrus;
    }

    [Fact]
    public void CompilerPublication()
    {
        var staged = Path.Combine(temporary, "staged-core");
        PapyrusCompiler.StageCore(Path.Combine(root, "src/papyrus/core"), staged);
        Assert.True(File.ReadAllText(Path.Combine(staged, "YLIWF_SKSE.psc")).Contains("Hidden Native"), "Caprica annotation missing");
        Assert.False(File.ReadAllText(Path.Combine(root, "src/papyrus/core/YLIWF_SKSE.psc")).Contains("Hidden Native"), "Original source modified");
        var compiler = Path.Combine(temporary, "compiler");
        File.WriteAllText(compiler, "fixture");
        var flags = Path.Combine(temporary, "flags");
        File.WriteAllText(flags, "fixture");
        var compileOutput = Path.Combine(temporary, "compiled");
        Directory.CreateDirectory(Path.Combine(compileOutput, "Scripts"));
        File.WriteAllText(Path.Combine(compileOutput, "Scripts/YLIWF_SKSE.pex"), "old script");
        var calls = 0;
        Assert.Throws<IOException>(() => PapyrusCompiler.Compile(root, compiler, flags, [temporary], compileOutput, true, (_, arguments, _) =>
        {
            if (++calls == 2)
            {
                throw new IOException("Patch compile failure");
            }

            var argv = arguments.ToArray();
            Scripts(argv[0], argv.Single(a => a.StartsWith("--output=", StringComparison.Ordinal))[9..]);
        }));
        Assert.True(File.ReadAllText(Path.Combine(compileOutput, "Scripts/YLIWF_SKSE.pex")) == "old script", "Failed compilation published core outputs");
        var failedStages = Directory.GetDirectories(compileOutput, ".staging-*");
        Assert.True(failedStages.Length == 1, "Failed compiler stage was not retained");
        PapyrusCompiler.Compile(root, compiler, flags, [temporary], compileOutput, true, (_, arguments, _) =>
        {
            var argv = arguments.ToArray();
            Scripts(argv[0], argv.Single(a => a.StartsWith("--output=", StringComparison.Ordinal))[9..]);
        });
        Assert.True(Directory.GetDirectories(compileOutput, ".staging-*").SequenceEqual(failedStages), "Successful compilation left a stage or deleted failed work");
        BuildReceipt.Validate(root, BuildComponent.PapyrusCore,
            Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/core"), Path.Combine(compileOutput, "Scripts")), Path.Combine(compileOutput, "Scripts", BuildReceipt.ScriptsFile));
        BuildReceipt.Validate(root, BuildComponent.Papyrus3Dnpc,
            Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/patches/3dnpc"), Path.Combine(compileOutput, "Patches/3DNPC/Scripts")), Path.Combine(compileOutput, "Patches/3DNPC/Scripts", BuildReceipt.ScriptsFile));
        foreach (var addon in Addon.Read(root).Where(addon => Directory.Exists(addon.PapyrusSource)))
        {
            var output = Path.Combine(compileOutput, "addons", addon.Name, "Scripts");
            BuildReceipt.Validate(root, BuildComponent.PapyrusAddons, Artifacts.ScriptOutputs(addon.PapyrusSource, output),
                Path.Combine(output, BuildReceipt.ScriptsFile));
        }
    }

    [Fact]
    public void AddonFailurePreservesPublishedScriptsAndReceiptsTrackAddonSources()
    {
        var checkout = CopyCheckout("addon-compile");
        foreach (var suffix in new[] { "A", "B" })
        {
            var addonRoot = Path.Combine(checkout, "src/addons/build-fixture-" + suffix);
            Directory.CreateDirectory(Path.Combine(addonRoot, "papyrus"));
            File.WriteAllText(Path.Combine(addonRoot, "addon.json"),
                JsonSerializer.Serialize(new
                {
                    name = "BuildFixture" + suffix,
                    version = "1.0.0",
                    adapterApiVersion = 1
                }));
            File.WriteAllText(Path.Combine(addonRoot, "papyrus/BuildFixture" + suffix + ".psc"),
                "Scriptname BuildFixture" + suffix + " Hidden\n");
        }
        var addons = Addon.Read(checkout);
        var first = addons.Single(addon => addon.Name == "BuildFixtureA");
        var second = addons.Single(addon => addon.Name == "BuildFixtureB");
        var compiler = Path.Combine(temporary, "fixture-compiler");
        var flags = Path.Combine(temporary, "fixture-flags");
        File.WriteAllText(compiler, "fixture");
        File.WriteAllText(flags, "fixture");
        var output = Path.Combine(temporary, "addon-output");
        var firstOutput = Path.Combine(output, "addons", first.Name, "Scripts");
        Directory.CreateDirectory(Path.Combine(output, "Scripts"));
        Directory.CreateDirectory(firstOutput);
        File.WriteAllText(Path.Combine(output, "Scripts/YLIWF_SKSE.pex"), "previous core");
        File.WriteAllText(Path.Combine(firstOutput, "BuildFixtureA.pex"), "previous add-on");
        Assert.Throws<IOException>(() => PapyrusCompiler.Compile(checkout, compiler, flags, [temporary], output, false,
            (_, arguments, _) =>
            {
                var argv = arguments.ToArray();
                if (argv[0] == second.PapyrusSource)
                {
                    throw new IOException("Add-on compilation failed");
                }
                Scripts(argv[0], argv.Single(argument => argument.StartsWith("--output=", StringComparison.Ordinal))[9..]);
            }));
        Assert.Equal("previous core", File.ReadAllText(Path.Combine(output, "Scripts/YLIWF_SKSE.pex")));
        Assert.Equal("previous add-on", File.ReadAllText(Path.Combine(firstOutput, "BuildFixtureA.pex")));
        Assert.False(File.Exists(Path.Combine(firstOutput, BuildReceipt.ScriptsFile)));
        Assert.False(Directory.Exists(Path.Combine(output, "addons", second.Name)));

        PapyrusCompiler.Compile(checkout, compiler, flags, [temporary], output, false, (_, arguments, _) =>
        {
            var argv = arguments.ToArray();
            Scripts(argv[0], argv.Single(argument => argument.StartsWith("--output=", StringComparison.Ordinal))[9..]);
        });
        var compiled = Artifacts.ScriptOutputs(first.PapyrusSource, firstOutput);
        var receipt = Path.Combine(firstOutput, BuildReceipt.ScriptsFile);
        BuildReceipt.Validate(checkout, BuildComponent.PapyrusAddons, compiled, receipt);
        File.AppendAllText(Path.Combine(first.PapyrusSource, "BuildFixtureA.psc"), "; changed source\n");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.PapyrusAddons, compiled, receipt));
        BuildReceipt.Validate(checkout, BuildComponent.PapyrusCore,
            Artifacts.ScriptOutputs(Path.Combine(checkout, "src/papyrus/core"), Path.Combine(output, "Scripts")),
            Path.Combine(output, "Scripts", BuildReceipt.ScriptsFile));
    }

    [Fact]
    public void CultureIndependentHeaders()
    {
        var previousCulture = CultureInfo.CurrentCulture;
        try
        {
            // Turkish casing exposes culture-dependent identifier handling (the Turkish-I problem).
            // https://learn.microsoft.com/en-us/dotnet/standard/base-types/best-practices-strings#ordinal-string-operations
            CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("tr-TR");
            Assert.True(SourcePreparation.ScriptName("; Scriptname LineComment\r\n;/ Scriptname BlockComment /;\r\n{Scriptname Documentation}\r\nSCRIPTNAME Actor extends Form\r\n") == "Actor", "Script header parsing depends on locale or includes comments");
            Fails(() => SourcePreparation.ScriptName("Scriptname Actor\nScriptname Other\n"));

            var headerSource = Path.Combine(temporary, "bridge-header-input");
            var headerOutput = Path.Combine(temporary, "bridge-header-output");
            Directory.CreateDirectory(headerSource);
            File.WriteAllText(Path.Combine(headerSource, "YLIWF_SKSE.psc"), "; bridge\r\nSCRIPTNAME YLIWF_SKSE HIDDEN \t\r\n");
            PapyrusCompiler.StageCore(headerSource, headerOutput);
            Assert.True(File.ReadAllText(Path.Combine(headerOutput, "YLIWF_SKSE.psc")).Contains("SCRIPTNAME YLIWF_SKSE HIDDEN Native", StringComparison.Ordinal), "Bridge header annotation depends on locale or rejects CRLF input");
        }
        finally
        {
            CultureInfo.CurrentCulture = previousCulture;
        }
    }

    [Fact]
    public void ImportPreparation()
    {
        var imports = Path.Combine(temporary, "vanilla");
        Directory.CreateDirectory(imports);
        File.WriteAllText(Path.Combine(imports, "Actor.psc"), "Scriptname Actor extends Form\n");
        var skseArchive = Path.Combine(temporary, "skse.zip");
        using (var archive = ZipFile.Open(skseArchive, ZipArchiveMode.Create))
        {
            void Entry(string name, string text)
            {
                using var writer = new StreamWriter(archive.CreateEntry("skse64-fixture/" + name).Open());
                writer.Write(text);
            }
            Entry("scripts/modified/Actor.psc", "Function Added() Native\n");
            Entry("scripts/modified/SKSE.psc", "Scriptname SKSE Hidden\n");
            Entry("skse64_common/skse_version.h", "#define SKSE_VERSION_INTEGER 2\n#define SKSE_VERSION_INTEGER_MINOR 3\n#define SKSE_VERSION_INTEGER_BETA 1\n");
        }
        var merged = Path.Combine(temporary, "merged");
        SourcePreparation.Prepare(skseArchive, imports, merged, "fixture");
        Assert.True(SourcePreparation.ScriptName(File.ReadAllText(Path.Combine(merged, "Actor.psc"))) == "Actor", "SKSE fragment failed to merge");
        using (var manifest = JsonDocument.Parse(File.ReadAllText(Path.Combine(merged, "manifest.json"))))
        {
            foreach (var item in manifest.RootElement.GetProperty("sources").EnumerateArray())
            {
                Assert.True(item.GetProperty("output_sha256").GetString() == Artifacts.HashFile(Path.Combine(merged, item.GetProperty("name").GetString()!)), "SKSE source hash incorrect");
            }
        }

        Fails(() => SourcePreparation.Prepare(skseArchive, imports, merged, "fixture"));
        File.WriteAllText(Path.Combine(imports, "Actor.psc"), "Scriptname Other\n");
        var rejected = Path.Combine(temporary, "rejected");
        Fails(() => SourcePreparation.Prepare(skseArchive, imports, rejected, "fixture"));
        Assert.False(Directory.Exists(rejected), "Invalid source preparation emitted partial imports");
    }
}
