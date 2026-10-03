namespace ModTools.Packaging;

public enum BuildMode
{
    FullSource,
    SuppliedDll
}

public static class BuildModes
{
    private const string FullSourceName = "full source build";
    private const string SuppliedDllName = "supplied DLL";

    public static BuildMode Parse(string? value) => value switch
    {
        null or SuppliedDllName => BuildMode.SuppliedDll,
        FullSourceName => BuildMode.FullSource,
        _ => throw new InvalidDataException($"Unknown build mode: {value}")
    };

    public static string Name(BuildMode mode) => mode switch
    {
        BuildMode.FullSource => FullSourceName,
        BuildMode.SuppliedDll => SuppliedDllName,
        _ => throw new InvalidDataException($"Unknown build mode: {mode}")
    };
}
