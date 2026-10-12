namespace ModTools.Packaging;

public sealed record NativeBuildInputs(string CommonLib, string Vcpkg, string BuildDirectory)
{
    public Dictionary<string, string> Capture() => SourceSnapshot.NativeDependencyFiles(CommonLib, Vcpkg, BuildDirectory)
        .ToDictionary(file => file.Key, file => Artifacts.HashFile(file.Value), StringComparer.Ordinal);
}
