using ModTools.Packaging;

internal static class BuildReceiptTests
{
    public static void Run(string root, string temporary, string esp)
    {
        static void Reject(Action action)
        {
            try
            {
                action();
            }
            catch (InvalidDataException) { return; }
            throw new InvalidOperationException("Mismatched build receipt was accepted");
        }
        // Change a separate checkout so the test never edits maintained sources.
        var checkout = Path.Combine(temporary, "receipt-checkout");
        foreach (var (name, file) in SourceSnapshot.ProjectFiles(root))
        {
            var destination = Path.Combine(checkout, name);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(file, destination);
        }
        var output = Path.Combine(temporary, "receipt-plugin.esp");
        File.Copy(esp, output);
        var receipt = BuildReceipt.PluginPath(output);
        var inputs = BuildReceipt.CaptureInputs(checkout, BuildComponent.Plugin);
        BuildReceipt.Write(checkout, BuildComponent.Plugin, inputs, [output], receipt);
        BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt);
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.PapyrusCore, [output], receipt));
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt + ".absent"));
        File.AppendAllText(output, "changed output");
        Artifacts.ValidateEsp(output);
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(esp, output, true);
        var source = Path.Combine(checkout, "src/plugin/Records/Quests.cs");
        File.AppendAllText(source, "\n// another revision\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        Reject(() => BuildReceipt.Write(checkout, BuildComponent.Plugin, inputs, [output], receipt));
        File.Copy(Path.Combine(root, "src/plugin/Records/Quests.cs"), source, true);
        var tool = Path.Combine(checkout, "tools/ModTools/Commands.cs");
        File.AppendAllText(tool, "\n// changed generator\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(Path.Combine(root, "tools/ModTools/Commands.cs"), tool, true);
        var added = Path.Combine(checkout, "src/plugin/NewRecord.cs");
        File.WriteAllText(added, "// new input\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Delete(added);
        File.Delete(source);
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(Path.Combine(root, "src/plugin/Records/Quests.cs"), source);
        BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt);
        File.WriteAllText(receipt, "{}");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
    }
}
