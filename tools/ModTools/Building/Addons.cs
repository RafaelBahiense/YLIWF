using System.Text.Json;
using System.Text.RegularExpressions;

namespace ModTools.Building;

// A native add-on declares its DLL/package suffix once; CMake reads the same file.
public sealed partial record Addon(string Name, string Version, uint AdapterApiVersion)
{
    public string DllFile => $"{ModInfo.BinaryName}.{Name}.dll";
    public string ArchiveName => $"{ModInfo.BinaryName}-{Name}-{Version}.zip";
    public string SourceArchiveName => $"{ModInfo.BinaryName}-{Name}-{Version}-source.zip";

    public static Addon[] Read(string root)
    {
        var directory = Path.Combine(root, "src/addons");
        if (!Directory.Exists(directory))
        {
            return [];
        }

        var addons = Directory.GetFiles(directory, "addon.json", SearchOption.AllDirectories)
            .Order(StringComparer.Ordinal).Select(path =>
            {
                using var document = JsonDocument.Parse(File.ReadAllText(path));
                var declaration = document.RootElement;
                var name = declaration.TryGetProperty("name", out var nameField) && nameField.ValueKind == JsonValueKind.String
                    ? nameField.GetString() : null;
                if (name == null || !NamePattern().IsMatch(name))
                {
                    throw new InvalidDataException($"Invalid add-on name: {path}");
                }

                if (!declaration.TryGetProperty("version", out var versionField) || versionField.ValueKind != JsonValueKind.String)
                {
                    throw new InvalidDataException($"Missing add-on version: {path}");
                }
                var version = ReleaseVersion.Parse(versionField.GetString()!, $"{path} version");
                if (!declaration.TryGetProperty("adapterApiVersion", out var apiField) || apiField.ValueKind != JsonValueKind.Number ||
                    !apiField.TryGetUInt32(out var apiVersion) || apiVersion == 0)
                {
                    throw new InvalidDataException($"Invalid adapter API version: {path}");
                }
                return new Addon(name, version, apiVersion);
            }).ToArray();
        if (addons.Select(addon => addon.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count() != addons.Length)
        {
            throw new InvalidDataException("Duplicate add-on names");
        }

        return addons;
    }

    [GeneratedRegex(@"^[A-Za-z][A-Za-z0-9_-]*$", RegexOptions.CultureInvariant)]
    private static partial Regex NamePattern();
}
