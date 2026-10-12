using System.IO.Compression;
using System.Text.Json;
using ModTools.Building;

namespace ModTools.Packaging;

public static class SourceSnapshot
{
    private const string ArchiveKind = "corresponding source";
    private static readonly string[] Ports = ["fmt", "rapidcsv", "skse-mcp", "spdlog"];
    public static Dictionary<string, string> ProjectFiles(string root)
    {
        var files = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var name in new[] { "README.md", ProjectPaths.License, ProjectPaths.Notice, ProjectPaths.Credits, ProjectPaths.Identity, "global.json", "CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json", ".editorconfig", ".clang-format", ".gitignore", "setup.ps1", "build.ps1", "lint.ps1" })
        {
            var path = Path.Combine(root, name);
            if (File.Exists(path))
            {
                files.Add(name, path);
            }
        }
        foreach (var directory in new[] { "src", "include", "tools", "tests", "cmake", "assets", "docs" })
        {
            AddDirectory(files, root, directory, directory);
        }

        return files;
    }
    private static void AddDirectory(Dictionary<string, string> files, string root, string directory, string archiveDirectory)
    {
        var path = Path.Combine(root, directory);
        if (!Directory.Exists(path))
        {
            throw new DirectoryNotFoundException(path);
        }

        foreach (var entry in new DirectoryInfo(path).EnumerateFileSystemInfos())
        {
            if ((entry.Attributes & FileAttributes.ReparsePoint) != 0)
            {
                throw new InvalidDataException($"Source snapshot does not follow links: {entry.FullName}");
            }

            if (entry is DirectoryInfo)
            {
                if (entry.Name is ".git" or "bin" or "obj" or "build" or ".vs")
                {
                    continue;
                }

                AddDirectory(files, path, entry.Name, archiveDirectory + "/" + entry.Name);
            }
            else
            {
                files.Add(archiveDirectory + "/" + entry.Name, entry.FullName);
            }
        }
    }
    internal static Dictionary<string, string> NativeDependencyFiles(string commonLib, string vcpkg, string nativeBuild)
    {
        var files = new Dictionary<string, string>(StringComparer.Ordinal);
        AddDirectory(files, Path.GetFullPath(commonLib), ".", "dependencies/CommonLibSSE-NG");
        var installed = Path.Combine(nativeBuild, "vcpkg_installed/x64-windows-static");
        foreach (var directory in new[] { "include", "lib", "share", "debug/lib" })
        {
            if (Directory.Exists(Path.Combine(installed, directory)))
            {
                AddDirectory(files, installed, directory, "installed/" + directory);
            }
        }
        foreach (var port in Ports)
        {
            var directory = Path.Combine(vcpkg, "buildtrees", port, "src");
            var sources = Directory.Exists(directory) ? Directory.GetDirectories(directory, "*.clean") : [];
            if (sources.Length > 1)
            {
                throw new InvalidDataException($"Ambiguous source trees for {port}");
            }
            // Binary caches can restore installed inputs without their source trees.
            // Restoring sources later changes this fingerprint and requires a new receipt.
            if (sources.Length == 1)
            {
                AddDirectory(files, sources[0], ".", "dependencies/" + port);
            }
        }
        foreach (var name in new[] { "CMakeCache.txt", "vcpkg_installed/vcpkg/status" })
        {
            var path = Path.Combine(nativeBuild, name);
            if (!File.Exists(path))
            {
                throw new FileNotFoundException("Native build provenance is missing; configure the build first", path);
            }
            files["native/" + name] = path;
        }
        return files;
    }

    public static string Prepare(string root, string dll, string destination, string commonLib, string vcpkg, string nativeBuild)
    {
        var files = ProjectFiles(root);
        // Snapshot actual patched sources, not a URL to a moving branch.
        AddDirectory(files, Path.GetFullPath(commonLib), ".", "dependencies/CommonLibSSE-NG");
        foreach (var port in Ports)
        {
            var sources = Directory.GetDirectories(Path.Combine(vcpkg, "buildtrees", port, "src"), "*.clean");
            if (sources.Length != 1)
            {
                throw new InvalidDataException($"Expected one unambiguous source tree for {port}; build/restore its exact sources before packaging");
            }

            AddDirectory(files, sources[0], ".", "dependencies/" + port);
            var notices = Path.Combine(nativeBuild, "vcpkg_installed/x64-windows-static/share", port, "copyright");
            files["licenses/" + port + ".txt"] = notices;
        }
        files["licenses/CommonLibSSE-NG.txt"] = Path.Combine(commonLib, ProjectPaths.License);
        foreach (var (name, path) in files.Where(file => file.Key.StartsWith("licenses/", StringComparison.Ordinal)))
        {
            if (!File.Exists(path))
            {
                throw new FileNotFoundException($"Missing dependency notice: {name}", path);
            }
            if (string.IsNullOrWhiteSpace(File.ReadAllText(path)))
            {
                throw new InvalidDataException($"Empty dependency notice: {name}");
            }
        }
        // Compiler commands and CMake caches disclose local paths and environment
        // details. Dependency versions remain useful and contain no build paths.
        foreach (var name in new[] { "vcpkg_installed/vcpkg/status" })
        {
            var path = Path.Combine(nativeBuild, name);
            if (!File.Exists(path))
            {
                throw new FileNotFoundException("Source provenance is missing; use a full build", path);
            }

            files["provenance/" + name] = path;
        }
        var timestamp = ReleaseTimestamp.Read(root);
        var metadata = new Dictionary<string, object>
        {
            [ManifestFields.ArchiveTimestamp] = timestamp.ToUnixTimeSeconds(),
            [ManifestFields.Kind] = ArchiveKind,
            [ManifestFields.DllHash] = Artifacts.HashFile(dll),
            [ManifestFields.Version] = ModInfo.Read(root).Version,
            [ManifestFields.Plugin] = ModInfo.PluginFile,
            [ManifestFields.NativeDependencies] = Ports.Prepend("CommonLibSSE-NG").ToArray(),
            [ManifestFields.BuildInputs] = "See docs/build-release.md and tools/dependencies.json. General-purpose compiler tools and Bethesda Creation Kit imports are obtained separately."
        };
        var addonHashes = new SortedDictionary<string, string>(StringComparer.Ordinal);
        var addonVersions = new SortedDictionary<string, object>(StringComparer.Ordinal);
        foreach (var addon in Addon.Read(root))
        {
            var addonDll = Path.Combine(nativeBuild, addon.DllFile);
            Artifacts.ValidateDll(addonDll);
            addonHashes.Add(addon.DllFile, Artifacts.HashFile(addonDll));
            addonVersions.Add(addon.DllFile, new Dictionary<string, object>
            {
                [ManifestFields.Version] = addon.Version,
                [ManifestFields.AdapterApiVersion] = addon.AdapterApiVersion
            });
        }
        metadata[ManifestFields.AddonHashes] = addonHashes;
        metadata[ManifestFields.Addons] = addonVersions;
        return Artifacts.PrepareZip(destination, files, metadata, timestamp);
    }
    public static void ValidateAddon(string archivePath, string dll, Addon addon)
    {
        Artifacts.ValidateDll(dll);
        using var archive = ZipFile.OpenRead(archivePath);
        using var stream = archive.GetEntry(Artifacts.Manifest)!.Open();
        using var manifest = JsonDocument.Parse(stream);
        if (!manifest.RootElement.TryGetProperty(ManifestFields.AddonHashes, out var hashes) ||
            !hashes.TryGetProperty(addon.DllFile, out var hash) || hash.GetString() != Artifacts.HashFile(dll) ||
            !manifest.RootElement.TryGetProperty(ManifestFields.Addons, out var addons) ||
            !addons.TryGetProperty(addon.DllFile, out var declaration) ||
            !declaration.TryGetProperty(ManifestFields.Version, out var version) || version.ValueKind != JsonValueKind.String ||
            version.GetString() != addon.Version ||
            !declaration.TryGetProperty(ManifestFields.AdapterApiVersion, out var api) || api.ValueKind != JsonValueKind.Number ||
            !api.TryGetUInt32(out var apiVersion) || apiVersion != addon.AdapterApiVersion)
        {
            throw new InvalidDataException($"Source archive does not match the add-on DLL, version or API requirement: {addon.DllFile}");
        }
    }
    public static void Validate(string archivePath, string dll, string? root = null)
    {
        using var archive = ZipFile.OpenRead(archivePath);
        var names = archive.Entries.Select(entry => entry.FullName).ToArray();
        if (names.Distinct(StringComparer.Ordinal).Count() != names.Length)
        {
            throw new InvalidDataException("Duplicate source ZIP entries");
        }

        foreach (var name in names)
        {
            Artifacts.ValidateName(name);
        }

        foreach (var required in new[] { ProjectPaths.License, ProjectPaths.Notice, ProjectPaths.Credits, ProjectPaths.Identity, "build.ps1", "src/native/plugin.cpp", "src/plugin/FollowerPlugin.cs", "src/papyrus/core/DialogueFollowerScript.psc", ProjectPaths.ToolProject })
        {
            if (archive.GetEntry(required) == null)
            {
                throw new InvalidDataException($"Source archive is missing {required}");
            }
        }

        using var stream = (archive.GetEntry(Artifacts.Manifest) ?? throw new InvalidDataException("Missing source manifest")).Open();
        using var manifest = JsonDocument.Parse(stream);
        var metadata = manifest.RootElement;
        using var identityReader = new StreamReader(archive.GetEntry(ProjectPaths.Identity)!.Open());
        var identity = ModIdentity.Parse(identityReader.ReadToEnd());
        if (!metadata.TryGetProperty(ManifestFields.Version, out var coreVersion) || coreVersion.ValueKind != JsonValueKind.String ||
            coreVersion.GetString() != identity.Version)
        {
            throw new InvalidDataException("Source archive version does not match its mod.json");
        }
        if (metadata.GetProperty(ManifestFields.Kind).GetString() != ArchiveKind || metadata.GetProperty(ManifestFields.DllHash).GetString() != Artifacts.HashFile(dll) || metadata.GetProperty(ManifestFields.Plugin).GetString() != ModInfo.PluginFile)
        {
            throw new InvalidDataException("Source archive does not match the supplied DLL and plugin identity");
        }

        foreach (var dependency in Ports.Prepend("CommonLibSSE-NG"))
        {
            if (!names.Any(name => name.StartsWith("dependencies/" + dependency + "/", StringComparison.Ordinal)))
            {
                throw new InvalidDataException($"Missing source dependency: {dependency}");
            }

            using var notice = (archive.GetEntry("licenses/" + dependency + ".txt") ?? throw new InvalidDataException($"Missing dependency notice: {dependency}")).Open();
            using var reader = new StreamReader(notice);
            if (string.IsNullOrWhiteSpace(reader.ReadToEnd()))
            {
                throw new InvalidDataException($"Empty dependency notice: {dependency}");
            }
        }
        var hashes = metadata.GetProperty(ManifestFields.Hashes);
        if (root != null)
        {
            var inputs = ProjectFiles(root).Where(file => IsCompiledInput(file.Key)).ToDictionary();
            var archivedInputs = hashes.EnumerateObject().Select(property => property.Name).Where(IsCompiledInput)
                .ToHashSet(StringComparer.Ordinal);
            if (!archivedInputs.SetEquals(inputs.Keys))
            {
                throw new InvalidDataException("Source archive compiled input files differ from this checkout");
            }
            foreach (var (name, path) in inputs)
            {
                if (!hashes.TryGetProperty(name, out var checksum) || checksum.GetString() != Artifacts.HashFile(path))
                {
                    throw new InvalidDataException($"Source archive does not match this checkout's compiled inputs: {name}");
                }
            }
        }
        if (hashes.EnumerateObject().Count() != names.Length - 1)
        {
            throw new InvalidDataException("Incomplete source checksums");
        }

        foreach (var property in hashes.EnumerateObject())
        {
            using var content = (archive.GetEntry(property.Name) ?? throw new InvalidDataException("Missing source entry")).Open();
            if (Convert.ToHexStringLower(System.Security.Cryptography.SHA256.HashData(content)) != property.Value.GetString())
            {
                throw new InvalidDataException($"Source checksum mismatch: {property.Name}");
            }
        }
    }
    private static bool IsCompiledInput(string name) =>
        name.StartsWith("src/", StringComparison.Ordinal) || name.StartsWith("include/", StringComparison.Ordinal) ||
        name.StartsWith("cmake/", StringComparison.Ordinal) ||
        (name.StartsWith("tools/ModTools/", StringComparison.Ordinal) && name.EndsWith(".cs", StringComparison.Ordinal)) ||
        name is "setup.ps1" or "build.ps1" or "tools/Environment.ps1" or "tools/dependencies.json" or ProjectPaths.PathDefaults ||
        name is "CMakeLists.txt" or "CMakePresets.json" or "assets/settings.ini" or ProjectPaths.Identity or "vcpkg.json" or
            "vcpkg-configuration.json" or "global.json" or ProjectPaths.ToolProject or ProjectPaths.ToolLock;

    public static Dictionary<string, string> ExtractBinaryNotices(string archivePath, string destination)
    {
        using var archive = ZipFile.OpenRead(archivePath);
        var files = new Dictionary<string, string>(StringComparer.Ordinal);
        // fmt permits omitting its notice for compiled code embedded in a binary.
        // Its source and notice remain in the corresponding source archive.
        foreach (var dependency in Ports.Prepend("CommonLibSSE-NG").Where(name => name != "fmt"))
        {
            var name = "licenses/" + dependency + ".txt";
            var entry = archive.GetEntry(name) ?? throw new InvalidDataException($"Missing dependency notice: {dependency}");
            var path = Path.Combine(destination, dependency + ".txt");
            Directory.CreateDirectory(destination);
            entry.ExtractToFile(path);
            files[name] = path;
        }
        return files;
    }
}
