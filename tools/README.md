# Development tools

`ModTools` uses .NET 10 and locked Mutagen packages for ESP generation and
inspection, Papyrus preparation and compilation, validation, and packaging.
See the [build guide](../docs/build-release.md) for setup and build targets.

## Generate and inspect plugins

Edit ESP definitions under `src/plugin/`; the project compiles them as linked
sources. `build.ps1 -Target Plugin` writes the generated ESP to `build/plugin`.
Original plugins under `plugin/input` are local inspection artifacts.

For direct CLI use, run from the repository root:

```powershell
. ./tools/Environment.ps1
Initialize-DotNet
Restore-ModTools
Invoke-ModTools @('generate', 'build/plugin')
Invoke-ModTools @('inspect', 'build/plugin/You Lead, I Will Follow.esp')
```

`inspect` lists FormIDs, record types, and editor IDs. Neither command requires
Skyrim master files.

## C# style and analysis

The root `.editorconfig` defines formatting and analyzer settings. Use semantic
names, remove unused imports, prefer object/collection initializers, and add
braces to control flow. Keep numeric suffixes only for meaningful IDs or slots.

After initializing .NET and restoring the projects:

```powershell
foreach ($project in 'tools/ModTools/ModTools.csproj', 'tests/tools/ModTools.Tests.csproj') {
    dotnet build $project -c Release --no-restore -t:Rebuild -p:AnalysisLevel=latest-recommended -p:EnforceCodeStyleInBuild=true
    dotnet format $project --no-restore --severity info --verify-no-changes
}
```

Remove `--verify-no-changes` to apply formatter fixes, then review the diff and
run `build.ps1 -Target Test`. Some editor refactorings still require manual edits.
`SYSLIB1045` is enabled as a warning for source-generated regex suggestions.

For a diagnostic report, add `-p:ErrorLog=<absolute-path>.sarif` to the build.
Use a different report path for each project and store reports under `build/`.
