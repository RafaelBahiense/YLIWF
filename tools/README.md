# Development tools

`ModTools` uses .NET 10 and locked Mutagen packages for builds, ESP generation,
Papyrus compilation, validation and packaging.
See the [build guide](../docs/build-release.md).

Use the [standalone UI preview](ui-preview/README.md) to edit and test menu layouts
without starting Skyrim.

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

## Linting

```powershell
./lint.ps1                         # Check C# and C++
./lint.ps1 -Language CSharp        # C# only
./lint.ps1 -Language Cpp           # C++ formatting only
./lint.ps1 -Fix                    # Apply supported formatting/style fixes
./lint.ps1 -NativeAnalysis         # Also run MSVC static analysis
./lint.ps1 -Language Cpp -IncludeAnalysis  # Check C++ includes
./lint.ps1 -Language Cpp -IncludeAnalysis -IncludeScope src/native/combat  # Limit include checks
```

C# uses SDK analyzers and `dotnet format` with `.editorconfig`.
Compiler/analyzer warnings fail lint; unavailable NuGet
vulnerability data (`NU1900`) remains a warning.

C++ formatting uses `.clang-format` and Visual Studio's existing Clang-Format;
`-ClangFormat` selects another executable. Native analysis requires a configured
native build (`./build.ps1 -Target Native`), checks core/add-on code and native
tests, and treats findings as errors. Its separate objects leave release flags
and DLLs untouched; dependency code is excluded from analysis diagnostics.
Use `-Configuration Debug` to analyze an existing Debug build.

`-IncludeAnalysis` runs Clang-Tidy's `misc-include-cleaner` against project C++
sources and headers using the selected build's `compile_commands.json`.
Each header is analyzed as a main file so unused and missing direct includes
are reported there too. A configured native build and an existing Clang-Tidy
with this check are required; `-ClangTidy` selects an executable. No tools are
installed by lint.

`-IncludeScope` accepts files or directories and limits include analysis;
formatting still checks its normal scope. Unmatched scopes and parsing errors
fail the check. Findings are errors and full diagnostics are written to
`build/lint/include-analysis.log`. Include analysis reports findings without
rewriting headers, including when `-Fix` applies formatting changes.

VS Code provides **Lint C# and C++** and **Check C++ includes (current file)**.
Review `-Fix` changes and run relevant tests.
