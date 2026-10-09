using System.Text.Json;

namespace ModTools.Building;

public enum BuildTarget
{
    All, Native, Plugin, Scripts, Package, Test, Verify, Repackage
}

public sealed record BuildOptions
{
    public required string Root { get; init; }
    public BuildTarget Target { get; init; }
    public BuildConfiguration Configuration { get; init; } = BuildConfiguration.Release;
    public bool Clean { get; init; }
    public bool Include3Dnpc { get; init; }
    public required string CommonLib { get; init; }
    public required string Vcpkg { get; init; }
    public required string Compiler { get; init; }
    public required string Flags { get; init; }
    public required string[] Imports { get; init; }
    public required string Output { get; init; }
    public string? Dll { get; init; }
    public string? SourceArchive { get; init; }
    public string? DeployTo { get; init; }

    public static BuildOptions Read(IReadOnlyDictionary<string, string> options, IReadOnlySet<string> switches)
    {
        string[] optionNames = ["root", "target", "configuration", "commonlib", "vcpkg", "compiler",
            "flags", "imports", "output", "dll", "source-archive", "deploy-to"];
        var allowed = optionNames.Select(name => "--" + name).ToHashSet(StringComparer.Ordinal);
        foreach (var key in options.Keys)
        {
            if (!allowed.Contains(key))
            {
                throw new ArgumentException($"Unknown build option: {key}");
            }
        }

        foreach (var flag in switches)
        {
            if (flag is not (Commands.Clean or Commands.Include3Dnpc))
            {
                throw new ArgumentException($"Unknown build switch: {flag}");
            }
        }

        var root = Path.GetFullPath(options.GetValueOrDefault("--root") ?? Directory.GetCurrentDirectory());
        var local = Path.Combine(root, ProjectPaths.LocalPaths);
        using var defaults = JsonDocument.Parse(File.ReadAllText(Path.Combine(root, ProjectPaths.PathDefaults)));
        using var document = File.Exists(local) ? JsonDocument.Parse(File.ReadAllText(local)) : null;
        string Resolve(string option, string property)
        {
            var value = options.GetValueOrDefault("--" + option);
            if (value == null && document != null && document.RootElement.TryGetProperty(property, out var configured))
            {
                value = configured.GetString();
            }

            return Path.GetFullPath(value ?? defaults.RootElement.GetProperty(property).GetString()!, root);
        }
        var targetText = options.GetValueOrDefault("--target") ?? "All";
        if (!Enum.TryParse<BuildTarget>(targetText, true, out var target) || !Enum.IsDefined(target))
        {
            throw new ArgumentException($"Unknown build target: {targetText}");
        }

        var configuration = BuildConfigurations.Parse(options.GetValueOrDefault("--configuration") ??
            BuildConfiguration.Release.CMakeName());

        var imports = defaults.RootElement.GetProperty("Imports").EnumerateArray().Select(item => item.GetString()!).ToArray();
        if (options.TryGetValue("--imports", out var list))
        {
            imports = list.Split(';', StringSplitOptions.RemoveEmptyEntries);
        }
        else if (document != null && document.RootElement.TryGetProperty("Imports", out var configuredImports))
        {
            imports = configuredImports.EnumerateArray().Select(item => item.GetString()!).ToArray();
        }

        string? OptionalPath(string name) => options.TryGetValue("--" + name, out var path) ? Path.GetFullPath(path, root) : null;
        var result = new BuildOptions
        {
            Root = root,
            Target = target,
            Configuration = configuration,
            Clean = switches.Contains(Commands.Clean),
            Include3Dnpc = switches.Contains(Commands.Include3Dnpc),
            CommonLib = Resolve("commonlib", "CommonLib"),
            Vcpkg = Resolve("vcpkg", "Vcpkg"),
            Compiler = Resolve("compiler", "Compiler"),
            Flags = Resolve("flags", "Flags"),
            Imports = imports.Select(path => Path.GetFullPath(path, root)).ToArray(),
            Output = Resolve("output", "Output"),
            Dll = OptionalPath("dll"),
            SourceArchive = OptionalPath("source-archive"),
            DeployTo = OptionalPath("deploy-to")
        };
        if (target == BuildTarget.Repackage)
        {
            if (result.Dll == null || result.SourceArchive == null)
            {
                throw new ArgumentException("Repackage requires --dll and its matching --source-archive");
            }
        }
        else if (result.Dll != null || result.SourceArchive != null)
        {
            throw new ArgumentException("Use target Repackage for a supplied DLL and source archive");
        }

        if (result.DeployTo != null && target is not (BuildTarget.All or BuildTarget.Package or BuildTarget.Repackage))
        {
            throw new ArgumentException("Deployment requires All, Package or Repackage");
        }

        if (result.Clean && target is not (BuildTarget.All or BuildTarget.Native or BuildTarget.Test or BuildTarget.Verify))
        {
            throw new ArgumentException("Clean requires a target that compiles native code");
        }

        return result;
    }
}
