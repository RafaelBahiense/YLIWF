[CmdletBinding()]
param(
    [ValidateSet('All', 'CSharp', 'Cpp')] [string] $Language = 'All',
    [switch] $Fix,
    [switch] $NativeAnalysis,
    [switch] $IncludeAnalysis,
    [string[]] $IncludeScope,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')] [string] $Configuration = 'Release',
    [string] $ClangFormat,
    [string] $ClangTidy
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/tools/Environment.ps1"
. "$PSScriptRoot/tools/IncludeAnalysis.ps1"
Push-Location $RepoRoot
try {
    if ($NativeAnalysis -and $Language -eq 'CSharp') { throw '-NativeAnalysis requires Cpp or All.' }
    if ($IncludeAnalysis -and $Language -eq 'CSharp') { throw '-IncludeAnalysis requires Cpp or All.' }
    if (($IncludeScope -or $ClangTidy) -and -not $IncludeAnalysis) { throw '-IncludeScope and -ClangTidy require -IncludeAnalysis.' }

    if ($Language -in @('All', 'CSharp')) {
        Initialize-DotNet
        $projects = @('tools/ModTools/ModTools.csproj', 'tests/tools/ModTools.Tests.csproj')
        foreach ($project in $projects) {
            Invoke-Tool 'dotnet' @('restore', $project, '--locked-mode', '--configfile', 'tools/NuGet.Config')
            $formatArguments = @('format', $project, '--no-restore', '--severity', 'info')
            if (-not $Fix) { $formatArguments += '--verify-no-changes' }
            Invoke-Tool 'dotnet' $formatArguments
        }
        foreach ($project in $projects) {
            Invoke-Tool 'dotnet' @('build', $project, '-c', 'Release', '--no-restore',
                '-p:EnforceCodeStyleInBuild=true', '-p:TreatWarningsAsErrors=true', '-p:WarningsNotAsErrors=NU1900')
        }
    }

    if ($Language -in @('All', 'Cpp')) {
        $ClangFormat = Resolve-ClangTool 'clang-format' $ClangFormat

        $files = foreach ($directory in @('include', 'src/native', 'src/addons', 'tests/native')) {
            if (Test-Path -LiteralPath $directory) {
                Get-ChildItem -LiteralPath $directory -Recurse -File |
                    Where-Object { $_.Extension -in @('.cpp', '.h', '.hpp') -and
                        ($directory -ne 'src/addons' -or $_.FullName -match '[\\/]native[\\/]') } |
                    ForEach-Object { $_.FullName }
            }
        }
        $formatFlags = if ($Fix) { @('-i') } else { @('--dry-run', '--Werror') }
        Invoke-Tool -Executable $ClangFormat -Arguments (@('--style=file', '--fallback-style=none') + $formatFlags + $files)

        if ($IncludeAnalysis -or $NativeAnalysis) {
            Initialize-Native
            $native = Join-Path $RepoRoot ('build/native/' + $Configuration.ToLowerInvariant())
        }

        if ($IncludeAnalysis) {
            $selected = @(Select-IncludeAnalysisFiles -Root $RepoRoot -Files $files -Scope $IncludeScope)
            $ClangTidy = Resolve-ClangTool 'clang-tidy' $ClangTidy
            Invoke-IncludeAnalysis -Root $RepoRoot -Native $native -ClangTidy $ClangTidy -Files $selected
        }

        if ($NativeAnalysis) {
            if (-not (Test-Path -LiteralPath (Join-Path $native 'CMakeCache.txt'))) {
                throw "Configure/build native first: ./build.ps1 -Target Native -Configuration $Configuration"
            }
            Invoke-Tool 'cmake' @('-S', $RepoRoot, '-B', $native)
            # Analysis parses full engine headers without a PCH; bound peak memory.
            Invoke-Tool 'cmake' @('--build', $native, '--target', 'native_analysis', '--parallel', '2')
        }
    }
    Write-Host 'Lint passed.'
} catch { Write-Error $_; exit 1 } finally { Pop-Location }
