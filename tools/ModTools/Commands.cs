using ModTools.Building;
using ModTools.Packaging;
using ModTools.Papyrus;

namespace ModTools;

public static class Commands
{
    public const string Build = "build";
    public const string Prepare = "prepare";
    public const string Scripts = "scripts";
    public const string Package = "package";
    public const string Generate = "generate";
    public const string Inspect = "inspect";
    internal const string Include3Dnpc = "--include-3dnpc";
    internal const string Clean = "--clean";

    public static int Run(string[] args)
    {
        var command = args[0];
        var options = new Dictionary<string, string>(StringComparer.Ordinal);
        var flags = new HashSet<string>();
        for (var i = 1; i < args.Length; ++i)
        {
            if (args[i] == Include3Dnpc || args[i] == Clean)
            {
                flags.Add(args[i]);
                continue;
            }
            if (!args[i].StartsWith("--", StringComparison.Ordinal) || i + 1 == args.Length)
            {
                throw new ArgumentException($"Expected --option value: {args[i]}");
            }

            var key = args[i++];
            options.Add(key, args[i]);
        }
        string RequiredOption(string name) => options.TryGetValue("--" + name, out var value) ? value : throw new ArgumentException($"Missing --{name}");
        switch (command)
        {
            case Build:
                BuildCoordinator.Run(BuildOptions.Read(options, flags));
                break;
            case Prepare:
                SourcePreparation.Prepare(RequiredOption("archive"), RequiredOption("vanilla"), RequiredOption("output"), RequiredOption("revision"));
                break;
            case Scripts:
                PapyrusCompiler.Compile(RequiredOption("root"), RequiredOption("compiler"), RequiredOption("flags"), RequiredOption("imports").Split(';', StringSplitOptions.RemoveEmptyEntries), RequiredOption("output"), flags.Contains(Include3Dnpc));
                break;
            case Package:
                Artifacts.Package(RequiredOption("root"), RequiredOption("esp"), RequiredOption("dll"), RequiredOption("papyrus"), RequiredOption("output"), flags.Contains(Include3Dnpc), BuildModes.Parse(options.GetValueOrDefault("--mode")), options.GetValueOrDefault("--source-archive"), options.GetValueOrDefault("--commonlib"), options.GetValueOrDefault("--vcpkg"), options.GetValueOrDefault("--native-build"));
                break;
            default:
                throw new ArgumentException($"Unknown command: {command}");
        }
        return 0;
    }
}
