namespace ModTools;

internal static class ProjectPaths
{
    internal const string Version = "VERSION";
    internal const string Identity = "mod.json";
    internal const string CoreScripts = "src/papyrus/core";
    internal const string Patch3DnpcScripts = "src/papyrus/patches/3dnpc";
    internal const string ScriptsOutput = "Scripts";
    internal const string Patch3DnpcOutput = "Patches/3DNPC/Scripts";
    internal const string ToolProject = "tools/ModTools/ModTools.csproj";
    internal const string ToolLock = "tools/ModTools/packages.lock.json";
    internal const string PathDefaults = "tools/paths.json";
    internal const string LocalPaths = ".tools/local.json";
    internal const string License = "LICENSE";
    internal const string Notice = "NOTICE";
    internal const string Credits = "CREDITS.md";

    internal static readonly string[] ReleaseNotices = [License, Notice, Credits];
}
