[CmdletBinding()]
param(
    [switch]$Check,
    [switch]$NativeOnly,
    [string]$VanillaScriptsZip,
    [string]$VanillaSources,
    [string]$CommonLib,
    [string]$Vcpkg,
    [string]$Compiler
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/tools/Environment.ps1"
Push-Location $RepoRoot
try {
    $paths = Get-LocalPaths
    foreach ($name in @('CommonLib','Vcpkg','Compiler')) {
        $value = Get-Variable -Name $name -ValueOnly
        if ($value) { $paths[$name] = [IO.Path]::GetFullPath($value) }
    }
    if ($VanillaSources) {
        $paths.Vanilla = [IO.Path]::GetFullPath($VanillaSources)
        $paths.Flags = Join-Path $paths.Vanilla 'TESV_Papyrus_Flags.flg'
        $paths.Imports = @((Join-Path $RepoRoot '.tools/papyrus/skse'), $paths.Vanilla)
    }
    $missing = @()
    foreach ($step in @('Initialize-DotNet','Initialize-Native')) {
        try { & $step; Write-Host "OK: $step" } catch { $missing += $_.Exception.Message }
    }
    if (!(Get-Command git -ErrorAction SilentlyContinue)) { $missing += 'Install Git for Windows, then reopen PowerShell.' }
    $required = @((Join-Path $paths.CommonLib 'CMakeLists.txt'), (Join-Path $paths.Vcpkg 'vcpkg.exe'))
    if (!$NativeOnly) { $required += @($paths.Compiler, $paths.Flags, (Join-Path $paths.Imports[0] 'Actor.psc')) }
    if ($Check) {
        foreach ($file in $required) { if (Test-Path -LiteralPath $file) { Write-Host "OK: $file" } else { $missing += "Missing $file; run setup.ps1." } }
        if ($missing.Count) { throw ($missing -join "`n") }
        Write-Host 'Setup checks passed. No files were installed or restored.'
        return
    }
    if ($missing.Count) { throw ($missing -join "`n") }
    $pins = [IO.File]::ReadAllText((Join-Path $RepoRoot 'tools/dependencies.json')) | ConvertFrom-Json
    $downloads = Join-Path $RepoRoot '.tools/downloads'
    New-Item -ItemType Directory -Force -Path $downloads | Out-Null
    function Get-PinnedDownload($pin, $name) {
        $file = Join-Path $downloads $name
        if (!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $pin.sha256) {
            Write-Host "Downloading pinned $name"
            $temporary = "$file.download"
            Invoke-WebRequest -Uri $pin.url -OutFile $temporary -UseBasicParsing
            if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $pin.sha256) { throw "Checksum mismatch: $name" }
            Move-Item -LiteralPath $temporary -Destination $file -Force
        }
        return $file
    }
    if (!$Vcpkg -and $paths.Vcpkg -eq (Join-Path $RepoRoot '.tools/vcpkg')) {
        $revision = $null
        if (!(Test-Path -LiteralPath (Join-Path $paths.Vcpkg '.git'))) {
            Invoke-Tool git @('init',$paths.Vcpkg)
            Invoke-Tool git @('-C',$paths.Vcpkg,'remote','add','origin',$pins.vcpkg.repository)
        } else { $revision = & git -C $paths.Vcpkg rev-parse HEAD }
        # Versioned ports need historical baseline/port trees; a depth-one
        # checkout can bootstrap successfully but fail at the first build.
        $shallow = & git -C $paths.Vcpkg rev-parse --is-shallow-repository
        if ($shallow -eq 'true') { Invoke-Tool git @('-C',$paths.Vcpkg,'fetch','--unshallow','origin',$pins.vcpkg.revision) }
        if ($revision -ne $pins.vcpkg.revision) {
            Invoke-Tool git @('-C',$paths.Vcpkg,'fetch','origin',$pins.vcpkg.revision)
            Invoke-Tool git @('-C',$paths.Vcpkg,'checkout','--detach',$pins.vcpkg.revision)
        }
        if (!(Test-Path -LiteralPath (Join-Path $paths.Vcpkg 'vcpkg.exe'))) { Invoke-Tool (Join-Path $paths.Vcpkg 'bootstrap-vcpkg.bat') @('-disableMetrics') }
    }
    if (!$CommonLib -and $paths.CommonLib -eq (Join-Path $RepoRoot '.tools/CommonLibSSE-NG')) {
        $marker = Join-Path $paths.CommonLib '.source-revision'
        if (!(Test-Path -LiteralPath $marker)) {
            if (Test-Path -LiteralPath $paths.CommonLib) { throw 'Unrecognized CommonLib directory. Supply -CommonLib explicitly or move it before setup.' }
            $archive = Get-PinnedDownload $pins.commonlib 'commonlib.tar.gz'
            $stage = Join-Path $downloads ('commonlib-' + [guid]::NewGuid().ToString('N'))
            New-Item -ItemType Directory -Path $stage | Out-Null
            Invoke-Tool tar @('-xzf',$archive,'-C',$stage)
            $extracted = [IO.Path]::GetFullPath((Join-Path $stage $pins.commonlib.folder))
            if (!$extracted.StartsWith($RepoRoot + '\')) { throw 'Unsafe extraction path' }
            Move-Item -LiteralPath $extracted -Destination $paths.CommonLib
            [IO.File]::WriteAllText($marker,$pins.commonlib.revision)
        } elseif ([IO.File]::ReadAllText($marker).Trim() -ne $pins.commonlib.revision) { throw 'CommonLib pin changed. Move the old local directory, then rerun setup.' }
    }
    if (!$NativeOnly) {
        Restore-ModTools
        if (!$Compiler -and $paths.Compiler -eq (Join-Path $RepoRoot '.tools/papyrus/caprica/Caprica.exe')) {
            $archive = Get-PinnedDownload $pins.caprica 'Caprica.v0.3.0.7z'
            $marker = Join-Path (Split-Path $paths.Compiler -Parent) '.compiler-cache.json'
            $valid = $false
            if ((Test-Path -LiteralPath $marker) -and (Test-Path -LiteralPath $paths.Compiler)) {
                $cached = [IO.File]::ReadAllText($marker) | ConvertFrom-Json
                $valid = $cached.archive -eq $pins.caprica.sha256 -and $cached.executable -eq (Get-FileHash -LiteralPath $paths.Compiler -Algorithm SHA256).Hash
            }
            if (!$valid) {
                $extractor = Get-PinnedDownload $pins.sevenzip '7zr.exe'
                Invoke-Tool $extractor @('x',$archive,('-o' + (Split-Path $paths.Compiler -Parent)),'-y')
                [IO.File]::WriteAllText($marker, (@{ archive = $pins.caprica.sha256; executable = (Get-FileHash -LiteralPath $paths.Compiler -Algorithm SHA256).Hash } | ConvertTo-Json))
            }
        }
        if ($VanillaScriptsZip -or !(Test-Path -LiteralPath $paths.Flags)) {
            if (!$VanillaScriptsZip) { $VanillaScriptsZip = Join-Path $RepoRoot '.tools/papyrus/Scripts.zip' }
            if (!(Test-Path -LiteralPath $VanillaScriptsZip)) { throw 'Supply -VanillaScriptsZip with Scripts.zip from your Creation Kit installation, or -VanillaSources.' }
            $stage = Join-Path $downloads ('vanilla-' + [guid]::NewGuid().ToString('N'))
            Expand-Archive -LiteralPath $VanillaScriptsZip -DestinationPath $stage
            $flags = @(Get-ChildItem -LiteralPath $stage -Recurse -Filter TESV_Papyrus_Flags.flg)
            if ($flags.Count -ne 1) { throw 'Scripts.zip must contain one TESV_Papyrus_Flags.flg alongside the vanilla sources.' }
            $paths.Vanilla = Split-Path $flags[0].FullName -Parent
            $paths.Flags = $flags[0].FullName
            $paths.Imports = @((Join-Path $RepoRoot '.tools/papyrus/skse'), $paths.Vanilla)
        }
        $archive = Get-PinnedDownload $pins.skse 'skse64-sources.zip'
        $manifestPath = Join-Path $paths.Imports[0] 'manifest.json'
        $valid = $false
        if (Test-Path -LiteralPath $manifestPath) {
            $manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
            $valid = $manifest.revision -eq $pins.skse.revision -and $manifest.archive_sha256 -eq $pins.skse.sha256
            foreach ($source in $manifest.sources) {
                $output = Join-Path $paths.Imports[0] $source.name
                if (!(Test-Path -LiteralPath $output) -or (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash -ne $source.output_sha256) { $valid = $false; break }
                if ($source.vanilla_source) {
                    $base = Join-Path $paths.Vanilla $source.name
                    if (!(Test-Path -LiteralPath $base) -or (Get-FileHash -LiteralPath $base -Algorithm SHA256).Hash -ne $source.vanilla_sha256) { $valid = $false; break }
                }
            }
        }
        if (!$valid) {
            $prepared = Join-Path $RepoRoot ('.tools/papyrus/skse-' + [guid]::NewGuid().ToString('N'))
            Invoke-ModTools @('prepare','--archive',$archive,'--vanilla',$paths.Vanilla,'--output',$prepared,'--revision',$pins.skse.revision)
            $paths.Imports = @($prepared, $paths.Vanilla)
        } else { Write-Host 'Reusing validated SKSE compiler imports.' }
    }
    foreach ($file in @((Join-Path $paths.CommonLib 'CMakeLists.txt'),(Join-Path $paths.Vcpkg 'vcpkg.exe'))) { if (!(Test-Path -LiteralPath $file)) { throw "Missing dependency: $file" } }
    [IO.File]::WriteAllText((Join-Path $RepoRoot '.tools/local.json'), ($paths | ConvertTo-Json -Depth 5))
    Write-Host 'Setup complete. Run .\build.ps1 or .\build.ps1 -Target Test.'
} catch { Write-Error $_; exit 1 } finally { Pop-Location }
