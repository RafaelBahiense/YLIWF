# Build, development, and releases

Run commands from the repository root in ordinary PowerShell.

## First setup

Install Git for Windows, the .NET 10 SDK, and Visual Studio 2022 Community or
Build Tools with **Desktop development with C++**, a Windows SDK, and
**C++ CMake tools for Windows**.

```powershell
.\setup.ps1 -VanillaScriptsZip 'C:\path\to\Creation Kit\Data\Scripts.zip'
.\setup.ps1 -Check
.\build.ps1
```

Alternatively, use `-VanillaSources` with a directory containing vanilla `.psc`
files and `TESV_Papyrus_Flags.flg`. Setup reuses validated local inputs on repeat
runs. `-Check` reports missing inputs without downloads or changes.

Setup locates Visual Studio, downloads dependencies pinned in
`tools/dependencies.json`, checks hashes, prepares SKSE/vanilla imports, and
builds ModTools when preparing Papyrus imports. Downloads and local configuration stay in ignored
`.tools/`; machine-wide SDKs and workloads must be installed separately.

Use `-CommonLib`, `-Vcpkg`, or `-Compiler` for custom locations. Setup records
them in `.tools/local.json`; build accepts these overrides plus `-Flags` and
`-ImportDirectories`. Custom checkouts replace the pinned inputs. Do not share
machine-local configuration between computers. Repository defaults live in
`tools/paths.json`, shared by setup and ModTools.

## Everyday commands

GitHub Actions caches pinned downloads, NuGet packages, vcpkg binary packages,
and CommonLib build outputs. Native caches are separated by runner image,
compiler, Windows SDK, workspace path, dependency pins, and CMake configuration.
CommonLib's precompiled headers and compiled objects can be reused; mod objects
and binaries are excluded and always rebuild. The first run after a cache change
performs a full build.

```powershell
.\build.ps1
.\build.ps1 -Target Native -Configuration Debug
.\build.ps1 -Target Plugin
.\build.ps1 -Target Scripts
.\build.ps1 -Target Test
.\build.ps1 -Target Verify
.\build.ps1 -Clean
```

VS Code provides matching build, test, and setup-check tasks; `Ctrl+Shift+B`
builds the release. Select `Release x64` or `Debug x64` in C/C++ configuration
after generating that build's compile database. For native debugging, install
the matching Debug DLL, start Skyrim, and use **Attach to Skyrim (native)** to
select its process; symbols come from the local Debug build.

`All` builds the DLL, generates the ESP, compiles five core scripts, validates
the outputs, and creates the binary and matching source ZIPs in `build/dist`.
`VERSION` contains three or four numeric components.

Native builds reuse `build/native/<configuration>`. `-Clean` creates a fresh
native/vcpkg build directory while retaining usable binary caches. Failed Papyrus
stages remain under `build/papyrus/.staging-*` for inspection; successful stages
are removed. Release archives are staged separately until validation passes.

`Verify` builds native plugins, runs all tests, and generates the ESP, without
requiring Bethesda imports. CI uses this same command. PowerShell initializes
the environment and builds ModTools once; the C# coordinator owns the build
sequence. CMake/Ninja own native compilation.

Individual targets write the ESP to `build/plugin` and PEX files to
`build/papyrus/Scripts`. `-Target Package` packages these existing outputs.
Use `-OutputDirectory` for another destination. Native builds embed
`assets/settings.ini` as the default configuration and first-launch template.
DLL, ESP and PEX outputs have local build receipts recording source and output hashes.
Packaging rejects missing receipts, changed inputs, and mixed outputs; rebuild the
affected component. Receipts remain beside local outputs and are not installed.
Install ZIPs contain only notices and licenses under `docs`; documentation,
default settings sources, and the build manifest remain in the source ZIP.
Every ZIP entry is still checked against its input hash before publication.

## Source archives and supplied DLLs

`-Target Package` packages local build outputs and snapshots their sources.
Use `-Target Repackage -Dll <dll> -SourceArchive <zip>` for externally supplied
binaries. This regenerates the ESP and compiles scripts, then checks the DLL hash, compiled inputs, dependency
snapshots, notices, and archive hashes.

