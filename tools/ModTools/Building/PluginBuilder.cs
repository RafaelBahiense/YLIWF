using ModTools.Packaging;
using Mutagen.Bethesda.Skyrim;

namespace ModTools.Building;

public static class PluginBuilder
{
    public static void Generate(string root, string directory)
    {
        var inputs = BuildReceipt.CaptureInputs(root, BuildComponent.Plugin);
        var output = Path.Combine(directory, ModInfo.PluginFile);
        Directory.CreateDirectory(directory);
        var mod = FollowerPlugin.Create();
        mod.BeginWrite.ToPath(output)
            .WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder()
            .NoMastersListContentCheck()
            .Write();
        BuildReceipt.Write(root, BuildComponent.Plugin, inputs, [output], BuildReceipt.PluginPath(output));
        Console.WriteLine($"Generated {mod.EnumerateMajorRecords().Count()} records: {output}");
    }
}
