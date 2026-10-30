using ModTools.Packaging;
using Mutagen.Bethesda.Skyrim;

namespace ModTools.Building;

public interface IAddonPluginGenerator
{
    string AddonName
    {
        get;
    }
    SkyrimMod Create();
}

public static class AddonPlugins
{
    public static string OutputPath(string directory, Addon addon) =>
        Path.Combine(directory, "addons", addon.Name, addon.PluginFile!);

    public static void Generate(string root, string directory)
    {
        var generators = typeof(AddonPlugins).Assembly.GetTypes()
            .Where(type => !type.IsAbstract && type.IsAssignableTo(typeof(IAddonPluginGenerator)))
            .Select(type => (IAddonPluginGenerator)System.Activator.CreateInstance(type)!)
            .ToDictionary(generator => generator.AddonName, StringComparer.Ordinal);
        var inputs = BuildReceipt.CaptureInputs(root, BuildComponent.Plugin);
        foreach (var addon in Addon.Read(root).Where(addon => addon.PluginFile != null))
        {
            if (!generators.TryGetValue(addon.Name, out var generator))
            {
                throw new InvalidDataException($"Missing plugin generator for {addon.Name}");
            }
            var mod = generator.Create();
            if (!string.Equals(mod.ModKey.FileName.String, addon.PluginFile, StringComparison.Ordinal))
            {
                throw new InvalidDataException($"Plugin generator filename does not match {addon.Name}");
            }
            var output = OutputPath(directory, addon);
            Directory.CreateDirectory(Path.GetDirectoryName(output)!);
            mod.BeginWrite.ToPath(output).WithLoadOrderFromHeaderMasters()
                .WithNoDataFolder().NoMastersListContentCheck().Write();
            BuildReceipt.Write(root, BuildComponent.Plugin, inputs, [output], BuildReceipt.PluginPath(output));
            Console.WriteLine($"Generated add-on plugin: {output}");
        }
    }
}
