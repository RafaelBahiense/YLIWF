using ModTools;
using ModTools.Packaging;
using Mutagen.Bethesda.Skyrim;

try
{
    if (args.Length > 0 && args[0] is Commands.Prepare or Commands.Scripts or Commands.Package)
    {
        return Commands.Run(args);
    }

    if (args.Length != 2 || args[0] is not (Commands.Inspect or Commands.Generate))
    {
        Console.Error.WriteLine("Usage: ModTools generate <directory> | inspect <esp> | prepare/scripts/package --option value...");
        return 2;
    }

    var path = Path.GetFullPath(args[1]);
    if (args[0] == Commands.Generate)
    {
        var inputs = BuildReceipt.CaptureInputs(Directory.GetCurrentDirectory(), BuildComponent.Plugin);
        var output = Path.Combine(path, ModInfo.PluginFile);
        Directory.CreateDirectory(path);
        var mod = FollowerPlugin.Create();
        mod.BeginWrite.ToPath(output)
            .WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder()
            .NoMastersListContentCheck()
            .Write();
        BuildReceipt.Write(Directory.GetCurrentDirectory(), BuildComponent.Plugin, inputs, [output], BuildReceipt.PluginPath(output));
        Console.WriteLine($"Generated {mod.EnumerateMajorRecords().Count()} records: {output}");
        return 0;
    }
    using var loaded = SkyrimMod.CreateFromBinaryOverlay(path, SkyrimRelease.SkyrimSE);
    Console.WriteLine($"Plugin: {loaded.ModKey}");
    foreach (var record in loaded.EnumerateMajorRecords())
    {
        Console.WriteLine($"{record.FormKey}\t{record.GetType().Name}\t{record.EditorID}");
    }

    return 0;
}
catch (Exception error)
{
    Console.Error.WriteLine($"ModTools failed: {error.Message}");
    return 1;
}
