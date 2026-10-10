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

## Linting

```powershell
./lint.ps1                         # Check C# and C++
./lint.ps1 -Language CSharp        # C# only
./lint.ps1 -Language Cpp           # C++ formatting only
./lint.ps1 -Fix                    # Apply supported formatting/style fixes
./lint.ps1 -NativeAnalysis         # Also run MSVC static analysis
```

C# uses SDK analyzers and `dotnet format` with `.editorconfig`, including
unused imports, initializers and generated-regex suggestions. Unfixable findings
need a manual change. Compiler/analyzer warnings fail lint; unavailable NuGet
vulnerability data (`NU1900`) remains a warning.

C++ formatting uses `.clang-format` and Visual Studio's existing Clang-Format;
`-ClangFormat` selects another executable. Native analysis requires a configured
native build (`./build.ps1 -Target Native`), checks core/add-on code and native
tests, and treats findings as errors. Its separate objects leave release flags
and DLLs untouched; dependency code is excluded from analysis diagnostics.
Use `-Configuration Debug` to analyze an existing Debug build.

VS Code has **Lint C# and C++**; Windows CI also runs native analysis. Checks
never rewrite source unless `-Fix` is specified. Review fixes and run relevant
tests afterward. No additional tool installation is performed.
