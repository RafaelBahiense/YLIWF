using System.Text.Json;

namespace ModTools.Packaging;

public enum BuildComponent
{
    Plugin = 0, PapyrusCore = 1, Papyrus3Dnpc = 2, Native = 3
}

// Local sidecars bind reusable outputs to the sources that produced them.
// They are checked before packaging and are not installed in Data.
public static class BuildReceipt
{
    public const string ScriptsFile = "build-receipt.json";
    private const int SchemaVersion = 1;
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };
    private sealed record Receipt(int Schema, BuildComponent Component, Dictionary<string, string> Inputs, Dictionary<string, string> Outputs);

    public static string PluginPath(string esp) => esp + ".build.json";

    public static Dictionary<string, string> CaptureInputs(string root, BuildComponent component)
    {
        ValidateToolBuild(root);
        return SourceSnapshot.ProjectFiles(root)
            .Where(file => IsComponentInput(file.Key, component) || IsSharedInput(file.Key))
            .ToDictionary(file => file.Key, file => Artifacts.HashFile(file.Value), StringComparer.Ordinal);
    }

    private static bool IsComponentInput(string name, BuildComponent component)
    {
        var sourceDirectory = component switch
        {
            BuildComponent.Plugin => "src/plugin/",
            BuildComponent.Native => "src/native/",
            _ => "src/papyrus/core/"
        };
        return name.StartsWith(sourceDirectory, StringComparison.Ordinal) || component switch
        {
            BuildComponent.Native => IsNativeInput(name),
            BuildComponent.Papyrus3Dnpc => name.StartsWith("src/papyrus/patches/3dnpc/", StringComparison.Ordinal),
            _ => false
        };
    }

    private static bool IsNativeInput(string name) =>
        name.StartsWith("include/", StringComparison.Ordinal) ||
        name.StartsWith("src/addons/", StringComparison.Ordinal) ||
        name.StartsWith("cmake/", StringComparison.Ordinal) ||
        name is "CMakeLists.txt" or "CMakePresets.json" or "vcpkg.json" or "vcpkg-configuration.json" or "assets/settings.ini";

    private static bool IsSharedInput(string name) =>
        name.StartsWith("src/shared/", StringComparison.Ordinal) ||
        (name.StartsWith("tools/ModTools/", StringComparison.Ordinal) && name.EndsWith(".cs", StringComparison.Ordinal)) ||
        name is ProjectPaths.Identity or "global.json" or "tools/dependencies.json" or ProjectPaths.PathDefaults or
            ProjectPaths.ToolProject or ProjectPaths.ToolLock;

    private static bool IsGeneratorInput(string name) =>
        (name.EndsWith(".cs", StringComparison.Ordinal) && (name.StartsWith("tools/ModTools/", StringComparison.Ordinal) ||
            name.StartsWith("src/plugin/", StringComparison.Ordinal) || name == "src/shared/ModInfo.cs")) ||
        name is ProjectPaths.Identity or ProjectPaths.ToolProject or ProjectPaths.ToolLock;

    private static void ValidateToolBuild(string root)
    {
        using var stream = typeof(BuildReceipt).Assembly.GetManifestResourceStream("ToolBuildInputs") ??
            throw new InvalidDataException("Missing generator build inputs; rebuild ModTools");
        using var reader = new StreamReader(stream);
        var built = new Dictionary<string, string>(StringComparer.Ordinal);
        while (reader.ReadLine() is { } line)
        {
            var separator = line.LastIndexOf('|');
            if (separator <= 0)
            {
                throw new InvalidDataException("Invalid generator build inputs; rebuild ModTools");
            }

            var path = Path.GetFullPath(Path.Combine(root, "tools/ModTools", line[..separator]));
            var name = Path.GetRelativePath(root, path).Replace('\\', '/');
            built.Add(name, line[(separator + 1)..].ToLowerInvariant());
        }
        var current = SourceSnapshot.ProjectFiles(root).Where(file => IsGeneratorInput(file.Key))
            .ToDictionary(file => file.Key, file => Artifacts.HashFile(file.Value), StringComparer.Ordinal);
        if (!SameHashes(built, current))
        {
            throw new InvalidDataException("ModTools does not match this checkout; rebuild it before generating, compiling, or packaging");
        }
    }

    private static bool SameHashes(Dictionary<string, string> left, Dictionary<string, string> right) =>
        left.Count == right.Count && left.All(pair => right.TryGetValue(pair.Key, out var hash) && pair.Value == hash);

    public static void Write(string root, BuildComponent component, Dictionary<string, string> inputs, IEnumerable<string> outputs, string path)
    {
        if (!SameHashes(inputs, CaptureInputs(root, component)))
        {
            throw new InvalidDataException($"{component} sources changed during the build; rebuild before publishing");
        }
        var hashes = outputs.ToDictionary(file => Path.GetFileName(file), Artifacts.HashFile, StringComparer.Ordinal);
        File.WriteAllText(path, JsonSerializer.Serialize(new Receipt(SchemaVersion, component, inputs, hashes), JsonOptions));
    }

    public static void Validate(string root, BuildComponent component, IEnumerable<string> outputs, string path)
    {
        if (!File.Exists(path))
        {
            throw new InvalidDataException($"Missing {component} build receipt: {path}. Rebuild this component.");
        }
        Receipt receipt;
        try
        {
            receipt = JsonSerializer.Deserialize<Receipt>(File.ReadAllText(path)) ?? throw new JsonException("Empty receipt");
        }
        catch (JsonException error)
        {
            throw new InvalidDataException($"Invalid build receipt: {path}", error);
        }
        if (receipt.Schema != SchemaVersion || receipt.Component != component || receipt.Inputs == null || receipt.Outputs == null ||
            !SameHashes(receipt.Inputs, CaptureInputs(root, component)))
        {
            throw new InvalidDataException($"{component} outputs do not match this checkout; rebuild this component");
        }
        var hashes = outputs.ToDictionary(file => Path.GetFileName(file), Artifacts.HashFile, StringComparer.Ordinal);
        if (!SameHashes(receipt.Outputs, hashes))
        {
            throw new InvalidDataException($"{component} outputs changed since compilation; rebuild this component");
        }
    }
}
