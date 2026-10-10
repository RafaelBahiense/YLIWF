[CmdletBinding()]
param(
    [ValidateSet('All', 'CSharp', 'Cpp')] [string] $Language = 'All',
    [switch] $Fix,
    [switch] $NativeAnalysis,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')] [string] $Configuration = 'Release',
    [string] $ClangFormat
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/tools/Environment.ps1"
Push-Location $RepoRoot
try {
    if ($NativeAnalysis -and $Language -eq 'CSharp') { throw '-NativeAnalysis requires Cpp or All.' }

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
        if (-not $ClangFormat) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
            if (Test-Path -LiteralPath $vswhere) {
                $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
                foreach ($relative in @('VC/Tools/Llvm/x64/bin/clang-format.exe', 'VC/Tools/Llvm/bin/clang-format.exe')) {
                    $candidate = if ($vs) { Join-Path $vs $relative }
                    if ($candidate -and (Test-Path -LiteralPath $candidate)) { $ClangFormat = $candidate; break }
                }
            }
            if (-not $ClangFormat) {
                $command = Get-Command clang-format -CommandType Application -ErrorAction SilentlyContinue
                if ($command) { $ClangFormat = $command.Source }
            }
        }
        if (-not $ClangFormat) { throw 'clang-format was not found. Supply -ClangFormat with an existing executable path.' }

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

        if ($NativeAnalysis) {
            Initialize-Native
            $native = Join-Path $RepoRoot ('build/native/' + $Configuration.ToLowerInvariant())
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