Full builds snapshot the actual CommonLib checkout and vcpkg source trees under
`buildtrees`. If a binary cache lacks sources, restore those exact sources before
packaging. Source ZIPs exclude saves and machine-local configuration.

Dependencies are under `dependencies/`. When building an extracted source ZIP,
use `-CommonLib './dependencies/CommonLibSSE-NG'`. `provenance/` records installed
package versions. Machine-specific CMake caches and compiler commands are omitted.

Release builds map native source paths to stable names. Papyrus headers contain
only source filenames, with compiler user/computer names and timestamps cleared;
debug line mappings remain available. ZIP entries use HEAD's commit date in UTC
(rounded to ZIP's two-second precision) and no copied filesystem attributes.
`SOURCE_DATE_EPOCH` can override the date with Unix seconds; extracted source
archives retain it in the build manifest for builds without Git. Repackaging
supplied sources retains their original archive date.
Debug builds retain local native source paths for
the debugger and are intended for local development.

All requested archives are prepared and validated before replacing releases.
Publication rolls back earlier replacements if another archive in the group fails.

## Dependency notices

Packaging reads CommonLib's `LICENSE` and the native build's
`vcpkg_installed/x64-windows-static/share/<port>/copyright` files.
Missing or empty notices fail packaging.

The source ZIP includes CommonLibSSE-NG, fmt, spdlog, rapidcsv, and SKSE-MCP
notices. Binary ZIPs take four from that matching source archive, omitting fmt
under its compiled-code exception. Supplied-DLL packaging uses the supplied
archive's notices. Review licenses when updating dependencies or adding assets.

## Publishing a release

1. Build, run the tests above, and perform [in-game verification](debugging.md#verification)
   on a new game and an existing YLIWF save.
2. Upload the matching source ZIP beside every core or optional binary download
   at no additional charge. Retain older sources while their binaries are
   downloadable; a moving branch alone does not identify an older binary's source.
3. Preserve `LICENSE`, `NOTICE`, `CREDITS.md`, and upstream/dependency notices.
   Describe YLIWF as an independent architectural rewrite derived from SFF and
   retain the original-project links.

Update `NOTICE` when attribution, ownership, lineage, or licensing changes.
Individual features and fixes belong in Git history or release notes.
Build commands create local archives; uploading them is a separate step.

## Installation and explicit deployment

The ZIP contains Data contents directly at its root:

```text
You Lead, I Will Follow.esp
Scripts/<five core PEX files>
SKSE/Plugins/YouLeadIWillFollow.dll
docs/YouLeadIWillFollow/<LICENSE, NOTICE, CREDITS.md, and dependency notices>
```

Install through your mod manager; runtime requirements are in the
[README](../README.md#installation). `-DeployTo 'C:\MO2\mods\YouLeadIWillFollow'`
deploys the core mod after successful packaging. The archive contains no user
INI; the running mod creates it only when missing and preserves existing files.
Builds deploy only when that option is supplied.

## Optional 3DNPC patch

```powershell
.\build.ps1 -Include3DNPC -ImportDirectories @(
  'C:\3DNPC\Source\Scripts',
  'C:\path\to\prepared\SKSE',
  'C:\path\to\vanilla\Source\Scripts'
)
```

The import list replaces defaults; retain SKSE and vanilla sources in precedence
order. The patch needs additional imports such as `SetHirelingRehire3DNPC.psc`.
Core and patch compilations must both pass before packaging.

The separate `YouLeadIWillFollow-<version>-3DNPC.zip` contains `follower3dnpc.pex`.
Install it after YLIWF and Interesting NPCs. `-DeployTo` deploys only the core mod.

## Tests

`-Target Test` runs CTest assertions and .NET checks for generated records,
artifact integrity, source preparation, compiler failures, and release rollback.
Windows CI also generates the ESP; full Papyrus builds require vanilla imports.

Native and Papyrus compilation require Windows.
See [development tools](../tools/README.md) for direct .NET commands and analysis.
