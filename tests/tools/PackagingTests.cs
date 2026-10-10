using System.IO.Compression;
using System.Text.Json;
using ModTools.Building;
using ModTools.Packaging;
using static TestSupport;

internal sealed class PackagingTests(string root, string temporary, DateTimeOffset archiveTimestamp,
    string dll, string esp, string papyrus)
{
    private readonly string commonLib = Path.Combine(temporary, "commonlib");
    private readonly string vcpkg = Path.Combine(temporary, "vcpkg");
    private readonly string nativeBuild = Path.Combine(temporary, "native");
    private readonly string sourceArchive = Path.Combine(temporary, "fixture-source.zip");
    private readonly string invalidNotices = Path.Combine(temporary, "invalid-notices.zip");
    private readonly Dictionary<string, string> sourceFiles = SourceSnapshot.ProjectFiles(root);
    private static readonly string[] RequiredSourceFiles = ["README.md", "docs/architecture.md", "docs/debugging.md", "docs/build-release.md", "tools/README.md", "assets/settings.ini", "lint.ps1", ".clang-format"];

    public void Run()
    {
        PrepareDependencies();
        CheckSourceArchive();
        var dependencyNotices = CheckDependencyNotices();
        var files = Artifacts.CoreFiles(root, esp, dll, Path.Combine(papyrus, "Scripts"), dependencyNotices);
        var zip = CheckCoreArchive(files, dependencyNotices);
        CheckZipFailures(zip);
        CheckReleasePublication(dependencyNotices);
        CheckIndependentAddonRelease();
        CheckPublicationRollback(files);
    }

    private void PrepareDependencies()
    {
        Check(RequiredSourceFiles.All(sourceFiles.ContainsKey), "Source archive omitted documentation, lint configuration or default settings");
        Check(!sourceFiles.Keys.Any(name => name.Contains("/bin/", StringComparison.Ordinal) || name.StartsWith("debug-data/", StringComparison.Ordinal) || name.StartsWith(".github/", StringComparison.Ordinal)), "Private data or compiled outputs entered source snapshot");
        Check(sourceFiles.ContainsKey("src/plugin/Records/Quests.cs"), "Source snapshot omitted plugin record definitions");
        // Test-only dependency stubs exercise the source archive contract, not a release.
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
    }

    private void CheckSourceArchive()
    {
        var addon = Addon.Read(root).First();
        var addonDll = Path.Combine(temporary, addon.DllFile);
        File.WriteAllBytes(addonDll, DllFixture());
        foreach (var declaredAddon in Addon.Read(root))
        {
            File.WriteAllBytes(Path.Combine(nativeBuild, declaredAddon.DllFile), DllFixture());
            File.WriteAllBytes(Path.Combine(temporary, declaredAddon.DllFile), DllFixture());
        }
        Artifacts.Publish(new[] { (SourceSnapshot.Prepare(root, dll, sourceArchive, commonLib, vcpkg, nativeBuild), sourceArchive) });
        SourceSnapshot.Validate(sourceArchive, dll, root);
        SourceSnapshot.ValidateAddon(sourceArchive, addonDll, addon);
        Fails(() => SourceSnapshot.ValidateAddon(sourceArchive, addonDll, addon with { Version = "2.3.4" }));
        Fails(() => SourceSnapshot.ValidateAddon(sourceArchive, addonDll, addon with { AdapterApiVersion = 2 }));
        var originalAddonDll = File.ReadAllBytes(addonDll);
        var mismatchedAddonDll = originalAddonDll.ToArray();
        mismatchedAddonDll[16] = 1; // Keep a structurally valid PE, change its identity hash.
        File.WriteAllBytes(addonDll, mismatchedAddonDll);
        Fails(() => SourceSnapshot.ValidateAddon(sourceArchive, addonDll, addon));
        File.WriteAllBytes(addonDll, originalAddonDll);
        Fails(() => SourceSnapshot.ValidateAddon(sourceArchive, Path.Combine(temporary, "missing-adapter.dll"), addon));
        using var snapshot = ZipFile.OpenRead(sourceArchive);
        Check(!snapshot.Entries.Any(entry => entry.FullName is "provenance/CMakeCache.txt" or "provenance/compile_commands.json"),
            "Source snapshot publishes machine-specific compiler paths");
        Check(snapshot.Entries.All(entry => entry.LastWriteTime.DateTime == archiveTimestamp.DateTime && entry.ExternalAttributes == 0),
            "Source ZIP date differs from the release or retains file attributes");
    }

    private Dictionary<string, string> CheckDependencyNotices()
    {
        var sourceMetadata = new Dictionary<string, object> { ["kind"] = "corresponding source", ["dll_sha256"] = Artifacts.HashFile(dll), ["plugin"] = ModInfo.PluginFile };
        var changedDefaults = Path.Combine(temporary, "different-defaults.ini");
        File.WriteAllText(changedDefaults, "[General]\niMaxFollowers=1\n");
        var differentDefaultsFiles = new Dictionary<string, string>(sourceFiles) { ["assets/settings.ini"] = changedDefaults };
        var differentDefaultsArchive = Path.Combine(temporary, "different-defaults.zip");
        Artifacts.Publish(new[] { (Artifacts.PrepareZip(differentDefaultsArchive, differentDefaultsFiles, sourceMetadata, archiveTimestamp), differentDefaultsArchive) });
        Fails(() => SourceSnapshot.Validate(differentDefaultsArchive, dll, root));
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
        return dependencyNotices;
    }

    private string CheckCoreArchive(Dictionary<string, string> files, IReadOnlyDictionary<string, string> dependencyNotices)
    {
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
        return zip;
    }

    private void CheckZipFailures(string zip)
    {
        var previous = File.ReadAllBytes(zip);
        Fails(() => Artifacts.PrepareZip(zip, new Dictionary<string, string> { ["Scripts/missing.pex"] = Path.Combine(temporary, "missing") }, new Dictionary<string, object>(), archiveTimestamp));
        Fails(() => Artifacts.PrepareZip(zip, new Dictionary<string, string> { ["../bad"] = esp }, new Dictionary<string, object>(), archiveTimestamp));
        Check(File.ReadAllBytes(zip).SequenceEqual(previous) && Directory.GetFiles(temporary, ".*.tmp").Length == 0, "Failed ZIP changed release or left temporary files");
    }

    private void CheckReleasePublication(IReadOnlyDictionary<string, string> dependencyNotices)
    {
        File.Delete(Path.Combine(papyrus, "Scripts/YLIWF_SKSE.pex"));
        Fails(() => Artifacts.Package(root, esp, dll, papyrus, temporary, false, BuildMode.SuppliedDll, sourceArchive));
        Scripts(Path.Combine(root, "src/papyrus/core"), Path.Combine(papyrus, "Scripts"));
        var version = ModInfo.Read(root).Version;
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
        CheckPublishedArchives(version, release, dependencyNotices);
    }

    private void CheckPublishedArchives(string version, string release, IReadOnlyDictionary<string, string> dependencyNotices)
    {
        foreach (var declaredAddon in Addon.Read(root))
        {
            using var archive = ZipFile.OpenRead(Path.Combine(temporary, declaredAddon.ArchiveName));
            Check(archive.GetEntry("SKSE/Plugins/" + declaredAddon.DllFile) != null && archive.GetEntry(Artifacts.Plugin) == null &&
                !archive.Entries.Any(entry => entry.FullName.EndsWith(".pex", StringComparison.OrdinalIgnoreCase)), "Native adapter must be a separate DLL-only add-on");
            CheckInstallNotices(archive, dependencyNotices);
            var addonSource = Path.Combine(temporary, declaredAddon.SourceArchiveName);
            SourceSnapshot.Validate(addonSource, dll, root);
            SourceSnapshot.ValidateAddon(addonSource, Path.Combine(temporary, declaredAddon.DllFile), declaredAddon);
        }

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
    }

    private void CheckIndependentAddonRelease()
    {
        var checkout = Path.Combine(temporary, "independent-addon-checkout");
        foreach (var (name, file) in SourceSnapshot.ProjectFiles(root))
        {
            var destination = Path.Combine(checkout, name);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(file, destination);
        }
        var declarationPath = Directory.GetFiles(Path.Combine(checkout, "src/addons"), "addon.json", SearchOption.AllDirectories).First();
        var declaration = System.Text.Json.Nodes.JsonNode.Parse(File.ReadAllText(declarationPath))!.AsObject();
        declaration["version"] = "2.3.4";
        File.WriteAllText(declarationPath, declaration.ToJsonString());
        var addon = Addon.Read(checkout).First(addon => addon.Version == "2.3.4");
        Check(ModInfo.Read(checkout).Version == ModInfo.Read(root).Version, "Add-on update changed the core version");
        var source = Path.Combine(temporary, "independent-source.zip");
        var previousEpoch = Environment.GetEnvironmentVariable("SOURCE_DATE_EPOCH");
        try
        {
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", archiveTimestamp.ToUnixTimeSeconds().ToString(System.Globalization.CultureInfo.InvariantCulture));
            Artifacts.Publish(new[] { (SourceSnapshot.Prepare(checkout, dll, source, commonLib, vcpkg, nativeBuild), source) });
        }
        finally
        {
            Environment.SetEnvironmentVariable("SOURCE_DATE_EPOCH", previousEpoch);
        }
        var output = Path.Combine(temporary, "independent-release");
        Artifacts.Package(checkout, esp, dll, papyrus, output, false, BuildMode.SuppliedDll, source);
        Check(File.Exists(Path.Combine(output, addon.ArchiveName)) && File.Exists(Path.Combine(output, addon.SourceArchiveName)),
            "Add-on update did not publish independently versioned binary and source archives");
        Check(File.Exists(Path.Combine(output, $"{ModInfo.BinaryName}-{ModInfo.Read(root).Version}.zip")), "Core release inherited the add-on version");
        SourceSnapshot.ValidateAddon(Path.Combine(output, addon.SourceArchiveName), Path.Combine(temporary, addon.DllFile), addon);
    }

    private void CheckPublicationRollback(Dictionary<string, string> files)
    {
        // Publication rollback if the second archive cannot be installed.
        var rollback = Path.Combine(temporary, "rollback.zip");
        File.WriteAllText(rollback, "old");
        var newZip = Artifacts.PrepareZip(rollback, files, new Dictionary<string, object>(), archiveTimestamp);
        Fails(() => Artifacts.Publish(new[] { (newZip, rollback), (Path.Combine(temporary, "absent.tmp"), Path.Combine(temporary, "patch.zip")) }));
        Check(File.ReadAllText(rollback) == "old", "Multi-archive rollback failed");
    }
}
