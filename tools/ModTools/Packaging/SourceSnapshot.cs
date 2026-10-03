using System.IO.Compression;
using System.Text.Json;

namespace ModTools.Packaging;

public static class SourceSnapshot
{
    private const string ArchiveKind = "corresponding source";
    private static readonly string[] Ports = ["fmt", "rapidcsv", "skse-mcp", "spdlog"];
    public static Dictionary<string, string> ProjectFiles(string root)
    {
        var files = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var name in new[] { "README.md", "LICENSE", "NOTICE", "CREDITS.md", "VERSION", "mod.json", "global.json", "CMakeLists.txt", "CMakePresets.json", "vcpkg.json", "vcpkg-configuration.json", ".editorconfig", ".gitignore", "setup.ps1", "build.ps1" })
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
        files["licenses/CommonLibSSE-NG.txt"] = Path.Combine(commonLib, "LICENSE");
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
        foreach (var name in new[] { "CMakeCache.txt", "compile_commands.json", "vcpkg_installed/vcpkg/status" })
        {
            var path = Path.Combine(nativeBuild, name);
            if (!File.Exists(path))
            {
                throw new FileNotFoundException("Source provenance is missing; use a full build", path);
            }

            files["provenance/" + name] = path;
        }
        return Artifacts.PrepareZip(destination, files, new Dictionary<string, object>
        {
            [ManifestFields.Kind] = ArchiveKind,
            [ManifestFields.DllHash] = Artifacts.HashFile(dll),
            [ManifestFields.Version] = File.ReadAllText(Path.Combine(root, "VERSION")).Trim(),
            [ManifestFields.Plugin] = ModInfo.PluginFile,
            [ManifestFields.NativeDependencies] = Ports.Prepend("CommonLibSSE-NG").ToArray(),
            [ManifestFields.BuildInputs] = "See docs/build-release.md and tools/dependencies.json. General-purpose compiler tools and Bethesda Creation Kit imports are obtained separately."
        });
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

        foreach (var required in new[] { "LICENSE", "NOTICE", "CREDITS.md", "mod.json", "build.ps1", "src/native/plugin.cpp", "src/plugin/FollowerPlugin.cs", "src/papyrus/core/DialogueFollowerScript.psc", "tools/ModTools/ModTools.csproj" })
        {
            if (archive.GetEntry(required) == null)
            {
                throw new InvalidDataException($"Source archive is missing {required}");
            }
        }

        using var stream = (archive.GetEntry(Artifacts.Manifest) ?? throw new InvalidDataException("Missing source manifest")).Open();
        using var manifest = JsonDocument.Parse(stream);
        var metadata = manifest.RootElement;
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
            foreach (var (name, path) in ProjectFiles(root).Where(file => file.Key.StartsWith("src/", StringComparison.Ordinal) ||
                file.Key.StartsWith("include/", StringComparison.Ordinal) || file.Key.StartsWith("cmake/", StringComparison.Ordinal) ||
                (file.Key.StartsWith("tools/ModTools/", StringComparison.Ordinal) && file.Key.EndsWith(".cs", StringComparison.Ordinal)) ||
                file.Key is "setup.ps1" or "build.ps1" or "tools/Environment.ps1" or "tools/dependencies.json" ||
                file.Key is "CMakeLists.txt" or "CMakePresets.json" or "assets/settings.ini" or "mod.json" or "VERSION" or "vcpkg.json" or "vcpkg-configuration.json" or "global.json" or "tools/ModTools/ModTools.csproj" or "tools/ModTools/packages.lock.json"))
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
