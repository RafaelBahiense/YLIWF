param([Parameter(Mandatory)] [string] $BuildDirectory, [Parameter(Mandatory)] [string] $Executable)
$ErrorActionPreference = 'Stop'
$repoDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$statusFile = Join-Path $BuildDirectory 'ui-status.txt'
$logFile = Join-Path $BuildDirectory 'hot-reload.log'
$hostFiles = @('Main.cpp', 'ReloadableUI.cpp', 'ReloadableUI.h', 'UIAPI.h', 'Session.h', 'CMakeLists.txt', 'vcpkg.json')

function Get-PreviewSourceStamp {
    $files = @(
        Get-ChildItem -LiteralPath (Join-Path $repoDirectory 'src/native/ui') -Recurse -File
        Get-ChildItem -LiteralPath (Join-Path $repoDirectory 'src/native') -Recurse -File |
            Where-Object { $_.Extension -in @('.h', '.hpp') }
        Get-ChildItem -LiteralPath (Join-Path $repoDirectory 'include') -Recurse -File |
            Where-Object { $_.Extension -in @('.h', '.hpp') }
        foreach ($manifest in Get-ChildItem -LiteralPath (Join-Path $repoDirectory 'src/addons') -Recurse -File -Filter addon.json) {
            $previewDirectory = Join-Path $manifest.DirectoryName 'preview'
            if (Test-Path -LiteralPath $previewDirectory) {
                $manifest
                Get-ChildItem -LiteralPath $previewDirectory -Recurse -File
                Get-ChildItem -LiteralPath (Join-Path $manifest.DirectoryName 'native') -Recurse -File |
                    Where-Object { $_.Extension -in @('.h', '.hpp') -or $_.Name -eq 'Menu.cpp' }
            }
        }
        Get-ChildItem -LiteralPath $PSScriptRoot -Recurse -File |
            Where-Object { $_.Extension -in @('.cpp', '.h', '.hpp', '.cmake', '.json') -or $_.Name -eq 'CMakeLists.txt' }
    )
    $theme = Select-String -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Pattern '^PREVIEW_THEME_FILE:FILEPATH=(.+)$'
    if ($theme -and (Test-Path -LiteralPath $theme.Matches[0].Groups[1].Value)) {
        $files += Get-Item -LiteralPath $theme.Matches[0].Groups[1].Value
    }
    return (($files | Sort-Object FullName -Unique | ForEach-Object {
        "$($_.FullName)|$($_.Length)|$($_.LastWriteTimeUtc.Ticks)"
    }) -join "`n")
}

function Get-HostStamp {
    return (($hostFiles | ForEach-Object {
        $item = Get-Item -LiteralPath (Join-Path $PSScriptRoot $_)
        "$($item.Length)|$($item.LastWriteTimeUtc.Ticks)"
    }) -join '|')
}

'Watching UI sources' | Set-Content -LiteralPath $statusFile -Encoding utf8NoBOM
$sourceStamp = Get-PreviewSourceStamp
$hostStamp = Get-HostStamp
$previewProcess = Start-Process -FilePath $Executable -WorkingDirectory $BuildDirectory -PassThru -WindowStyle Normal
Write-Host 'UI hot reload enabled. Save menu sources to rebuild. Close the preview to stop.'
try {
    while (-not $previewProcess.HasExited) {
        Start-Sleep -Milliseconds 400
        $nextStamp = Get-PreviewSourceStamp
        if ($nextStamp -eq $sourceStamp) { continue }
        # Wait for editors to finish their save/rename sequence.
        Start-Sleep -Milliseconds 400
        $sourceStamp = Get-PreviewSourceStamp
        if ((Get-HostStamp) -ne $hostStamp) {
            'Preview host changed; close the window and rerun build.ps1.' | Set-Content -LiteralPath $statusFile -Encoding utf8NoBOM
            Write-Host 'Preview host changed; restart is required.'
            continue
        }
        'Building UI; current menu remains active...' | Set-Content -LiteralPath $statusFile -Encoding utf8NoBOM
        try {
            & cmake --build $BuildDirectory --target yliwf-preview-ui --parallel 2 *> $logFile
            $succeeded = $LASTEXITCODE -eq 0
        } catch {
            $_ | Out-String | Add-Content -LiteralPath $logFile
            $succeeded = $false
        }
        if ($succeeded) {
            'Build succeeded; reloading UI...' | Set-Content -LiteralPath $statusFile -Encoding utf8NoBOM
            Write-Host 'UI rebuilt.'
        } else {
            'UI build failed; previous menu retained. See hot-reload.log.' | Set-Content -LiteralPath $statusFile -Encoding utf8NoBOM
            Write-Host "UI build failed. Details: $logFile"
        }
    }
} finally {
    $previewProcess.Dispose()
}
