using System.Text.Json;

// Shared by plugin authoring and build tools; embedded so commands also work outside the repo.
public sealed record ModIdentity(string DisplayName, string ShortName, string PluginFile, string BinaryName, string ScriptPrefix, string Author)
{
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
        var identity = new ModIdentity(Required("displayName"), Required("shortName"), Required("pluginFile"), Required("binaryName"), Required("scriptPrefix"), Required("author"));
        foreach (var value in new[] { identity.ShortName, identity.BinaryName, identity.ScriptPrefix })
        {
            if (!(char.IsAsciiLetter(value[0]) || value[0] == '_') || value.Any(c => !(char.IsAsciiLetterOrDigit(c) || c == '_' || c == '-')))
            {
                throw new InvalidDataException("mod.json: shortName, binaryName and scriptPrefix must be identifiers");
            }
        }
        if (identity.ScriptPrefix.Contains('-') || identity.PluginFile.IndexOfAny("<>:\"/\\|?*;".ToCharArray()) >= 0 ||
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

    private static ModIdentity Load()
    {
        using var stream = typeof(ModInfo).Assembly.GetManifestResourceStream("ModIdentity")
            ?? throw new InvalidDataException("Missing embedded mod.json");
        using var reader = new StreamReader(stream);
        return ModIdentity.Parse(reader.ReadToEnd());
    }
}
