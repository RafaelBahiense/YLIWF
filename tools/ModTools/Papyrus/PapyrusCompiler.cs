using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using ModTools.Packaging;

namespace ModTools.Papyrus;

public static partial class PapyrusCompiler
{
    public static void Run(string executable, IEnumerable<string> arguments, string root)
    {
        var startInfo = new ProcessStartInfo(executable) { WorkingDirectory = root, UseShellExecute = false };
        foreach (var argument in arguments)
        {
            startInfo.ArgumentList.Add(argument);
        }

        Console.WriteLine($"Running: {executable} {string.Join(' ', startInfo.ArgumentList)}");
        using var process = Process.Start(startInfo) ?? throw new IOException($"Could not start {executable}");
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new IOException($"{executable} failed (exit {process.ExitCode}). Staged outputs were not published.");
        }
    }
    public static void StageCore(string source, string destination)
    {
        Directory.CreateDirectory(destination);
        foreach (var file in Directory.GetFiles(source, "*.psc"))
        {
            File.Copy(file, Path.Combine(destination, Path.GetFileName(file)));
        }

        var regex = BridgeHeaderRegex();
        foreach (var bridge in Directory.GetFiles(destination, "*.psc")
            .Where(file => Path.GetFileNameWithoutExtension(file) == ModInfo.NativeScript))
        {
            var text = File.ReadAllText(bridge);
            if (regex.Count(text) != 1 || !string.Equals(SourcePreparation.ScriptName(text), Path.GetFileNameWithoutExtension(bridge), StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException($"Unexpected {Path.GetFileNameWithoutExtension(bridge)} header; cannot annotate temporary Caprica input");
            }

            File.WriteAllText(bridge, regex.Replace(text, "$1 Native"), new UTF8Encoding(false));
        }
    }
    public static void Compile(string root, string compiler, string flags, string[] imports, string output, bool patch, string? wine = null,
        Action<string, IEnumerable<string>, string>? runner = null)
    {
        foreach (var path in new[] { compiler, flags }.Concat(imports))
        {
            if (!File.Exists(path) && !Directory.Exists(path))
            {
                throw new FileNotFoundException($"Missing compiler input: {path}");
            }
        }

        if (imports.Length == 0)
        {
            throw new ArgumentException("At least one import directory is required");
        }

        if (!OperatingSystem.IsWindows() && string.IsNullOrWhiteSpace(wine))
        {
            throw new ArgumentException("Supply --wine on Linux to run Caprica");
        }

        runner ??= Run;
        foreach (var script in new[] { ModInfo.NativeScript })
        {
            if (!File.Exists(Path.Combine(root, "src/papyrus/core", script + ".psc")))
            {
                throw new InvalidDataException($"mod.json scriptPrefix does not match the existing {script} source. Script identities must remain stable for existing saves.");
            }
        }
        var coreInputs = BuildReceipt.CaptureInputs(root, BuildComponent.PapyrusCore);
        var patchInputs = patch ? BuildReceipt.CaptureInputs(root, BuildComponent.Papyrus3Dnpc) : null;
        var stage = Path.Combine(Path.GetFullPath(output), ".staging-" + Guid.NewGuid().ToString("N"));
        var core = Path.Combine(stage, "Source/Scripts");
        StageCore(Path.Combine(root, "src/papyrus/core"), core);
        var groups = new List<(string Source, string Staging, string Destination, string[] Imports)> {
            (core, Path.Combine(stage, "Scripts"), Path.Combine(output, "Scripts"), new[] { core }.Concat(imports).ToArray())
        };
        if (patch)
        {
            var source = Path.Combine(root, "src/papyrus/patches/3dnpc");
            groups.Add((source, Path.Combine(stage, "Patches/3DNPC/Scripts"), Path.Combine(output, "Patches/3DNPC/Scripts"), new[] { source, core }.Concat(imports).ToArray()));
        }
        string CompilerPath(string path) => wine == null ? Path.GetFullPath(path) : "Z:" + Path.GetFullPath(path).Replace('/', '\\');
        var completed = new List<(string[] Outputs, string Destination)>();
        foreach (var (source, staging, destination, groupImports) in groups)
        {
            Directory.CreateDirectory(staging);
            var arguments = new List<string> {
                CompilerPath(source), "--game=skyrim", "--ignorecwd", "--enable-language-extensions=false",
                "--import=" + string.Join(';', groupImports.Select(CompilerPath)),
                "--flags=" + CompilerPath(flags), "--output=" + CompilerPath(staging)
            };
            if (wine != null)
            {
                arguments.Insert(0, Path.GetFullPath(compiler));
            }

            runner(wine ?? compiler, arguments, root);
            var outputs = Artifacts.ScriptOutputs(source, staging);
            foreach (var file in outputs)
            {
                PexMetadata.Sanitize(file);
            }
            completed.Add((outputs, destination));
        }
        // Validate both groups before publishing either outputs or receipts.
        for (var i = 0; i < completed.Count; ++i)
        {
            var (outputs, _) = completed[i];
            var component = i == 0 ? BuildComponent.PapyrusCore : BuildComponent.Papyrus3Dnpc;
            var receipt = Path.Combine(Path.GetDirectoryName(outputs[0])!, BuildReceipt.ScriptsFile);
            BuildReceipt.Write(root, component, i == 0 ? coreInputs : patchInputs!, outputs, receipt);
        }
        // A failure in an optional patch must not publish the already compiled core.
        foreach (var (outputs, destination) in completed)
        {
            Directory.CreateDirectory(destination);
            foreach (var file in outputs)
            {
                File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), true);
            }
            File.Copy(Path.Combine(Path.GetDirectoryName(outputs[0])!, BuildReceipt.ScriptsFile), Path.Combine(destination, BuildReceipt.ScriptsFile), true);
        }
        // Only remove the stage created by this successful invocation.
        Directory.Delete(stage, true);
        Console.WriteLine($"Validated {completed.Sum(group => group.Outputs.Length)} Skyrim PEX files: {output}");
    }

    [GeneratedRegex(@"^(Scriptname\s+[A-Za-z_][A-Za-z0-9_]*\s+Hidden)[ \t]*\r?$", RegexOptions.Multiline | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex BridgeHeaderRegex();
}
