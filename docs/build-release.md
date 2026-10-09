# Build and release

Run commands from the repository root in PowerShell. Native and Papyrus
compilation require Windows.

## First setup

Install Git for Windows, .NET 10 and Visual Studio 2022 with:
**Desktop development with C++**, a Windows SDK and **C++ CMake tools for Windows**.

```powershell
.\setup.ps1 -VanillaScriptsZip 'C:\path\to\Creation Kit\Data\Scripts.zip'
.\setup.ps1 -Check
.\build.ps1
```

Alternatively, `-VanillaSources` accepts a directory containing vanilla
`.psc` files and `TESV_Papyrus_Flags.flg`. `-Check` reports missing inputs
without changing anything.

Setup downloads pinned dependencies and prepares imports in ignored `.tools/`.
It reuses validated inputs. Machine-wide SDKs must be installed separately.
Overrides `-CommonLib`, `-Vcpkg` and `-Compiler` are stored in
`.tools/local.json`; repository defaults are in `tools/paths.json`.
Build also accepts `-Flags` and `-ImportDirectories`.

## Everyday commands

```powershell
.\build.ps1
.\build.ps1 -Target Native -Configuration Debug
.\build.ps1 -Target Plugin
.\build.ps1 -Target Scripts
.\build.ps1 -Target Test
.\build.ps1 -Target Verify
.\build.ps1 -Target Package
.\build.ps1 -Clean
```

| Target | Result |
| --- | --- |
| `All` (default) | Build, validate and package core and declared add-ons |
| `Native` | DLLs in `build/native/<configuration>` |
| `Plugin` | ESP in `build/plugin` |
| `Scripts` | PEX files in `build/papyrus/Scripts` |
| `Test` | Native and .NET checks |
| `Verify` | Native builds, tests and ESP generation; no Bethesda imports needed |
| `Package` | Package existing outputs into `build/dist` |
| `Repackage` | Regenerate ESP/scripts for a supplied DLL and matching source ZIP |

Build receipts reject stale or mixed outputs; rebuild the affected component.
CMake discovers core sources in `src/native/` and each add-on's `native/` folder.
New `.cpp` files need no build-list edits; tests remain explicit targets.
`-Clean` resets native outputs while keeping binary caches.
Failed Papyrus stages remain in `build/papyrus/.staging-*`.

Add-ons are discovered from `src/addons/**/addon.json` and packaged separately
as `YouLeadIWillFollow-<version>-<addon>.zip`. They share the source archive
and notices. Supplied-DLL repackaging includes declared add-ons beside the core
DLL only when their hashes match the source manifest.

VS Code provides build/test tasks. Generate the matching compile database,
select Release or Debug x64, install the Debug DLL, then use
**Attach to Skyrim (native)** to select the game process.

## CI and caching

CI runs `Verify`. It caches pinned downloads, NuGet, vcpkg binaries and CommonLib
build outputs. Dependency keys track pins and ports; CommonLib keys also track
build settings, runner, compiler, SDK and workspace path. Mod objects always
rebuild. A new key requires one full build.

## Packaging

`VERSION` supplies three or four numeric components. Outputs are:
`YouLeadIWillFollow-<version>.zip`, its `-source.zip`, and optional add-on ZIPs.

To repackage an external DLL:

```powershell
.\build.ps1 -Target Repackage -Dll 'C:\path\YouLeadIWillFollow.dll' -SourceArchive 'C:\path\matching-source.zip'
```

Sources include the actual CommonLib checkout and vcpkg dependency sources.
Restore missing sources before packaging cached binaries. Extracted source
archives contain `dependencies/`; pass
`-CommonLib './dependencies/CommonLibSSE-NG'` when rebuilding.

Packaging validates hashes, receipts and notices before replacing archives.
Publication rolls back if any replacement fails.
`-OutputDirectory` changes the destination.

Release paths and Papyrus headers omit machine identity. ZIPs use HEAD's UTC
commit date, rounded to two seconds, without copied filesystem attributes.
`SOURCE_DATE_EPOCH` overrides it; supplied source archives retain their date.
Debug builds keep local paths for debugging.

## Dependency notices

Packaging reads CommonLib's `LICENSE` and vcpkg's `share/<port>/copyright`.
Missing notices fail packaging. Source archives contain CommonLib, fmt, spdlog,
rapidcsv and SKSE-MCP notices. Binary archives omit fmt under its compiled-code
exception and take the other notices from the matching source archive.

## Publishing a release

1. Build, run tests and complete [in-game verification](debugging.md#verification).
2. Publish matching source beside each binary at no extra charge. Retain sources
   while their binaries remain downloadable.
3. Preserve `LICENSE`, `NOTICE`, `CREDITS.md` and dependency notices.

Update `NOTICE` for attribution, lineage or licensing changes; individual fixes
belong in Git history or release notes. Build commands do not upload releases.

## Installation and explicit deployment

Install ZIPs contain Data contents:

```text
You Lead, I Will Follow.esp
Scripts/<core PEX files>
SKSE/Plugins/YouLeadIWillFollow.dll
docs/YouLeadIWillFollow/<notices and licenses>
```

Use a mod manager. `-DeployTo 'C:\MO2\mods\YouLeadIWillFollow'` explicitly
deploys the core after packaging. The mod creates a missing INI; releases omit
it to preserve settings. Documentation and build inputs belong in the source ZIP.

## Optional 3DNPC patch

```powershell
.\build.ps1 -Include3DNPC -ImportDirectories @(
  'C:\3DNPC\Source\Scripts',
  'C:\path\to\prepared\SKSE',
  'C:\path\to\vanilla\Source\Scripts'
)
```

The import list replaces defaults and must include `SetHirelingRehire3DNPC.psc`.
Install the separate `-3DNPC.zip` after YLIWF and Interesting NPCs.
Both core and patch compilation must pass; `-DeployTo` deploys only the core.

See [development tools](../tools/README.md) for inspection and C# analysis.
