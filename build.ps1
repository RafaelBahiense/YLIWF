[CmdletBinding()]
param(
    [ValidateSet('All','Native','Plugin','Scripts','Package','Test','Verify','Repackage')][string]$Target = 'All',
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
    [string]$DeployTo
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/tools/Environment.ps1"
Push-Location $RepoRoot
try {
    if ($Target -in @('All','Native','Test','Verify')) { Initialize-Native }
    Initialize-DotNet
    Restore-ModTools
    $arguments = @('build','--root',$RepoRoot,'--target',$Target,'--configuration',$Configuration)
    $names = @{
        CommonLib = 'commonlib'; Vcpkg = 'vcpkg'; Compiler = 'compiler'; Flags = 'flags'
        Dll = 'dll'; SourceArchive = 'source-archive'; OutputDirectory = 'output'
        DeployTo = 'deploy-to'
    }
    foreach ($name in $names.Keys) {
        if ($PSBoundParameters.ContainsKey($name)) { $arguments += @(('--' + $names[$name]), $PSBoundParameters[$name]) }
    }
    if ($ImportDirectories) { $arguments += @('--imports', ($ImportDirectories -join ';')) }
    if ($Clean) { $arguments += '--clean' }
    if ($Include3DNPC) { $arguments += '--include-3dnpc' }
    Invoke-ModTools $arguments
} catch { Write-Error $_; exit 1 } finally { Pop-Location }
