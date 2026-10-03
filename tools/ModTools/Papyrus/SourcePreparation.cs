using System.IO.Compression;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using ModTools.Packaging;

namespace ModTools.Papyrus;

public static partial class SourcePreparation
{
    private static readonly JsonSerializerOptions ManifestJsonOptions = new() { WriteIndented = true };
    private static readonly string[] VersionFields = ["SKSE_VERSION_INTEGER", "SKSE_VERSION_INTEGER_MINOR", "SKSE_VERSION_INTEGER_BETA"];

    public static string? ScriptName(string text)
    {
        var clean = PapyrusCommentsRegex().Replace(text, "");
        var headers = ScriptNameRegex().Matches(clean);
        if (headers.Count > 1)
        {
            throw new InvalidDataException("Multiple Scriptname declarations");
        }

        return headers.Count == 1 ? headers[0].Groups[1].Value : null;
    }
    public static void Prepare(string archivePath, string vanilla, string output, string revision)
    {
        if (Directory.Exists(output))
        {
            throw new IOException($"Choose a fresh source output: {output}");
        }

        var bases = Directory.GetFiles(vanilla, "*.psc").ToDictionary(path => Path.GetFileNameWithoutExtension(path), StringComparer.OrdinalIgnoreCase);
        using var archive = ZipFile.OpenRead(archivePath);
        var root = $"skse64-{revision}/";
        var prefix = root + "scripts/modified/";
        var files = archive.Entries.Where(entry => entry.FullName.StartsWith(prefix, StringComparison.Ordinal) && entry.Name.EndsWith(".psc", StringComparison.OrdinalIgnoreCase)).OrderBy(entry => entry.Name).ToArray();
        if (files.Length == 0)
        {
            throw new InvalidDataException($"Archive does not contain official SKSE revision {revision}");
        }

        var generated = new Dictionary<string, string>();
        var sources = new List<object>();
        static string Read(ZipArchiveEntry entry)
        {
            using var reader = new StreamReader(entry.Open(), Encoding.UTF8, true);
            return reader.ReadToEnd();
        }
        foreach (var entry in files)
        {
            var name = entry.FullName[prefix.Length..];
            if (!ScriptFileNameRegex().IsMatch(name))
            {
                throw new InvalidDataException($"Unexpected SKSE source path: {name}");
            }

            var text = Read(entry);
            bases.TryGetValue(Path.GetFileNameWithoutExtension(name), out var original);
            if (original != null)
            {
                if (ScriptName(text) != null)
                {
                    throw new InvalidDataException($"SKSE {name} is a complete script but vanilla also defines it");
                }

                text = File.ReadAllText(original) + $"\n\n; SKSE64 additions from {revision}\n" + text;
            }
            if (!string.Equals(ScriptName(text), Path.GetFileNameWithoutExtension(name), StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException($"Missing or mismatched Scriptname in {name}; provide its vanilla base");
            }

            generated.Add(name, text);
            sources.Add(new
            {
                name,
                skse_source = entry.FullName,
                vanilla_source = original == null ? null : Path.GetFullPath(original),
                vanilla_sha256 = original == null ? null : Artifacts.HashFile(original),
                output_sha256 = Artifacts.Hash(Encoding.UTF8.GetBytes(text))
            });
        }
        var header = Read(archive.GetEntry(root + "skse64_common/skse_version.h") ?? throw new InvalidDataException("Missing SKSE version header"));
        var version = string.Join('.', VersionFields.Select(field =>
        {
            var match = Regex.Match(header, @"#define\s+" + field + @"\s+(\d+)");
            if (!match.Success)
            {
                throw new InvalidDataException($"Missing version field {field}");
            }

            return match.Groups[1].Value;
        }));
        Directory.CreateDirectory(output);
        foreach (var (name, text) in generated)
        {
            File.WriteAllText(Path.Combine(output, name), text, new UTF8Encoding(false));
        }

        File.WriteAllText(Path.Combine(output, "manifest.json"), JsonSerializer.Serialize(new
        {
            repository = "https://github.com/ianpatt/skse64",
            revision,
            version,
            download_url = $"https://codeload.github.com/ianpatt/skse64/zip/{revision}",
            archive_sha256 = Artifacts.HashFile(archivePath),
            sources
        }, ManifestJsonOptions));
        Console.WriteLine($"Prepared {generated.Count} complete SKSE {version} imports: {output}");
    }

    [GeneratedRegex(@";/[\s\S]*?/;|\{[^}]*\}|;[^\r\n]*")]
    private static partial Regex PapyrusCommentsRegex();

    // Papyrus syntax must parse consistently regardless of the user's locale.
    [GeneratedRegex(@"^\s*scriptname\s+(\w+)", RegexOptions.Multiline | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex ScriptNameRegex();

    [GeneratedRegex(@"^[A-Za-z_][A-Za-z_0-9]*\.psc$", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex ScriptFileNameRegex();
}
