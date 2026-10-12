using ModTools.Packaging;

namespace ModTools.Tests;

public abstract class TestWorkspace : IDisposable
{
    protected readonly string root = FindRoot();
    protected readonly string temporary = Path.Combine(Path.GetTempPath(), "yliwf-tests-" + Guid.NewGuid().ToString("N"));

    protected TestWorkspace() => Directory.CreateDirectory(temporary);

    private static string FindRoot()
    {
        // Test Explorer can start elsewhere; the assembly remains beneath the checkout.
        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory != null; directory = directory.Parent)
        {
            if (File.Exists(Path.Combine(directory.FullName, "mod.json")) &&
                File.Exists(Path.Combine(directory.FullName, "tools/ModTools/ModTools.csproj")))
            {
                return directory.FullName;
            }
        }
        throw new DirectoryNotFoundException("Run the tests from a repository build.");
    }

    protected string CreatePlugin()
    {
        var esp = Path.Combine(temporary, Artifacts.Plugin);
        FollowerPlugin.Create().BeginWrite.ToPath(esp).WithLoadOrderFromHeaderMasters()
            .WithNoDataFolder().NoMastersListContentCheck().Write();
        BuildReceipt.Write(root, BuildComponent.Plugin, BuildReceipt.CaptureInputs(root, BuildComponent.Plugin),
            [esp], BuildReceipt.PluginPath(esp));
        return esp;
    }

    protected string CreateDll()
    {
        var dll = Path.Combine(temporary, Artifacts.Dll);
        File.WriteAllBytes(dll, TestSupport.DllFixture());
        return dll;
    }

    protected string CopyCheckout(string name)
    {
        var checkout = Path.Combine(temporary, name);
        foreach (var (relative, file) in SourceSnapshot.ProjectFiles(root))
        {
            var destination = Path.Combine(checkout, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(file, destination);
        }
        return checkout;
    }

    public void Dispose()
    {
        Directory.Delete(temporary, recursive: true);
        GC.SuppressFinalize(this);
    }
}
