using Mutagen.Bethesda.Skyrim;
using Noggog;

internal static class PackageExtensions
{
    internal static Package WithUnusedScheduleBytes(this Package package)
    {
        // PSDT contains three schedule bytes that xEdit marks as unused.
        // https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsTES5.pas#L10555-L10577
        // Mutagen exposes them as Unknown3:
        // https://github.com/Mutagen-Modding/Mutagen/blob/0.54.4/Mutagen.Bethesda.Skyrim/Records/Major%20Records/Package.xml#L16-L25
        // Allocate a separate buffer for each package so their bytes cannot affect one another.
        package.Unknown3 = new MemorySlice<byte>(new byte[3]);
        return package;
    }
}
