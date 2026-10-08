using System.IO.Compression;
using ModTools.Packaging;
using ModTools.Papyrus;

namespace ModTools.Building;

public static class BuildCoordinator
{
    public static void Run(BuildOptions options)
    {
        var root = options.Root;
        var preset = options.Configuration.ToLowerInvariant();
        var native = Path.Combine(root, "build/native", preset);
        var plugin = Path.Combine(root, "build/plugin");
        var papyrus = Path.Combine(root, "build/papyrus");
        void Tool(string executable, params string[] arguments) => ProcessRunner.Run(executable, arguments, root);
        var buildNative = options.Target is BuildTarget.All or BuildTarget.Native or BuildTarget.Test or BuildTarget.Verify;
        var test = options.Target is BuildTarget.Test or BuildTarget.Verify;
        string[] NativeOutputs() => new[] { Path.Combine(native, ModInfo.Identity.DllFile) }
            .Concat(Addon.Read(root).Select(addon => Path.Combine(native, addon.DllFile))).ToArray();
        if (buildNative)
        {
            var inputs = BuildReceipt.CaptureInputs(root, BuildComponent.Native);
            Environment.SetEnvironmentVariable("VCPKG_ROOT", options.Vcpkg);
            // A clean build preserves the existing working build and binary caches.
            if (options.Clean)
            {
                native += "-clean-" + Guid.NewGuid().ToString("N");
            }

            Tool("cmake", "--preset", preset, "-B", native, "-DCOMMONLIB_SSE_FOLDER=" + options.CommonLib);
            var targets = test ? new[] { "native_tests" } : new[] { "native_plugins" };
            if (options.Target == BuildTarget.Verify)
            {
                targets = ["native_plugins", "native_tests"];
            }

            Tool("cmake", ["--build", native, "--target", .. targets, "--parallel"]);
            if (options.Target != BuildTarget.Test)
            {
                BuildReceipt.Write(root, BuildComponent.Native, inputs, NativeOutputs(), Path.Combine(native, BuildReceipt.ScriptsFile));
            }
        }
        if (test)
        {
            Tool("ctest", "--test-dir", native, "--output-on-failure");
            Tool("dotnet", "restore", "tests/tools/ModTools.Tests.csproj", "--locked-mode", "--configfile", "tools/NuGet.Config");
            // ModTools is already built by the launcher; avoid rebuilding the running executable.
            Tool("dotnet", "build", "tests/tools/ModTools.Tests.csproj", "-c", "Release", "--no-restore", "-p:BuildProjectReferences=false");
            Tool("dotnet", Path.Combine(root, "tests/tools/bin/Release/net10.0/ModTools.Tests.dll"), root);
            Console.WriteLine("All tests passed.");
        }
        if (options.Target is BuildTarget.All or BuildTarget.Plugin or BuildTarget.Verify)
        {
            PluginBuilder.Generate(root, plugin);
        }

        if (options.Target is BuildTarget.All or BuildTarget.Scripts or BuildTarget.Repackage)
        {
            PapyrusCompiler.Compile(root, options.Compiler, options.Flags, options.Imports, papyrus, options.Include3Dnpc);
        }

        if (options.Target == BuildTarget.Repackage)
        {
            PluginBuilder.Generate(root, plugin);
        }

        if (options.Target is BuildTarget.All or BuildTarget.Package or BuildTarget.Repackage)
        {
            if (options.Target != BuildTarget.Repackage)
            {
                BuildReceipt.Validate(root, BuildComponent.Native, NativeOutputs(), Path.Combine(native, BuildReceipt.ScriptsFile));
            }
            Artifacts.Package(root, Path.Combine(plugin, ModInfo.PluginFile), options.Dll ?? Path.Combine(native, ModInfo.Identity.DllFile),
                papyrus, options.Output, options.Include3Dnpc,
                options.Target == BuildTarget.Repackage ? BuildMode.SuppliedDll : BuildMode.FullSource,
                options.SourceArchive, options.CommonLib, options.Vcpkg, native);
            if (options.DeployTo != null)
            {
                Deploy(root, options.Output, options.DeployTo);
            }
        }
    }

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
