using System.IO.Compression;
using ModTools.Packaging;
using ModTools.Papyrus;

namespace ModTools.Building;

public static class BuildCoordinator
{
    public static void Run(BuildOptions options)
    {
        var native = Path.Combine(options.Root, "build/native", options.Configuration.ToLowerInvariant());
        var plugin = Path.Combine(options.Root, "build/plugin");
        var papyrus = Path.Combine(options.Root, "build/papyrus");

        if (options.Target is BuildTarget.All or BuildTarget.Native or BuildTarget.Test or BuildTarget.Verify)
        {
            native = BuildNative(options, native);
        }
        if (options.Target is BuildTarget.Test or BuildTarget.Verify)
        {
            RunTests(options.Root, native);
        }
        if (options.Target is BuildTarget.All or BuildTarget.Plugin or BuildTarget.Verify)
        {
            PluginBuilder.Generate(options.Root, plugin);
        }
        if (options.Target is BuildTarget.All or BuildTarget.Scripts or BuildTarget.Repackage)
        {
            PapyrusCompiler.Compile(options.Root, options.Compiler, options.Flags, options.Imports, papyrus, options.Include3Dnpc);
        }
        if (options.Target == BuildTarget.Repackage)
        {
            PluginBuilder.Generate(options.Root, plugin);
        }
        if (options.Target is BuildTarget.All or BuildTarget.Package or BuildTarget.Repackage)
        {
            Package(options, native, plugin, papyrus);
        }
    }

    private static string BuildNative(BuildOptions options, string native)
    {
        var inputs = BuildReceipt.CaptureInputs(options.Root, BuildComponent.Native);
        Environment.SetEnvironmentVariable("VCPKG_ROOT", options.Vcpkg);
        // Preserve the working directory and binary caches for clean builds.
        if (options.Clean)
        {
            native += "-clean-" + Guid.NewGuid().ToString("N");
        }

        RunTool(options.Root, "cmake", "--preset", options.Configuration.ToLowerInvariant(),
            "-B", native, "-DCOMMONLIB_SSE_FOLDER=" + options.CommonLib);
        string[] targets = options.Target switch
        {
            BuildTarget.Test => ["native_tests"],
            BuildTarget.Verify => ["native_plugins", "native_tests"],
            _ => ["native_plugins"]
        };
        RunTool(options.Root, "cmake", ["--build", native, "--target", .. targets, "--parallel"]);
        if (options.Target != BuildTarget.Test)
        {
            BuildReceipt.Write(options.Root, BuildComponent.Native, inputs,
                NativeOutputs(options.Root, native), Path.Combine(native, BuildReceipt.ScriptsFile));
        }
        return native;
    }

    private static void RunTests(string root, string native)
    {
        RunTool(root, "ctest", "--test-dir", native, "--output-on-failure");
        RunTool(root, "dotnet", "restore", "tests/tools/ModTools.Tests.csproj",
            "--locked-mode", "--configfile", "tools/NuGet.Config");
        // The launcher already built ModTools; do not rebuild the running executable.
        RunTool(root, "dotnet", "build", "tests/tools/ModTools.Tests.csproj", "-c", "Release",
            "--no-restore", "-p:BuildProjectReferences=false");
        RunTool(root, "dotnet", Path.Combine(root, "tests/tools/bin/Release/net10.0/ModTools.Tests.dll"), root);
        Console.WriteLine("All tests passed.");
    }

    private static string[] NativeOutputs(string root, string native) =>
        new[] { Path.Combine(native, ModInfo.Identity.DllFile) }
            .Concat(Addon.Read(root).Select(addon => Path.Combine(native, addon.DllFile))).ToArray();

    private static void Package(BuildOptions options, string native, string plugin, string papyrus)
    {
        if (options.Target != BuildTarget.Repackage)
        {
            BuildReceipt.Validate(options.Root, BuildComponent.Native,
                NativeOutputs(options.Root, native), Path.Combine(native, BuildReceipt.ScriptsFile));
        }
        Artifacts.Package(options.Root, Path.Combine(plugin, ModInfo.PluginFile),
            options.Dll ?? Path.Combine(native, ModInfo.Identity.DllFile), papyrus, options.Output,
            options.Include3Dnpc, options.Target == BuildTarget.Repackage ? BuildMode.SuppliedDll : BuildMode.FullSource,
            options.SourceArchive, options.CommonLib, options.Vcpkg, native);
        if (options.DeployTo != null)
        {
            Deploy(options.Root, options.Output, options.DeployTo);
        }
    }

    private static void RunTool(string root, string executable, params string[] arguments) =>
        ProcessRunner.Run(executable, arguments, root);

    private static void Deploy(string root, string output, string destination)
    {
        var version = File.ReadAllText(Path.Combine(root, "VERSION")).Trim();
        using var archive = ZipFile.OpenRead(Path.Combine(output, $"{ModInfo.BinaryName}-{version}.zip"));
        foreach (var entry in archive.Entries)
        {
            Artifacts.ValidateName(entry.FullName);
            var path = Path.Combine(destination, entry.FullName);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            entry.ExtractToFile(path, true);
        }
        Console.WriteLine($"Deployed core mod: {destination}");
    }
}
