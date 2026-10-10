using System.Text.Json;

namespace Yliwf;

// Shared by plugin authoring and build tools; embedded so commands also work outside the repo.
public sealed record ModIdentity(string DisplayName, string ShortName, string PluginFile, string BinaryName, string ScriptPrefix, string Author, string Version)
{
    private static readonly System.Buffers.SearchValues<char> s_myChars = System.Buffers.SearchValues.Create("<>:\"/\\|?*;");

    public string DllFile => BinaryName + ".dll";
    public string IniFile => BinaryName + ".ini";
    public string DocumentationDirectory => "docs/" + BinaryName;
    public string NativeScript => ScriptPrefix + "_SKSE";

    public static ModIdentity Parse(string json)
    {
        using var document = JsonDocument.Parse(json);
        string Required(string name)
        {
            if (!document.RootElement.TryGetProperty(name, out var field) || field.ValueKind != JsonValueKind.String)
            {
                throw new InvalidDataException($"mod.json: missing string field {name}");
            }
            var value = field.GetString();
            if (string.IsNullOrWhiteSpace(value) || value.Any(char.IsControl))
            {
                throw new InvalidDataException($"mod.json: {name} must be nonempty text without control characters");
            }
            return value;
        }
        var version = ReleaseVersion.Parse(Required("version"), "mod.json version");
        var identity = new ModIdentity(Required("displayName"), Required("shortName"), Required("pluginFile"), Required("binaryName"), Required("scriptPrefix"), Required("author"), version);
        foreach (var value in new[] { identity.ShortName, identity.BinaryName, identity.ScriptPrefix })
        {
            if (!(char.IsAsciiLetter(value[0]) || value[0] == '_') || value.Any(c => !(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-')))
            {
                throw new InvalidDataException("mod.json: shortName, binaryName and scriptPrefix must be identifiers");
            }
        }
        if (identity.ScriptPrefix.Contains('-') || identity.PluginFile.AsSpan().IndexOfAny(s_myChars) >= 0 ||
            !identity.PluginFile.EndsWith(".esp", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException("mod.json: invalid script prefix or plugin filename");
        }
        return identity;
    }
}

public static class ModInfo
{
    public static ModIdentity Identity { get; } = Load();
    public static string DisplayName => Identity.DisplayName;
    public static string ShortName => Identity.ShortName;
    public static string PluginFile => Identity.PluginFile;
    public static string BinaryName => Identity.BinaryName;
    public static string NativeScript => Identity.NativeScript;

    public static ModIdentity Read(string root) => ModIdentity.Parse(File.ReadAllText(Path.Combine(root, "mod.json")));

    private static ModIdentity Load()
    {
        using var stream = typeof(ModInfo).Assembly.GetManifestResourceStream("ModIdentity")
            ?? throw new InvalidDataException("Missing embedded mod.json");
        using var reader = new StreamReader(stream);
        return ModIdentity.Parse(reader.ReadToEnd());
    }
}

public static class ReleaseVersion
{
    public static string Parse(string value, string field)
    {
        var parts = value.Split('.');
        // SKSE stores versions as 8/8/12/4-bit components; reject silent truncation.
        int[] limits = [255, 255, 4095, 15];
        if (parts.Length is not (3 or 4) || parts.Where((part, index) =>
            part.Length == 0 || part.Any(c => !char.IsAsciiDigit(c)) ||
            (part.Length > 1 && part[0] == '0') || !int.TryParse(part, out var number) || number > limits[index]).Any())
        {
            throw new InvalidDataException($"{field}: expected three or four numeric components within SKSE version limits (255.255.4095.15)");
        }
        return value;
    }
}
