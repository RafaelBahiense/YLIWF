using ModTools.Building;

internal static class BuildTests
{
    internal static void Run(string root, string temporary)
    {
        void Check(bool value, string message)
        {
            if (!value)
            {
                throw new InvalidOperationException(message);
            }
        }
        void Reject(Action action)
        {
            try { action(); }
            catch (ArgumentException) { return; }
            catch (InvalidDataException) { return; }
            throw new InvalidOperationException("Expected invalid build declaration to fail");
        }
        var directory = Path.Combine(temporary, "build-options");
        Directory.CreateDirectory(Path.Combine(directory, ".tools"));
        Directory.CreateDirectory(Path.Combine(directory, "tools"));
        File.Copy(Path.Combine(root, "tools/paths.json"), Path.Combine(directory, "tools/paths.json"));
        File.WriteAllText(Path.Combine(directory, ".tools/local.json"),
            """{"CommonLib":"custom commonlib", "Imports":["custom imports"]}""");
        var arguments = new Dictionary<string, string> { ["--root"] = directory };
        var options = BuildOptions.Read(arguments, new HashSet<string>());
        Check(options.Target == BuildTarget.All && options.CommonLib == Path.Combine(directory, "custom commonlib") &&
            options.Imports.SequenceEqual([Path.Combine(directory, "custom imports")]), "Local paths were not resolved relative to the build root");
        arguments["--commonlib"] = "override";
        Check(BuildOptions.Read(arguments, new HashSet<string>()).CommonLib == Path.Combine(directory, "override"), "Explicit option did not override local paths");
        arguments["--dll"] = "external.dll";
        Reject(() => BuildOptions.Read(arguments, new HashSet<string>()));
        arguments["--target"] = "Repackage";
        Reject(() => BuildOptions.Read(arguments, new HashSet<string>()));
        arguments["--source-archive"] = "matching.zip";
        Check(BuildOptions.Read(arguments, new HashSet<string>()).Target == BuildTarget.Repackage, "Valid repackaging request rejected");
        Reject(() => BuildOptions.Read(arguments, new HashSet<string> { "--clean" }));
        arguments["--typo"] = "value";
        Reject(() => BuildOptions.Read(arguments, new HashSet<string>()));
        var addonDirectory = Path.Combine(directory, "src/addons");
        foreach (var name in new[] { "First", "Second" })
        {
            Directory.CreateDirectory(Path.Combine(addonDirectory, name));
            File.WriteAllText(Path.Combine(addonDirectory, name, "addon.json"), $$"""{"name":"{{name}}", "sources":["Adapter.cpp"]}""");
        }
        Check(Addon.Read(directory).Select(addon => addon.DllFile).SequenceEqual([
            ModInfo.BinaryName + ".First.dll", ModInfo.BinaryName + ".Second.dll"]), "New add-ons need special-case tooling");
        var second = Path.Combine(addonDirectory, "Second/addon.json");
        File.WriteAllText(second, """{"name":"first"}""");
        Reject(() => Addon.Read(directory));
        File.WriteAllText(second, """{"name":"../escape"}""");
        Reject(() => Addon.Read(directory));
    }
}
