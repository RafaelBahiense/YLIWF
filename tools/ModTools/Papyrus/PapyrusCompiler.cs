using System.Text;
using System.Text.RegularExpressions;
using ModTools.Building;
using ModTools.Packaging;

namespace ModTools.Papyrus;

public static partial class PapyrusCompiler
{
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
    public static void Compile(string root, string compiler, string flags, string[] imports, string output, bool patch,
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

        if (!OperatingSystem.IsWindows() && runner == null)
        {
            throw new PlatformNotSupportedException("Papyrus compilation requires Windows.");
        }

        runner ??= ProcessRunner.Run;
        foreach (var script in new[] { ModInfo.NativeScript })
        {
            if (!File.Exists(Path.Combine(root, ProjectPaths.CoreScripts, script + ".psc")))
            {
                throw new InvalidDataException($"mod.json scriptPrefix does not match the existing {script} source. Script identities must remain stable for existing saves.");
            }
        }
        var coreInputs = BuildReceipt.CaptureInputs(root, BuildComponent.PapyrusCore);
        var patchInputs = patch ? BuildReceipt.CaptureInputs(root, BuildComponent.Papyrus3Dnpc) : null;
        var stage = Path.Combine(Path.GetFullPath(output), ".staging-" + Guid.NewGuid().ToString("N"));
        var core = Path.Combine(stage, "Source/Scripts");
        StageCore(Path.Combine(root, ProjectPaths.CoreScripts), core);
        var groups = new List<(string Source, string Staging, string Destination, string[] Imports)> {
            (core, Path.Combine(stage, ProjectPaths.ScriptsOutput), Path.Combine(output, ProjectPaths.ScriptsOutput), new[] { core }.Concat(imports).ToArray())
        };
        if (patch)
        {
            var source = Path.Combine(root, ProjectPaths.Patch3DnpcScripts);
            groups.Add((source, Path.Combine(stage, ProjectPaths.Patch3DnpcOutput), Path.Combine(output, ProjectPaths.Patch3DnpcOutput), new[] { source, core }.Concat(imports).ToArray()));
        }
        var completed = new List<(string[] Outputs, string Destination)>();
        foreach (var (source, staging, destination, groupImports) in groups)
        {
            Directory.CreateDirectory(staging);
            var arguments = new List<string> {
                Path.GetFullPath(source), "--game=skyrim", "--ignorecwd", "--enable-language-extensions=false",
                "--import=" + string.Join(';', groupImports.Select(path => Path.GetFullPath(path))),
                "--flags=" + Path.GetFullPath(flags), "--output=" + Path.GetFullPath(staging)
            };
            runner(compiler, arguments, root);
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
