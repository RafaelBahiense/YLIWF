using System.Text.Json;
using System.Text.RegularExpressions;

namespace ModTools.Building;

// A native add-on declares its DLL/package suffix once; CMake reads the same file.
public sealed partial record Addon(string Name)
{
    public string DllFile => $"{ModInfo.BinaryName}.{Name}.dll";

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
                var name = document.RootElement.GetProperty("name").GetString();
                if (name == null || !NamePattern().IsMatch(name))
                {
                    throw new InvalidDataException($"Invalid add-on name: {path}");
                }

                return new Addon(name);
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
