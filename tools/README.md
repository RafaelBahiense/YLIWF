# Development tools

`ModTools` uses .NET 10 and locked Mutagen packages for builds, ESP generation,
Papyrus compilation, validation and packaging.
See the [build guide](../docs/build-release.md).

| Directory | Responsibility |
| --- | --- |
| `Building/` | Targets, paths and compiler invocation |
| `Papyrus/` | Source preparation and compilation |
| `Packaging/` | Validation and archive publication |

## Generate and inspect plugins

Edit `src/plugin/`; `build.ps1 -Target Plugin` writes the ESP to `build/plugin`.
Original plugins in `plugin/input` are local inspection inputs.

For direct CLI use:

```powershell
. ./tools/Environment.ps1
Initialize-DotNet
Restore-ModTools
Invoke-ModTools @('generate', 'build/plugin')
Invoke-ModTools @('inspect', 'build/plugin/You Lead, I Will Follow.esp')
```

`inspect` lists FormIDs, types and editor IDs. Neither command needs Skyrim masters.

## C# analysis

`.editorconfig` defines style. Use semantic names, remove unused imports and
prefer initializers. After initializing .NET and restoring:

```powershell
foreach ($project in 'tools/ModTools/ModTools.csproj', 'tests/tools/ModTools.Tests.csproj') {
    dotnet build $project -c Release --no-restore -t:Rebuild -p:AnalysisLevel=latest-recommended -p:EnforceCodeStyleInBuild=true
    dotnet format $project --no-restore --severity info --verify-no-changes
}
```

Remove `--verify-no-changes` to apply fixes, review them, then run
`build.ps1 -Target Test`. `SYSLIB1045` suggests generated regexes.
For a SARIF report, add `-p:ErrorLog=<absolute-path>.sarif`; use separate paths
under `build/` for each project.
