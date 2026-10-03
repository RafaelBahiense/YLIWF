[CmdletBinding()]
param(
    [ValidateSet('All','Native','Plugin','Scripts','Package','Test')][string]$Target = 'All',
    [ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration = 'Release',
    [switch]$Clean,
    [switch]$Include3DNPC,
    [string[]]$ImportDirectories,
    [string]$CommonLib,
    [string]$Vcpkg,
    [string]$Compiler,
    [string]$Flags,
    [string]$Dll,
    [string]$SourceArchive,
    [string]$OutputDirectory,
    [string]$Wine,
    [string]$DeployTo
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/tools/Environment.ps1"
Push-Location $RepoRoot
try {
    if ($DeployTo -and $Target -notin @('All','Package')) { throw '-DeployTo requires All or Package.' }
    if ($Dll -and $Target -notin @('All','Package')) { throw '-Dll is supported for All or Package.' }
    if (!$OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot 'build/dist' }
    $OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
    $paths = Get-LocalPaths
    foreach ($name in @('CommonLib','Vcpkg','Compiler','Flags')) {
        $value = Get-Variable -Name $name -ValueOnly
        if ($value) { $paths[$name] = [IO.Path]::GetFullPath($value) }
    }
    if ($ImportDirectories) { $paths.Imports = @($ImportDirectories | ForEach-Object { [IO.Path]::GetFullPath($_) }) }
    $preset = $Configuration.ToLowerInvariant()
    $native = Join-Path $RepoRoot "build/native/$preset"
    if ($Target -in @('All','Native','Test') -and !$Dll) {
        Initialize-Native
        $env:VCPKG_ROOT = $paths.Vcpkg
        if ($Clean) { $native += '-clean-' + [guid]::NewGuid().ToString('N') }
        Invoke-Tool cmake @('--preset',$preset,'-B',$native,('-DCOMMONLIB_SSE_FOLDER=' + $paths.CommonLib))
        $nativeTargets = if ($Target -eq 'Test') { @('debug_rules_test','controller_rules_test') } else { @('mod') }
        Invoke-Tool cmake (@('--build',$native,'--target') + $nativeTargets + @('--parallel'))
    }
    if ($Target -eq 'Native') { Write-Host "Built: $native/$($ModIdentity.binaryName).dll"; return }
    Initialize-DotNet
    Restore-ModTools
    if ($Target -eq 'Test') {
        Invoke-Tool ctest @('--test-dir',$native,'--output-on-failure')
        Invoke-Tool dotnet @('restore','tests/tools/ModTools.Tests.csproj','--locked-mode','--configfile','tools/NuGet.Config')
        Invoke-Tool dotnet @('run','--project','tests/tools/ModTools.Tests.csproj','-c','Release','--no-restore','--',$RepoRoot)
        Write-Host 'All tests passed.'
        return
    }
    $stage = Join-Path $RepoRoot ('build/mod/.staging-' + [guid]::NewGuid().ToString('N'))
    $espDirectory = Join-Path $stage 'esp'
    $papyrus = Join-Path $stage 'papyrus'
    if ($Target -in @('All','Plugin')) { Invoke-ModTools @('generate',$espDirectory) }
    if ($Target -in @('All','Scripts')) {
        $arguments = @('scripts','--root',$RepoRoot,'--compiler',$paths.Compiler,'--flags',$paths.Flags,'--imports',($paths.Imports -join ';'),'--output',$papyrus)
        if ($Include3DNPC) { $arguments += '--include-3dnpc' }
        if ($Wine) { $arguments += @('--wine',$Wine) }
        Invoke-ModTools $arguments
    }
    if ($Target -eq 'Package') {
        $espDirectory = Join-Path $RepoRoot 'build/plugin'
        $papyrus = Join-Path $RepoRoot 'build/papyrus'
    }
    if ($Target -in @('All','Package')) {
        if (!$Dll) { $Dll = Join-Path $native ($ModIdentity.binaryName + '.dll') }
        $mode = if ($Target -eq 'All' -and !$PSBoundParameters.ContainsKey('Dll')) { 'full source build' } else { 'supplied DLL' }
        $arguments = @('package','--root',$RepoRoot,'--esp',(Join-Path $espDirectory $ModIdentity.pluginFile),'--dll',$Dll,'--papyrus',$papyrus,'--output',$OutputDirectory,'--mode',$mode)
        if ($SourceArchive) { $arguments += @('--source-archive',([IO.Path]::GetFullPath($SourceArchive))) }
        else { $arguments += @('--commonlib',$paths.CommonLib,'--vcpkg',$paths.Vcpkg,'--native-build',$native) }
        if ($Include3DNPC) { $arguments += '--include-3dnpc' }
        Invoke-ModTools $arguments
    }
    if ($Target -in @('All','Plugin')) {
        New-Item -ItemType Directory -Force -Path build/plugin | Out-Null
        Copy-Item -LiteralPath (Join-Path $espDirectory $ModIdentity.pluginFile) -Destination build/plugin -Force
        Copy-Item -LiteralPath (Join-Path $espDirectory ($ModIdentity.pluginFile + '.build.json')) -Destination build/plugin -Force
    }
    if ($Target -in @('All','Scripts')) {
        New-Item -ItemType Directory -Force -Path build/papyrus/Scripts | Out-Null
        Copy-Item -Path (Join-Path $papyrus 'Scripts/*.pex') -Destination build/papyrus/Scripts -Force
        Copy-Item -LiteralPath (Join-Path $papyrus 'Scripts/build-receipt.json') -Destination build/papyrus/Scripts -Force
        if ($Include3DNPC) {
            New-Item -ItemType Directory -Force -Path build/papyrus/Patches/3DNPC/Scripts | Out-Null
            Copy-Item -Path (Join-Path $papyrus 'Patches/3DNPC/Scripts/*.pex') -Destination build/papyrus/Patches/3DNPC/Scripts -Force
            Copy-Item -LiteralPath (Join-Path $papyrus 'Patches/3DNPC/Scripts/build-receipt.json') -Destination build/papyrus/Patches/3DNPC/Scripts -Force
        }
    }
    if ($DeployTo) {
        $version = [IO.File]::ReadAllText((Join-Path $RepoRoot 'VERSION')).Trim()
        $deployment = Join-Path $stage 'deployment'
        Expand-Archive -LiteralPath (Join-Path $OutputDirectory "$($ModIdentity.binaryName)-$version.zip") -DestinationPath $deployment
        New-Item -ItemType Directory -Force -Path $DeployTo | Out-Null
        foreach ($file in Get-ChildItem -LiteralPath $deployment -File -Recurse) {
            $relative = $file.FullName.Substring($deployment.Length + 1)
            $destination = Join-Path $DeployTo $relative
            New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
        }
        Write-Host "Deployed core mod: $DeployTo"
    }
    if ($stage -and (Test-Path -LiteralPath $stage)) {
        $resolvedStage = [IO.Path]::GetFullPath($stage)
        $stagingRoot = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'build/mod')) + [IO.Path]::DirectorySeparatorChar
        if (!$resolvedStage.StartsWith($stagingRoot, [StringComparison]::OrdinalIgnoreCase) -or
            ![IO.Path]::GetFileName($resolvedStage).StartsWith('.staging-')) { throw 'Unsafe build staging cleanup path.' }
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }
} catch { Write-Error $_; exit 1 } finally { Pop-Location }
