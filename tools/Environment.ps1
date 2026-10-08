$RepoRoot = Split-Path $PSScriptRoot -Parent

function Invoke-Tool {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed (exit $LASTEXITCODE)" }
}

function Initialize-DotNet {
    $env:DOTNET_CLI_HOME = Join-Path $RepoRoot '.tools/dotnet-home'
    $env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
    $env:DOTNET_NOLOGO = '1'
    $env:NUGET_PACKAGES = Join-Path $RepoRoot '.tools/nuget/packages'
    $env:NUGET_HTTP_CACHE_PATH = Join-Path $RepoRoot '.tools/nuget/http'
    if (!(Get-Command dotnet -ErrorAction SilentlyContinue)) { throw 'Install the .NET 10 SDK, then reopen PowerShell.' }
    $version = & dotnet --version
    if ($LASTEXITCODE -ne 0 -or $version -notmatch '^10\.') { throw 'The repository needs the .NET 10 SDK (global.json).' }
}

function Initialize-Native {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022/Build Tools with Desktop development with C++ and CMake tools.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Visual Studio C++ tools are missing. Install Desktop development with C++ and a Windows SDK.' }
    $dev = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    $environment = & $env:ComSpec /d /c "call `"$dev`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio could not initialize its x64 compiler environment.' }
    $developerPaths = @()
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') {
            if ($matches[1] -ieq 'PATH') { $developerPaths += $matches[2] }
            else { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
        }
    }
    if (!$developerPaths) { throw 'Visual Studio did not provide a compiler PATH.' }
    $cmakeBin = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin'
    $ninjaBin = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
    # Some Windows hosts export both PATH and Path; retain both so the compiler PATH is not overwritten.
    $developerPath = (($developerPaths -join ';') -split ';' | Select-Object -Unique) -join ';'
    $env:PATH = "$cmakeBin;$ninjaBin;$developerPath"
    foreach ($tool in @('cl','rc','cmake','ninja')) {
        if (!(Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Missing $tool. Add the Windows SDK and CMake tools in Visual Studio Installer." }
    }
}

function Get-LocalPaths {
    $paths = @{}
    $defaults = [IO.File]::ReadAllText((Join-Path $RepoRoot 'tools/paths.json')) | ConvertFrom-Json
    foreach ($property in $defaults.PSObject.Properties) {
        $values = @($property.Value | ForEach-Object { [IO.Path]::GetFullPath((Join-Path $RepoRoot $_)) })
        $paths[$property.Name] = if ($property.Name -eq 'Imports') { $values } else { $values[0] }
    }
    $config = Join-Path $RepoRoot '.tools/local.json'
    if (Test-Path -LiteralPath $config) {
        $local = [IO.File]::ReadAllText($config) | ConvertFrom-Json
        foreach ($property in $local.PSObject.Properties) { $paths[$property.Name] = $property.Value }
    }
    return $paths
}

function Restore-ModTools {
    Invoke-Tool dotnet @('restore', (Join-Path $RepoRoot 'tools/ModTools/ModTools.csproj'), '--locked-mode', '--configfile', (Join-Path $RepoRoot 'tools/NuGet.Config'))
    Invoke-Tool dotnet @('build', (Join-Path $RepoRoot 'tools/ModTools/ModTools.csproj'), '-c', 'Release', '--no-restore')
}

function Invoke-ModTools {
    param([string[]]$Arguments)
    Invoke-Tool dotnet (@((Join-Path $RepoRoot 'tools/ModTools/bin/Release/net10.0/ModTools.dll')) + $Arguments)
}
