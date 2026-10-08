using System.Buffers.Binary;
using System.Globalization;
using System.IO.Compression;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;
using ModTools.Building;
using ModTools.Packaging;
using ModTools.Papyrus;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

var root = Path.GetFullPath(args.Single());
var archiveTimestamp = ReleaseTimestamp.Read(root);
var temporary = Path.Combine(Path.GetTempPath(), "yliwf-tools-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temporary);
void Check(bool condition, string description)
{
    if (!condition)
    {
        throw new InvalidOperationException(description);
    }
}
void Fails(Action action)
{
    try
    {
        action();
    }
    catch (InvalidDataException) { return; }
    catch (IOException) { return; }
    catch (OverflowException) { return; }
    throw new InvalidOperationException("Expected invalid input to fail");
}
void CheckInstallNotices(ZipArchive archive, IReadOnlyDictionary<string, string> dependencyNotices)
{
    var prefix = ModInfo.Identity.DocumentationDirectory + "/";
    var expected = new[] { "LICENSE", "NOTICE", "CREDITS.md" }.Concat(dependencyNotices.Keys)
        .Select(name => prefix + name).ToHashSet(StringComparer.Ordinal);
    var actual = archive.Entries.Where(entry => entry.FullName.StartsWith("docs/", StringComparison.Ordinal))
        .Select(entry => entry.FullName).ToHashSet(StringComparer.Ordinal);
    Check(actual.SetEquals(expected), "Install ZIP must contain only project and dependency notices under docs");
    Check(!archive.Entries.Any(entry => entry.FullName.EndsWith(".ini", StringComparison.OrdinalIgnoreCase)),
        "Install ZIP must not overwrite user configuration");
    using var reader = new StreamReader(archive.GetEntry(prefix + "NOTICE")!.Open());
    Check(reader.ReadToEnd().Contains("THIS MOD IS NOT MADE, GUARANTEED OR SUPPORTED BY ZENIMAX OR ITS AFFILIATES.", StringComparison.Ordinal),
        "Install ZIP omitted Bethesda disclaimer");
}
byte[] DllFixture()
{
    // Export-table fixture only. It is never published as an installable plugin.
    var bytes = new byte[1024];
    void U32(int offset, uint value) => BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
    void U16(int offset, ushort value) => BinaryPrimitives.WriteUInt16LittleEndian(bytes.AsSpan(offset), value);
    bytes[0] = (byte)'M';
    bytes[1] = (byte)'Z';
    U32(60, 128);
    U32(128, 0x4550);
    U16(132, 0x8664);
    U16(134, 1);
    U16(148, 240);
    U16(150, 0x2000);
    U16(152, 0x20b);
    U32(264, 0x1000);
    U32(268, 40);
    U32(400, 512);
    U32(404, 0x1000);
    U32(408, 512);
    U32(412, 512);
    U32(536, 2);
    U32(544, 0x1040);
    U32(576, 0x1060);
    U32(580, 0x1080);
    Encoding.ASCII.GetBytes("SKSEPlugin_Load\0").CopyTo(bytes, 608);
    Encoding.ASCII.GetBytes("SKSEPlugin_Version\0").CopyTo(bytes, 640);
    return bytes;
}
void Scripts(string source, string output)
{
    Directory.CreateDirectory(output);
    foreach (var file in Directory.GetFiles(source, "*.psc"))
    {
        File.WriteAllBytes(Path.Combine(output, Path.GetFileNameWithoutExtension(file) + ".pex"), PexFixture(Path.GetFileName(file)));
    }
}
byte[] PexFixture(string filename)
{
    using var stream = new MemoryStream();
    stream.Write(new byte[] { 0xfa, 0x57, 0xc0, 0xde, 3, 2, 0, 1 });
    stream.Write(Enumerable.Repeat((byte)123, 8).ToArray());
    Span<byte> length = stackalloc byte[2];
    foreach (var text in new[] { "C:\\Users\\PrivateUser\\repo\\" + filename, "PrivateUser", "PrivateComputer" })
    {
        var bytes = Encoding.UTF8.GetBytes(text);
        BinaryPrimitives.WriteUInt16BigEndian(length, checked((ushort)bytes.Length));
        stream.Write(length);
        stream.Write(bytes);
    }
    stream.Write(new byte[] { 0, 0, 1 }); // Empty string table; debug section present.
    stream.Write(Enumerable.Repeat((byte)124, 8).ToArray());
    stream.Write(new byte[] { 0, 0, 0, 0, 0, 0 }); // Empty debug, flag and object tables.
    return stream.ToArray();
}
try
{
    BuildTests.Run(root, temporary);
    var previousEpoch = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
    try
    {
        Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "1791462459");
        var expectedDate = DateTimeOffset.FromUnixTimeSeconds(1791462458);
        Check(ReleaseTimestamp.Read(root) == expectedDate, "Explicit release date was not rounded to ZIP precision");
        Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "invalid");
        Fails(() => ReleaseTimestamp.Read(root));
        Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", "0");
        Fails(() => ReleaseTimestamp.Read(root));
        Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", null);
        var extractedSource = Path.Combine(temporary, "extracted-source");
        var extractedManifest = Path.Combine(extractedSource, Artifacts.Manifest);
        Directory.CreateDirectory(Path.GetDirectoryName(extractedManifest)!);
        File.WriteAllText(extractedManifest, "{\"archive_timestamp\":1791462458}");
        Check(ReleaseTimestamp.Read(extractedSource) == expectedDate, "Extracted sources lost the original release date");
        Fails(() => ReleaseTimestamp.Read(temporary));
    }
    finally
    {
        Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", previousEpoch);
    }
    var privatePex = PexFixture("Test.psc");
    var cleanPex = PexMetadata.Sanitize(privatePex);
    Check(!Encoding.UTF8.GetString(cleanPex).Contains("Private", StringComparison.Ordinal), "PEX retains machine identity");
    Check(cleanPex.AsSpan(8, 8).SequenceEqual(new byte[8]), "PEX retains compile time");
    Check(cleanPex.AsSpan(33, 8).SequenceEqual(new byte[8]), "PEX retains debug modification time");
    Check(cleanPex.AsSpan(41).SequenceEqual(privatePex.AsSpan(privatePex.Length - 6)), "PEX metadata sanitation changed debug tables or bytecode");
    Check(PexMetadata.Sanitize(cleanPex).SequenceEqual(cleanPex), "PEX sanitation is not idempotent");
    Fails(() => PexMetadata.Sanitize(privatePex[..20]));
    var identityJson = File.ReadAllText(Path.Combine(root, "mod.json"));
    Check(ModInfo.Identity == ModIdentity.Parse(identityJson), "Embedded identity is stale; rebuild ModTools after editing mod.json");
    var branding = JsonNode.Parse(identityJson)!.AsObject();
    branding["displayName"] = "Followers \"Together\"";
    branding["shortName"] = "FT";
    var renamed = ModIdentity.Parse(branding.ToJsonString());
    Check(renamed.DisplayName == "Followers \"Together\"" && renamed.ShortName == "FT", "Branding did not update");
    Check(renamed.PluginFile == ModInfo.PluginFile && renamed.DllFile == ModInfo.Identity.DllFile &&
        renamed.IniFile == ModInfo.Identity.IniFile && renamed.NativeScript == ModInfo.NativeScript,
        "Display rename changed compatibility identities");
    foreach (var (field, invalidName) in new[] { ("binaryName", "../outside"), ("scriptPrefix", "bad-prefix"), ("pluginFile", "../Other.esp"), ("displayName", "") })
    {
        var invalidIdentity = JsonNode.Parse(identityJson)!.AsObject();
        invalidIdentity[field] = invalidName;
        Fails(() => ModIdentity.Parse(invalidIdentity.ToJsonString()));
    }

    ProtocolTests.Run(root);

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

    var mod = FollowerPlugin.Create();
    var esp = Path.Combine(temporary, Artifacts.Plugin);
    mod.BeginWrite.ToPath(esp).WithLoadOrderFromHeaderMasters().WithNoDataFolder().NoMastersListContentCheck().Write();
    Artifacts.ValidateEsp(esp);
    BuildReceipt.Write(root, BuildComponent.Plugin, BuildReceipt.CaptureInputs(root, BuildComponent.Plugin), [esp], BuildReceipt.PluginPath(esp));
    using (var loaded = SkyrimMod.CreateFromBinaryOverlay(esp, SkyrimRelease.SkyrimSE))
    {
        Check(loaded.EnumerateMajorRecords().Count() == 62, "Record count changed");
        Check(loaded.ModHeader.Author == ModInfo.Identity.Author, "Plugin header did not use centralized author");
        var questKey = FormKey.Factory("0750BA:Skyrim.esm");
        var quest = loaded.Quests.Single(q => q.FormKey == questKey);
        Check(quest.EditorID == "DialogueFollower", "Wrong follower quest");
        var script = quest.VirtualMachineAdapter!.Scripts.Single(s => s.Name == "DialogueFollowerScript");
        Check(!script.Properties.Any(p => p.Name is "ExtraAliases" or "CanRecruitMore" or "CurrentFollowerCount"), "Native bindings leaked back into the vanilla facade");
        Check(((IScriptObjectPropertyGetter)script.Properties.Single(p => p.Name == "pPlayerFollowerCount")).Object.FormKey == FormKey.Factory("0BCC98:Skyrim.esm"), "Vanilla follower count binding changed");
        Check(quest.VirtualMachineAdapter.Aliases.Count == 1 && quest.VirtualMachineAdapter.Aliases.Single().Property.Alias == 1 &&
            quest.VirtualMachineAdapter.Aliases.Single().Scripts.Single().Name == "TrainedAnimalScript", "Animal listener lost or humanoid Papyrus listeners retained");

        foreach (var id in new uint[] { 0, 2, 3, 4, 5, 6, 7, 8 })
        {
            var alias = quest.Aliases.Single(a => a.ID == id);
            Check(alias.Name == (id == 0 ? "Follower" : $"ExtraFollower{id - 1:00}"), "Alias identity changed");
            Check(alias.PackageData.Count >= 3, "Follower packages missing");
            foreach (var package in alias.PackageData.Where(p => p.FormKey.ModKey == mod.ModKey))
            {
                Check(loaded.Packages.Any(p => p.FormKey == package.FormKey), "Missing alias package record");
            }
        }
    }

    var papyrus = Path.Combine(temporary, "papyrus");
    Scripts(Path.Combine(root, "src/papyrus/core"), Path.Combine(papyrus, "Scripts"));
    BuildReceipt.Write(root, BuildComponent.PapyrusCore, BuildReceipt.CaptureInputs(root, BuildComponent.PapyrusCore),
        Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/core"), Path.Combine(papyrus, "Scripts")), Path.Combine(papyrus, "Scripts", BuildReceipt.ScriptsFile));
    File.WriteAllText(Path.Combine(papyrus, "Scripts/Game.pex"), "stale import");
    var sourceFiles = SourceSnapshot.ProjectFiles(root);
    Check(new[] { "README.md", "docs/architecture.md", "docs/debugging.md", "docs/build-release.md", "tools/README.md", "assets/settings.ini" }
        .All(sourceFiles.ContainsKey), "Source archive omitted documentation or default settings");
    Check(!sourceFiles.Keys.Any(name => name.Contains("/bin/", StringComparison.Ordinal) || name.StartsWith("debug-data/", StringComparison.Ordinal) || name.StartsWith(".github/", StringComparison.Ordinal)), "Private data or compiled outputs entered source snapshot");
    Check(sourceFiles.ContainsKey("src/plugin/Records/Quests.cs"), "Source snapshot omitted plugin record definitions");
    // Test-only dependency stubs exercise the source archive contract, not a release.
    var commonLib = Path.Combine(temporary, "commonlib");
    var vcpkg = Path.Combine(temporary, "vcpkg");
    var nativeBuild = Path.Combine(temporary, "native");
    foreach (var dependency in new[] { "CommonLibSSE-NG", "fmt", "rapidcsv", "skse-mcp", "spdlog" })
    {
        var source = dependency == "CommonLibSSE-NG" ? commonLib : Path.Combine(vcpkg, "buildtrees", dependency, "src", "fixture.clean");
        Directory.CreateDirectory(source);
        var license = Path.Combine(source, "LICENSE");
        File.WriteAllText(license, $"Test-only {dependency} source notice");
        var notice = license;
        if (dependency != "CommonLibSSE-NG")
        {
            var share = Path.Combine(nativeBuild, "vcpkg_installed/x64-windows-static/share", dependency);
            Directory.CreateDirectory(share);
            notice = Path.Combine(share, "copyright");
            File.WriteAllText(notice, $"Test-only {dependency} installed notice");
        }
        sourceFiles.Add("dependencies/" + dependency + "/LICENSE", license);
        sourceFiles.Add("licenses/" + dependency + ".txt", notice);
    }
    foreach (var name in new[] { "CMakeCache.txt", "compile_commands.json", "vcpkg_installed/vcpkg/status" })
    {
        var path = Path.Combine(nativeBuild, name);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, "Test-only source provenance");
    }

    var sourceArchive = Path.Combine(temporary, "fixture-source.zip");
    Artifacts.Publish(new[] { (SourceSnapshot.Prepare(root, dll, sourceArchive, commonLib, vcpkg, nativeBuild), sourceArchive) });
    SourceSnapshot.Validate(sourceArchive, dll, root);
    using (var snapshot = ZipFile.OpenRead(sourceArchive))
    {
        Check(!snapshot.Entries.Any(entry => entry.FullName is "provenance/CMakeCache.txt" or "provenance/compile_commands.json"),
            "Source snapshot publishes machine-specific compiler paths");
        Check(snapshot.Entries.All(entry => entry.LastWriteTime.DateTime == archiveTimestamp.DateTime && entry.ExternalAttributes == 0),
            "Source ZIP date differs from the release or retains file attributes");
    }
    var sourceMetadata = new Dictionary<string, object> { ["kind"] = "corresponding source", ["dll_sha256"] = Artifacts.HashFile(dll), ["plugin"] = ModInfo.PluginFile };
    var changedDefaults = Path.Combine(temporary, "different-defaults.ini");
    File.WriteAllText(changedDefaults, "[General]\niMaxFollowers=1\n");
    var differentDefaultsFiles = new Dictionary<string, string>(sourceFiles) { ["assets/settings.ini"] = changedDefaults };
    var differentDefaultsArchive = Path.Combine(temporary, "different-defaults.zip");
    Artifacts.Publish(new[] { (Artifacts.PrepareZip(differentDefaultsArchive, differentDefaultsFiles, sourceMetadata, archiveTimestamp), differentDefaultsArchive) });
    Fails(() => SourceSnapshot.Validate(differentDefaultsArchive, dll, root));
    var invalidNotices = Path.Combine(temporary, "invalid-notices.zip");
    var missingNoticeFiles = new Dictionary<string, string>(sourceFiles);
    missingNoticeFiles.Remove("licenses/spdlog.txt");
    Artifacts.Publish(new[] { (Artifacts.PrepareZip(invalidNotices, missingNoticeFiles, sourceMetadata, archiveTimestamp), invalidNotices) });
    Fails(() => SourceSnapshot.Validate(invalidNotices, dll));
    var installedFmtNotice = sourceFiles["licenses/fmt.txt"];
    var originalFmtNotice = File.ReadAllText(installedFmtNotice);
    var originalSourceArchive = File.ReadAllBytes(sourceArchive);
    File.Delete(installedFmtNotice);
    Fails(() => SourceSnapshot.Prepare(root, dll, sourceArchive, commonLib, vcpkg, nativeBuild));
    File.WriteAllText(installedFmtNotice, " \n");
    Fails(() => SourceSnapshot.Prepare(root, dll, sourceArchive, commonLib, vcpkg, nativeBuild));
    Artifacts.Publish(new[] { (Artifacts.PrepareZip(invalidNotices, sourceFiles, sourceMetadata, archiveTimestamp), invalidNotices) });
    Fails(() => SourceSnapshot.Validate(invalidNotices, dll));
    Check(File.ReadAllBytes(sourceArchive).SequenceEqual(originalSourceArchive), "Missing or empty notice changed the existing source archive");
    File.WriteAllText(installedFmtNotice, originalFmtNotice);
    var dependencyNotices = SourceSnapshot.ExtractBinaryNotices(sourceArchive, Path.Combine(temporary, "notices"));
    Check(dependencyNotices.Count == 4 && !dependencyNotices.ContainsKey("licenses/fmt.txt"), "Binary notice selection did not honor fmt's exception");
    var files = Artifacts.CoreFiles(root, esp, dll, Path.Combine(papyrus, "Scripts"), dependencyNotices);
    var mismatchedDll = Path.Combine(temporary, "mismatched.dll");
    File.WriteAllBytes(mismatchedDll, [1, 2, 3]);
    Fails(() => SourceSnapshot.Validate(sourceArchive, mismatchedDll));
    var zip = Path.Combine(temporary, "mod.zip");
    Artifacts.Publish(new[] { (Artifacts.PrepareZip(zip, files, new Dictionary<string, object> { ["version"] = "test" }, archiveTimestamp), zip) });
    using (var archive = ZipFile.OpenRead(zip))
    {
        Check(archive.Entries.All(entry => entry.LastWriteTime.DateTime == archiveTimestamp.DateTime), "ZIP entries have inconsistent release dates");
        var names = archive.Entries.Select(e => e.FullName).ToArray();
        Check(names.Contains(Artifacts.Plugin) && names.Contains("SKSE/Plugins/" + Artifacts.Dll), "Missing plugin or DLL");
        Check(names.Count(n => n.EndsWith(".pex", StringComparison.Ordinal)) == 5 && !names.Contains("Scripts/Game.pex"), "Missing scripts or stale outputs entered archive");
        Check(!names.Contains("Scripts/YLIWF_State.pex") && !names.Contains("Scripts/YLIWF_Log.pex") && !names.Contains("Scripts/FollowerAliasScript.pex") && !names.Contains("Scripts/YLIWF_FollowerAliasScript.pex"), "Obsolete scripts packaged");
        Check(names.Contains(ModInfo.Identity.DocumentationDirectory + "/NOTICE") && names.Contains(ModInfo.Identity.DocumentationDirectory + "/licenses/CommonLibSSE-NG.txt"), "Release omitted attribution and third-party notices");
        Check(!names.Contains(ModInfo.Identity.DocumentationDirectory + "/licenses/fmt.txt"), "Binary archive included fmt's optional notice");
        foreach (var (name, path) in dependencyNotices)
        {
            using var reader = new StreamReader(archive.GetEntry(ModInfo.Identity.DocumentationDirectory + "/" + name)!.Open());
            Check(reader.ReadToEnd() == File.ReadAllText(path), "Release notice did not come from the matching dependency snapshot");
        }
        Check(!names.Any(n => n.StartsWith("Data/", StringComparison.Ordinal) || n.EndsWith(".psc", StringComparison.Ordinal)), "Invalid install layout");
        using var manifestStream = archive.GetEntry(Artifacts.Manifest)!.Open();
        using var manifest = JsonDocument.Parse(manifestStream);
        foreach (var property in manifest.RootElement.GetProperty("sha256").EnumerateObject())
        {
            using var stream = archive.GetEntry(property.Name)!.Open();
            using var bytes = new MemoryStream();
            stream.CopyTo(bytes);
            Check(Artifacts.Hash(bytes.ToArray()) == property.Value.GetString(), "Wrong ZIP manifest hash");
        }
    }
    var previous = File.ReadAllBytes(zip);
    Fails(() => Artifacts.PrepareZip(zip, new Dictionary<string, string> { ["Scripts/missing.pex"] = Path.Combine(temporary, "missing") }, new Dictionary<string, object>(), archiveTimestamp));
    Fails(() => Artifacts.PrepareZip(zip, new Dictionary<string, string> { ["../bad"] = esp }, new Dictionary<string, object>(), archiveTimestamp));
    Check(File.ReadAllBytes(zip).SequenceEqual(previous) && Directory.GetFiles(temporary, ".*.tmp").Length == 0, "Failed ZIP changed release or left temporary files");
    File.Delete(Path.Combine(papyrus, "Scripts/YLIWF_SKSE.pex"));
    Fails(() => Artifacts.Package(root, esp, dll, papyrus, temporary, false, BuildMode.SuppliedDll, sourceArchive));
    Scripts(Path.Combine(root, "src/papyrus/core"), Path.Combine(papyrus, "Scripts"));
    var version = File.ReadAllText(Path.Combine(root, "VERSION")).Trim();
    var release = Path.Combine(temporary, $"{ModInfo.BinaryName}-{version}.zip");
    File.WriteAllText(release, "previous release");
    BuildReceiptTests.Run(root, temporary, esp);
    var corePex = Path.Combine(papyrus, "Scripts/YLIWF_SKSE.pex");
    var originalPex = File.ReadAllBytes(corePex);
    File.AppendAllText(corePex, "stale compiled script");
    Artifacts.ValidatePex(corePex);
    Fails(() => Artifacts.Package(root, esp, dll, papyrus, temporary, false, BuildMode.SuppliedDll, sourceArchive));
    Check(File.ReadAllText(release) == "previous release", "Mismatched PEX replaced the release");
    File.WriteAllBytes(corePex, originalPex);
    Fails(() => Artifacts.Package(root, esp, dll, papyrus, temporary, false, BuildMode.SuppliedDll, invalidNotices));
    Check(File.ReadAllText(release) == "previous release", "Invalid dependency notices replaced the existing release");
    Fails(() => Artifacts.Package(root, esp, dll, papyrus, temporary, true, BuildMode.SuppliedDll, sourceArchive));
    Check(File.ReadAllText(release) == "previous release", "Failed patch replaced core archive");
    Scripts(Path.Combine(root, "src/papyrus/patches/3dnpc"), Path.Combine(papyrus, "Patches/3DNPC/Scripts"));
    BuildReceipt.Write(root, BuildComponent.Papyrus3Dnpc, BuildReceipt.CaptureInputs(root, BuildComponent.Papyrus3Dnpc),
        Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/patches/3dnpc"), Path.Combine(papyrus, "Patches/3DNPC/Scripts")), Path.Combine(papyrus, "Patches/3DNPC/Scripts", BuildReceipt.ScriptsFile));
    Fails(() => Artifacts.Package(root, esp, dll, papyrus, temporary, false, BuildMode.SuppliedDll));
    Artifacts.Package(root, esp, dll, papyrus, temporary, true, BuildMode.SuppliedDll, sourceArchive);
    using (var archive = ZipFile.OpenRead(Path.Combine(temporary, $"{ModInfo.BinaryName}-{version}-3DNPC.zip")))
    {
        Check(archive.GetEntry("Scripts/follower3dnpc.pex") != null && archive.GetEntry(Artifacts.Plugin) == null, "Patch was not separate");
        Check(archive.GetEntry(ModInfo.Identity.DocumentationDirectory + "/LICENSE") != null, "Patch omitted its license");
        CheckInstallNotices(archive, dependencyNotices);
    }
    Check(Directory.GetDirectories(temporary, ".dependency-notices-*").Length == 0, "Packaging left temporary dependency notices");
    using (var archive = ZipFile.OpenRead(release))
    {
        CheckInstallNotices(archive, dependencyNotices);
        foreach (var (name, path) in dependencyNotices)
        {
            using var reader = new StreamReader(archive.GetEntry(ModInfo.Identity.DocumentationDirectory + "/" + name)!.Open());
            Check(reader.ReadToEnd() == File.ReadAllText(path), "Supplied-DLL packaging did not preserve source archive notices");
        }
        Check(archive.GetEntry(ModInfo.Identity.DocumentationDirectory + "/licenses/fmt.txt") == null, "Supplied-DLL packaging included fmt's optional notice");
    }

    // Publication rollback if the second archive cannot be installed.
    var rollback = Path.Combine(temporary, "rollback.zip");
    File.WriteAllText(rollback, "old");
    var newZip = Artifacts.PrepareZip(rollback, files, new Dictionary<string, object>(), archiveTimestamp);
    Fails(() => Artifacts.Publish(new[] { (newZip, rollback), (Path.Combine(temporary, "absent.tmp"), Path.Combine(temporary, "patch.zip")) }));
    Check(File.ReadAllText(rollback) == "old", "Multi-archive rollback failed");

    var staged = Path.Combine(temporary, "staged-core");
    PapyrusCompiler.StageCore(Path.Combine(root, "src/papyrus/core"), staged);
    Check(File.ReadAllText(Path.Combine(staged, "YLIWF_SKSE.psc")).Contains("Hidden Native"), "Caprica annotation missing");
    Check(!File.ReadAllText(Path.Combine(root, "src/papyrus/core/YLIWF_SKSE.psc")).Contains("Hidden Native"), "Original source modified");
    var compiler = Path.Combine(temporary, "compiler");
    File.WriteAllText(compiler, "fixture");
    var flags = Path.Combine(temporary, "flags");
    File.WriteAllText(flags, "fixture");
    var compileOutput = Path.Combine(temporary, "compiled");
    Directory.CreateDirectory(Path.Combine(compileOutput, "Scripts"));
    File.WriteAllText(Path.Combine(compileOutput, "Scripts/YLIWF_SKSE.pex"), "old script");
    var calls = 0;
    Fails(() => PapyrusCompiler.Compile(root, compiler, flags, [temporary], compileOutput, true, (_, arguments, _) =>
    {
        if (++calls == 2)
        {
            throw new IOException("Patch compile failure");
        }

        var argv = arguments.ToArray();
        Scripts(argv[0], argv.Single(a => a.StartsWith("--output=", StringComparison.Ordinal))[9..]);
    }));
    Check(File.ReadAllText(Path.Combine(compileOutput, "Scripts/YLIWF_SKSE.pex")) == "old script", "Failed compilation published core outputs");
    var failedStages = Directory.GetDirectories(compileOutput, ".staging-*");
    Check(failedStages.Length == 1, "Failed compiler stage was not retained");
    PapyrusCompiler.Compile(root, compiler, flags, [temporary], compileOutput, true, (_, arguments, _) =>
    {
        var argv = arguments.ToArray();
        Scripts(argv[0], argv.Single(a => a.StartsWith("--output=", StringComparison.Ordinal))[9..]);
    });
    Check(Directory.GetDirectories(compileOutput, ".staging-*").SequenceEqual(failedStages), "Successful compilation left a stage or deleted failed work");
    BuildReceipt.Validate(root, BuildComponent.PapyrusCore,
        Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/core"), Path.Combine(compileOutput, "Scripts")), Path.Combine(compileOutput, "Scripts", BuildReceipt.ScriptsFile));
    BuildReceipt.Validate(root, BuildComponent.Papyrus3Dnpc,
        Artifacts.ScriptOutputs(Path.Combine(root, "src/papyrus/patches/3dnpc"), Path.Combine(compileOutput, "Patches/3DNPC/Scripts")), Path.Combine(compileOutput, "Patches/3DNPC/Scripts", BuildReceipt.ScriptsFile));

    var previousCulture = CultureInfo.CurrentCulture;
    try
    {
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("tr-TR");
        Check(SourcePreparation.ScriptName("; Scriptname LineComment\r\n;/ Scriptname BlockComment /;\r\n{Scriptname Documentation}\r\nSCRIPTNAME Actor extends Form\r\n") == "Actor", "Script header parsing depends on locale or includes comments");
        Fails(() => SourcePreparation.ScriptName("Scriptname Actor\nScriptname Other\n"));

        var headerSource = Path.Combine(temporary, "bridge-header-input");
        var headerOutput = Path.Combine(temporary, "bridge-header-output");
        Directory.CreateDirectory(headerSource);
        File.WriteAllText(Path.Combine(headerSource, "YLIWF_SKSE.psc"), "; bridge\r\nSCRIPTNAME YLIWF_SKSE HIDDEN \t\r\n");
        PapyrusCompiler.StageCore(headerSource, headerOutput);
        Check(File.ReadAllText(Path.Combine(headerOutput, "YLIWF_SKSE.psc")).Contains("SCRIPTNAME YLIWF_SKSE HIDDEN Native", StringComparison.Ordinal), "Bridge header annotation depends on locale or rejects CRLF input");
    }
    finally
    {
        CultureInfo.CurrentCulture = previousCulture;
    }

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
    Check(SourcePreparation.ScriptName(File.ReadAllText(Path.Combine(merged, "Actor.psc"))) == "Actor", "SKSE fragment failed to merge");
    using (var manifest = JsonDocument.Parse(File.ReadAllText(Path.Combine(merged, "manifest.json"))))
    {
        foreach (var item in manifest.RootElement.GetProperty("sources").EnumerateArray())
        {
            Check(item.GetProperty("output_sha256").GetString() == Artifacts.HashFile(Path.Combine(merged, item.GetProperty("name").GetString()!)), "SKSE source hash incorrect");
        }
    }

    Fails(() => SourcePreparation.Prepare(skseArchive, imports, merged, "fixture"));
    File.WriteAllText(Path.Combine(imports, "Actor.psc"), "Scriptname Other\n");
    var rejected = Path.Combine(temporary, "rejected");
    Fails(() => SourcePreparation.Prepare(skseArchive, imports, rejected, "fixture"));
    Check(!Directory.Exists(rejected), "Invalid source preparation emitted partial imports");
    Console.WriteLine("Tooling tests passed: Papyrus/native contracts, generated records, PE/PEX/ESP rejection, ZIP layout and hashes, matching source and notices, release preservation, optional patch, rollback, source staging, compiler failure.");
    return 0;
}
catch (Exception error) { Console.Error.WriteLine(error); return 1; }
finally { Directory.Delete(temporary, true); }
