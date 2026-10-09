using System.Buffers.Binary;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using ModTools.Building;

namespace ModTools.Packaging;

public static class Artifacts
{
    private static readonly JsonSerializerOptions ManifestJsonOptions = new() { WriteIndented = true };

    public static string Plugin => ModInfo.PluginFile;
    public static string Dll => ModInfo.Identity.DllFile;
    public static string Manifest => ModInfo.Identity.DocumentationDirectory + "/build-manifest.json";
    public static string Hash(byte[] data) => Convert.ToHexStringLower(SHA256.HashData(data));
    public static string HashFile(string path) => Hash(File.ReadAllBytes(path));
    public static void ValidateDll(string path)
    {
        var data = File.ReadAllBytes(path);
        void Require(bool valid, string message)
        {
            if (!valid)
            {
                throw new InvalidDataException($"{message}: {path}");
            }
        }
        uint ReadUInt32(int offset)
        {
            Require(offset >= 0 && offset <= data.Length - 4, "Truncated DLL");
            return BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(offset, 4));
        }
        ushort ReadUInt16(int offset)
        {
            Require(offset >= 0 && offset <= data.Length - 2, "Truncated DLL");
            return BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(offset, 2));
        }
        Require(data.Length >= 64 && data[0] == 'M' && data[1] == 'Z', "Not a Windows DLL");
        var peHeaderOffset = checked((int)ReadUInt32(60));
        Require(ReadUInt32(peHeaderOffset) == 0x4550, "Invalid PE signature");
        Require(ReadUInt16(peHeaderOffset + 4) == 0x8664 && (ReadUInt16(peHeaderOffset + 22) & 0x2000) != 0, "Expected x64 Windows DLL");
        var sectionCount = ReadUInt16(peHeaderOffset + 6);
        var optionalHeaderSize = ReadUInt16(peHeaderOffset + 20);
        var optionalHeaderOffset = checked(peHeaderOffset + 24);
        var sectionTableOffset = checked(optionalHeaderOffset + optionalHeaderSize);
        Require(optionalHeaderSize >= 120 && (long)sectionTableOffset + sectionCount * 40 <= data.Length && ReadUInt16(optionalHeaderOffset) == 0x20b, "Truncated or non-PE32+ DLL");
        int ResolveFileOffset(uint address, long size)
        {
            for (var i = 0; i < sectionCount; ++i)
            {
                var sectionHeaderOffset = sectionTableOffset + i * 40;
                var virtualAddress = ReadUInt32(sectionHeaderOffset + 12);
                var rawDataSize = ReadUInt32(sectionHeaderOffset + 16);
                var rawDataOffset = ReadUInt32(sectionHeaderOffset + 20);
                var relativeOffset = (long)address - virtualAddress;
                if (relativeOffset >= 0 && relativeOffset < Math.Max(ReadUInt32(sectionHeaderOffset + 8), rawDataSize) && relativeOffset + size <= rawDataSize && rawDataOffset + relativeOffset + size <= data.Length)
                {
                    return checked((int)(rawDataOffset + relativeOffset));
                }
            }
            throw new InvalidDataException($"Invalid DLL export address: {path}");
        }
        Require(ReadUInt32(optionalHeaderOffset + 112) != 0 && ReadUInt32(optionalHeaderOffset + 116) >= 40, "DLL has no exports");
        var exportDirectoryOffset = ResolveFileOffset(ReadUInt32(optionalHeaderOffset + 112), 40);
        var exportNameCount = ReadUInt32(exportDirectoryOffset + 24);
        var exportNameTableOffset = ResolveFileOffset(ReadUInt32(exportDirectoryOffset + 32), (long)exportNameCount * 4);
        var exportedNames = new HashSet<string>(StringComparer.Ordinal);
        for (long i = 0; i < exportNameCount; ++i)
        {
            var nameOffset = ResolveFileOffset(ReadUInt32(checked(exportNameTableOffset + (int)(i * 4))), 1);
            var terminatorOffset = Array.IndexOf(data, (byte)0, nameOffset, Math.Min(256, data.Length - nameOffset));
            Require(terminatorOffset >= 0, "Malformed DLL export name");
            exportedNames.Add(Encoding.ASCII.GetString(data, nameOffset, terminatorOffset - nameOffset));
        }
        Require(exportedNames.Contains("SKSEPlugin_Load") && (exportedNames.Contains("SKSEPlugin_Version") || exportedNames.Contains("SKSEPlugin_Query")), "DLL missing SKSE entry points");
    }
    public static void ValidateEsp(string path)
    {
        using var stream = File.OpenRead(path);
        Span<byte> header = stackalloc byte[24];
        if (stream.Read(header) != 24 || !header[..4].SequenceEqual("TES4"u8))
        {
            throw new InvalidDataException($"Not a TES4 plugin: {path}");
        }

        var size = BinaryPrimitives.ReadUInt32LittleEndian(header[4..]);
        if (size < 18 || stream.Length <= 24L + size)
        {
            throw new InvalidDataException($"Truncated or empty plugin: {path}");
        }
    }
    public static void ValidatePex(string path)
    {
        using var stream = File.OpenRead(path);
        Span<byte> header = stackalloc byte[8];
        if (stream.Read(header) != 8 || !header.SequenceEqual(new byte[] { 0xfa, 0x57, 0xc0, 0xde, 3, 2, 0, 1 }))
        {
            throw new InvalidDataException($"Not a Skyrim 3.2 PEX: {path}");
        }
    }
    public static string[] ScriptOutputs(string sources, string output)
    {
        var files = Directory.GetFiles(sources, "*.psc").Order(StringComparer.Ordinal).Select(source => Path.Combine(output, Path.GetFileNameWithoutExtension(source) + ".pex")).ToArray();
        if (files.Length == 0)
        {
            throw new InvalidDataException($"No scripts in {sources}");
        }

        foreach (var file in files)
        {
            ValidatePex(file);
        }

        return files;
    }
    public static Dictionary<string, string> CoreFiles(string root, string esp, string dll, string scripts, IReadOnlyDictionary<string, string> dependencyNotices)
    {
        ValidateEsp(esp);
        ValidateDll(dll);
        var files = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            [Plugin] = esp,
            [$"SKSE/Plugins/{Dll}"] = dll
        };
        AddNotices(root, files, dependencyNotices);
        foreach (var script in ScriptOutputs(Path.Combine(root, ProjectPaths.CoreScripts), scripts))
        {
            files.Add("Scripts/" + Path.GetFileName(script), script);
        }

        return files;
    }
    public static void AddNotices(string root, Dictionary<string, string> files, IReadOnlyDictionary<string, string> dependencyNotices)
    {
        foreach (var name in ProjectPaths.ReleaseNotices)
        {
            files[$"{ModInfo.Identity.DocumentationDirectory}/{name}"] = Path.Combine(root, name);
        }

        foreach (var (name, path) in dependencyNotices)
        {
            files[$"{ModInfo.Identity.DocumentationDirectory}/{name}"] = path;
        }

    }
    public static void ValidateName(string name)
    {
        if (string.IsNullOrWhiteSpace(name) || name.StartsWith('/') || name.Contains('\\') || name.Contains(':') || name.Split('/').Any(part => part is ".." or "." or ""))
        {
            throw new InvalidDataException($"Unsafe archive path: {name}");
        }
    }
    // Prepare every requested archive before publishing any release.
    public static string PrepareZip(string destination, IReadOnlyDictionary<string, string> files, IReadOnlyDictionary<string, object> metadata, DateTimeOffset timestamp, bool includeManifest = true)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(destination))!);
        var temporary = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(destination))!, "." + Path.GetFileName(destination) + "." + Guid.NewGuid().ToString("N") + ".tmp");
        try
        {
            var hashes = new SortedDictionary<string, string>(StringComparer.Ordinal);
            using (var archive = ZipFile.Open(temporary, ZipArchiveMode.Create))
            {
                foreach (var (name, source) in files.OrderBy(pair => pair.Key, StringComparer.Ordinal))
                {
                    ValidateName(name);
                    if (name == Manifest)
                    {
                        throw new InvalidDataException("Reserved build manifest path");
                    }

                    var bytes = File.ReadAllBytes(source);
                    hashes.Add(name, Hash(bytes));
                    using var entry = CreateEntry(archive, name, timestamp).Open();
                    entry.Write(bytes);
                }
                if (includeManifest)
                {
                    var manifest = metadata.ToDictionary(pair => pair.Key, pair => pair.Value);
                    manifest[ManifestFields.Hashes] = hashes;
                    using var writer = new StreamWriter(CreateEntry(archive, Manifest, timestamp).Open(), new UTF8Encoding(false));
                    writer.Write(JsonSerializer.Serialize(manifest, ManifestJsonOptions));
                }
            }
            using (var archive = ZipFile.OpenRead(temporary))
            {
                if (archive.Entries.Count != files.Count + (includeManifest ? 1 : 0))
                {
                    throw new InvalidDataException("Unexpected ZIP entries");
                }

                foreach (var (name, digest) in hashes)
                {
                    using var stream = archive.GetEntry(name)!.Open();
                    if (Convert.ToHexStringLower(SHA256.HashData(stream)) != digest)
                    {
                        throw new InvalidDataException($"ZIP checksum mismatch: {name}");
                    }
                }
                if (includeManifest)
                {
                    using var manifestStream = archive.GetEntry(Manifest)!.Open();
                    using var verified = JsonDocument.Parse(manifestStream);
                    if (verified.RootElement.GetProperty(ManifestFields.Hashes).EnumerateObject().Count() != hashes.Count)
                    {
                        throw new InvalidDataException("Invalid ZIP manifest");
                    }
                }
            }
            return temporary;
        }
        catch { File.Delete(temporary); throw; }
    }
    private static ZipArchiveEntry CreateEntry(ZipArchive archive, string name, DateTimeOffset timestamp)
    {
        var entry = archive.CreateEntry(name, CompressionLevel.SmallestSize);
        entry.LastWriteTime = timestamp;
        entry.ExternalAttributes = 0;
        return entry;
    }
    public static void Publish(IEnumerable<(string Temporary, string Destination)> archives)
    {
        var group = archives.ToArray();
        var completed = new List<(string Destination, string? Backup)>();
        var published = false;
        try
        {
            foreach (var (temporary, destination) in group)
            {
                var backup = File.Exists(destination) ? destination + "." + Guid.NewGuid().ToString("N") + ".rollback" : null;
                if (backup != null)
                {
                    File.Replace(temporary, destination, backup);
                }
                else
                {
                    File.Move(temporary, destination);
                }

                completed.Add((destination, backup));
            }
            published = true;
        }
        catch
        {
            foreach (var (destination, backup) in completed.AsEnumerable().Reverse())
            {
                if (backup != null)
                {
                    File.Move(backup, destination, true);
                }
                else
                {
                    File.Delete(destination);
                }
            }

            throw;
        }
        finally
        {
            foreach (var (temporary, _) in group)
            {
                File.Delete(temporary);
            }
            // Retain recovery files if rollback itself fails.
            foreach (var (_, backup) in completed)
            {
                if (backup != null && published)
                {
                    File.Delete(backup);
                }
            }
        }
    }
    public static void Package(string root, string esp, string dll, string papyrus, string output, bool patch, BuildMode mode, string? sourceArchive = null, string? commonLib = null, string? vcpkg = null, string? nativeBuild = null)
    {
        BuildReceipt.Validate(root, BuildComponent.Plugin, [esp], BuildReceipt.PluginPath(esp));
        var scripts = Path.Combine(papyrus, ProjectPaths.ScriptsOutput);
        BuildReceipt.Validate(root, BuildComponent.PapyrusCore, ScriptOutputs(Path.Combine(root, ProjectPaths.CoreScripts), scripts), Path.Combine(scripts, BuildReceipt.ScriptsFile));
        if (patch)
        {
            var patchScripts = Path.Combine(papyrus, ProjectPaths.Patch3DnpcOutput);
            BuildReceipt.Validate(root, BuildComponent.Papyrus3Dnpc, ScriptOutputs(Path.Combine(root, ProjectPaths.Patch3DnpcScripts), patchScripts), Path.Combine(patchScripts, BuildReceipt.ScriptsFile));
        }
        var version = File.ReadAllText(Path.Combine(root, ProjectPaths.Version)).Trim();
        if (!Version.TryParse(version, out var parsedVersion) || parsedVersion.Build < 0)
        {
            throw new InvalidDataException("VERSION must contain three or four numeric components");
        }

        var metadata = new Dictionary<string, object>();
        var pending = new List<(string, string)>();
        var noticeDirectory = Path.Combine(Path.GetFullPath(output), ".dependency-notices-" + Guid.NewGuid().ToString("N"));
        try
        {
            var sourceName = $"{ModInfo.BinaryName}-{version}-source.zip";
            var sourceDestination = Path.Combine(output, sourceName);
            string sourceTemporary;
            if (sourceArchive != null)
            {
                SourceSnapshot.Validate(sourceArchive, dll, root);
                Directory.CreateDirectory(output);
                sourceTemporary = sourceDestination + "." + Guid.NewGuid().ToString("N") + ".tmp";
                File.Copy(sourceArchive, sourceTemporary);
                pending.Add((sourceTemporary, sourceDestination));
            }
            else
            {
                if (mode != BuildMode.FullSource || commonLib == null || vcpkg == null || nativeBuild == null)
                {
                    throw new InvalidDataException("Packaging requires the DLL's matching --source-archive, or all source paths from a full build");
                }

                sourceTemporary = SourceSnapshot.Prepare(root, dll, sourceDestination, commonLib, vcpkg, nativeBuild);
                pending.Add((sourceTemporary, sourceDestination));
                SourceSnapshot.Validate(sourceTemporary, dll, root);
            }
            var dependencyNotices = SourceSnapshot.ExtractBinaryNotices(sourceTemporary, noticeDirectory);
            DateTimeOffset timestamp;
            using (var sourceZip = ZipFile.OpenRead(sourceTemporary))
            {
                // Supplied sources retain the original release's date.
                timestamp = new DateTimeOffset(sourceZip.GetEntry(Manifest)!.LastWriteTime.DateTime, TimeSpan.Zero);
            }
            var destination = Path.Combine(output, $"{ModInfo.BinaryName}-{version}.zip");
            pending.Add((PrepareZip(destination, CoreFiles(root, esp, dll, Path.Combine(papyrus, ProjectPaths.ScriptsOutput), dependencyNotices), metadata, timestamp, includeManifest: false), destination));
            foreach (var addon in Addon.Read(root))
            {
                var addonDll = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(dll))!, addon.DllFile);
                if (mode == BuildMode.SuppliedDll && !File.Exists(addonDll))
                {
                    continue;
                }

                SourceSnapshot.ValidateAddon(sourceTemporary, addonDll, addon.DllFile);
                var files = new Dictionary<string, string> { ["SKSE/Plugins/" + addon.DllFile] = addonDll };
                AddNotices(root, files, dependencyNotices);
                destination = Path.Combine(output, $"{ModInfo.BinaryName}-{version}-{addon.Name}.zip");
                pending.Add((PrepareZip(destination, files, metadata, timestamp, includeManifest: false), destination));
            }
            if (patch)
            {
                var files = ScriptOutputs(Path.Combine(root, ProjectPaths.Patch3DnpcScripts), Path.Combine(papyrus, ProjectPaths.Patch3DnpcOutput)).ToDictionary(file => "Scripts/" + Path.GetFileName(file), file => file);
                AddNotices(root, files, dependencyNotices);
                destination = Path.Combine(output, $"{ModInfo.BinaryName}-{version}-3DNPC.zip");
                pending.Add((PrepareZip(destination, files, metadata, timestamp, includeManifest: false), destination));
            }
            Publish(pending);
            foreach (var (_, path) in pending)
            {
                Console.WriteLine($"Published: {path}");
            }
        }
        finally
        {
            foreach (var (temporary, _) in pending)
            {
                File.Delete(temporary);
            }
            if (Directory.Exists(noticeDirectory))
            {
                Directory.Delete(noticeDirectory, true);
            }
        }
    }
}
