using ModTools.Packaging;

var root = Path.GetFullPath(args.Single());
var timestamp = ReleaseTimestamp.Read(root);
var temporary = Path.Combine(Path.GetTempPath(), "yliwf-tools-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(temporary);
try
{
    BuildTests.Run(root, temporary);
    MetadataTests.Run(root, temporary);
    ProtocolTests.Run(root);
    var dll = PluginTests.CreateDll(temporary);
    var esp = PluginTests.Run(root, temporary);
    var papyrus = PapyrusTests.PrepareScripts(root, temporary);
    new PackagingTests(root, temporary, timestamp, dll, esp, papyrus).Run();
    PapyrusTests.Run(root, temporary);
    Console.WriteLine("Tooling tests passed: Papyrus/native contracts, generated records, PE/PEX/ESP rejection, ZIP layout and hashes, matching source and notices, release preservation, optional patch, rollback, source staging, compiler failure.");
    return 0;
}
catch (Exception error)
{
    Console.Error.WriteLine(error);
    return 1;
}
finally
{
    Directory.Delete(temporary, true);
}
