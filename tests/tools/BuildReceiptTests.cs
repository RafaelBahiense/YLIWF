using ModTools.Packaging;

internal static class BuildReceiptTests
{
    public static void Run(string root, string temporary, string esp)
    {
        var checkout = CopyCheckout(root, Path.Combine(temporary, "receipt-checkout"));
        var output = Path.Combine(temporary, "receipt-plugin.esp");
        File.Copy(esp, output);
        var receipt = BuildReceipt.PluginPath(output);
        var inputs = BuildReceipt.CaptureInputs(checkout, BuildComponent.Plugin);
        BuildReceipt.Write(checkout, BuildComponent.Plugin, inputs, [output], receipt);

        CheckReceiptIdentity(checkout, output, receipt);
        CheckChangedOutput(checkout, esp, output, receipt);
        CheckChangedPluginInputs(root, checkout, output, receipt, inputs);
        CheckChangedInputSet(root, checkout, output, receipt);
        CheckNativeInputs(root, checkout, temporary, output);
        CheckMalformedReceipt(checkout, output, receipt);
    }

    private static string CopyCheckout(string root, string checkout)
    {
        // Mutate a separate checkout, never maintained sources.
        foreach (var (name, file) in SourceSnapshot.ProjectFiles(root))
        {
            var destination = Path.Combine(checkout, name);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(file, destination);
        }
        return checkout;
    }

    private static void CheckReceiptIdentity(string checkout, string output, string receipt)
    {
        BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt);
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.PapyrusCore, [output], receipt));
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt + ".absent"));
    }

    private static void CheckChangedOutput(string checkout, string esp, string output, string receipt)
    {
        File.AppendAllText(output, "changed output");
        Artifacts.ValidateEsp(output);
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(esp, output, true);
    }

    private static void CheckChangedPluginInputs(string root, string checkout, string output, string receipt,
        Dictionary<string, string> inputs)
    {
        var source = Path.Combine(checkout, "src/plugin/Records/Quests.cs");
        File.AppendAllText(source, "\n// another revision\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        Reject(() => BuildReceipt.Write(checkout, BuildComponent.Plugin, inputs, [output], receipt));
        File.Copy(Path.Combine(root, "src/plugin/Records/Quests.cs"), source, true);

        var tool = Path.Combine(checkout, "tools/ModTools/Commands.cs");
        File.AppendAllText(tool, "\n// changed generator\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(Path.Combine(root, "tools/ModTools/Commands.cs"), tool, true);
    }

    private static void CheckChangedInputSet(string root, string checkout, string output, string receipt)
    {
        var added = Path.Combine(checkout, "src/plugin/NewRecord.cs");
        File.WriteAllText(added, "// new input\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Delete(added);

        var source = Path.Combine(checkout, "src/plugin/Records/Quests.cs");
        File.Delete(source);
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(Path.Combine(root, "src/plugin/Records/Quests.cs"), source);
        BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt);
    }

    private static void CheckNativeInputs(string root, string checkout, string temporary, string output)
    {
        var receipt = Path.Combine(temporary, "native.build.json");
        var inputs = BuildReceipt.CaptureInputs(checkout, BuildComponent.Native);
        BuildReceipt.Write(checkout, BuildComponent.Native, inputs, [output], receipt);
        BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt);

        var source = Path.Combine(checkout, "src/native/Controller.cpp");
        File.AppendAllText(source, "\n// changed native input\n");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt));
        File.Copy(Path.Combine(root, "src/native/Controller.cpp"), source, true);

        var addon = Path.Combine(checkout, "src/addons/new/addon.json");
        Directory.CreateDirectory(Path.GetDirectoryName(addon)!);
        File.WriteAllText(addon, """{"name":"New"}""");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt));
        File.Delete(addon);
        BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt);
    }

    private static void CheckMalformedReceipt(string checkout, string output, string receipt)
    {
        File.WriteAllText(receipt, "{}");
        Reject(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
    }

    private static void Reject(Action action)
    {
        try
        {
            action();
        }
        catch (InvalidDataException)
        {
            return;
        }
        throw new InvalidOperationException("Mismatched build receipt was accepted");
    }
}
