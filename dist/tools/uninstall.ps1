# NoGoggles uninstaller: unregisters the driver and removes its files.
$ErrorActionPreference = 'Stop'

# If anything goes wrong, show it and wait, instead of the window vanishing.
trap { Write-Host ""; Write-Host "PROBLEM: $_" -ForegroundColor Red; [void](Read-Host 'Press Enter to close'); exit 1 }
function Say($msg, $color = 'Gray') { Write-Host $msg -ForegroundColor $color }

Say "=== NoGoggles uninstaller ===" 'Cyan'
while (Get-Process -Name vrserver, vrmonitor, vrcompositor -ErrorAction SilentlyContinue) {
    Say "SteamVR is running. Please close SteamVR, then press Enter." 'Yellow'
    [void](Read-Host)
}

$dest = Join-Path $env:LOCALAPPDATA 'NoGoggles\nogoggles'

$vrpathFile = Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath'
if (Test-Path $vrpathFile) {
    $paths = Get-Content $vrpathFile -Raw | ConvertFrom-Json
    if ($paths.PSObject.Properties.Name -contains 'external_drivers' -and $paths.external_drivers) {
        $list = @($paths.external_drivers | Where-Object { $_ -notlike '*\NoGoggles\nogoggles' })
        $paths | Add-Member -NotePropertyName external_drivers -NotePropertyValue ([object[]]$list) -Force
        [System.IO.File]::WriteAllText($vrpathFile, ($paths | ConvertTo-Json -Depth 20))
    }
    Say "Removed from SteamVR's driver list."
}

$steam = $null
try { $steam = ((Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamPath) -replace '/', '\' } catch {}
if ($steam) {
    $cfgFile = Join-Path $steam 'config\steamvr.vrsettings'
    if (Test-Path $cfgFile) {
        $cfg = Get-Content $cfgFile -Raw | ConvertFrom-Json
        if ($cfg.steamvr) { $cfg.steamvr.PSObject.Properties.Remove('enableHomeApp') }
        if ($cfg.power) { $cfg.power.PSObject.Properties.Remove('pauseCompositorOnStandby') }
        if ($cfg.dashboard) { $cfg.dashboard.PSObject.Properties.Remove('enableDashboard') }
        [System.IO.File]::WriteAllText($cfgFile, ($cfg | ConvertTo-Json -Depth 20))
        Say "SteamVR settings put back to normal."
    }
}

if ($steam) {
    $chapFile = Join-Path $steam 'config\chaperone_info.vrchap'
    if (Test-Path $chapFile) {
        $chap = Get-Content $chapFile -Raw | ConvertFrom-Json
        if ($chap.universes) {
            $left = @($chap.universes | Where-Object { "$($_.universeID)" -ne '4206942069' })
            $chap | Add-Member -NotePropertyName universes -NotePropertyValue ([object[]]$left) -Force
            [System.IO.File]::WriteAllText($chapFile, ($chap | ConvertTo-Json -Depth 20))
        }
    }
}

if (Test-Path $dest) { Remove-Item (Split-Path $dest -Parent) -Recurse -Force }
$lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'NoGoggles Settings.lnk'
if (Test-Path $lnk) { Remove-Item $lnk -Force }
$key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NoGoggles'
if (Test-Path $key) { Remove-Item $key -Recurse -Force }

Say ""
Say "NoGoggles is uninstalled." 'Green'
Say ""
[void](Read-Host 'Press Enter to close')
