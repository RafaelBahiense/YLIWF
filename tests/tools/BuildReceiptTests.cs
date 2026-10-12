using ModTools.Packaging;

namespace ModTools.Tests;

public sealed class BuildReceiptTests : TestWorkspace
{
    private readonly string checkout;
    private readonly string output;
    private readonly string receipt;
    private readonly Dictionary<string, string> inputs;

    public BuildReceiptTests()
    {
        try
        {
            checkout = CopyCheckout("receipt-checkout");
            output = CreatePlugin();
            receipt = BuildReceipt.PluginPath(output);
            inputs = BuildReceipt.CaptureInputs(checkout, BuildComponent.Plugin);
            BuildReceipt.Write(checkout, BuildComponent.Plugin, inputs, [output], receipt);
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    [Fact]
    public void ReceiptIdentity()
    {
        BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt);
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.PapyrusCore, [output], receipt));
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt + ".absent"));
    }

    [Fact]
    public void ChangedOutput()
    {
        File.AppendAllText(output, "changed output");
        Artifacts.ValidateEsp(output);
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
    }

    [Fact]
    public void ChangedPluginInputs()
    {
        var source = Path.Combine(checkout, "src/plugin/Records/Quests.cs");
        File.AppendAllText(source, "\n// another revision\n");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Write(checkout, BuildComponent.Plugin, inputs, [output], receipt));
        File.Copy(Path.Combine(root, "src/plugin/Records/Quests.cs"), source, true);

        var tool = Path.Combine(checkout, "tools/ModTools/Commands.cs");
        File.AppendAllText(tool, "\n// changed generator\n");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(Path.Combine(root, "tools/ModTools/Commands.cs"), tool, true);
    }

    [Fact]
    public void ChangedInputSet()
    {
        var added = Path.Combine(checkout, "src/plugin/NewRecord.cs");
        File.WriteAllText(added, "// new input\n");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Delete(added);

        var source = Path.Combine(checkout, "src/plugin/Records/Quests.cs");
        File.Delete(source);
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
        File.Copy(Path.Combine(root, "src/plugin/Records/Quests.cs"), source);
        BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt);
    }

    [Fact]
    public void NativeInputs()
    {
        var receipt = Path.Combine(temporary, "native.build.json");
        var inputs = BuildReceipt.CaptureInputs(checkout, BuildComponent.Native);
        BuildReceipt.Write(checkout, BuildComponent.Native, inputs, [output], receipt);
        BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt);

        var source = Path.Combine(checkout, "src/native/Controller.cpp");
        File.AppendAllText(source, "\n// changed native input\n");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt));
        File.Copy(Path.Combine(root, "src/native/Controller.cpp"), source, true);

        var addon = Path.Combine(checkout, "src/addons/new/addon.json");
        Directory.CreateDirectory(Path.GetDirectoryName(addon)!);
        File.WriteAllText(addon, """{"name":"New"}""");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt));
        File.Delete(addon);
        BuildReceipt.Validate(checkout, BuildComponent.Native, [output], receipt);
    }

    [Fact]
    public void MalformedReceipt()
    {
        File.WriteAllText(receipt, "{}");
        Assert.Throws<InvalidDataException>(() => BuildReceipt.Validate(checkout, BuildComponent.Plugin, [output], receipt));
    }
}
