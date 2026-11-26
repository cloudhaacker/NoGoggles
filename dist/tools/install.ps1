# NoGoggles installer: copies the driver into your user folder and tells SteamVR about it.
$ErrorActionPreference = 'Stop'
$version = '0.6'

# If anything goes wrong, show it and wait, instead of the window vanishing.
trap { Write-Host ""; Write-Host "PROBLEM: $_" -ForegroundColor Red; [void](Read-Host 'Press Enter to close'); exit 1 }

function Say($msg, $color = 'Gray') { Write-Host $msg -ForegroundColor $color }
function Fail($msg) { Say ""; Say "PROBLEM: $msg" 'Red'; [void](Read-Host 'Press Enter to close'); exit 1 }

Say "=== NoGoggles installer ===" 'Cyan'

# 1. SteamVR must be closed, or it will overwrite our changes when it exits.
while (Get-Process -Name vrserver, vrmonitor, vrcompositor -ErrorAction SilentlyContinue) {
    Say "SteamVR is running. Please close SteamVR, then press Enter." 'Yellow'
    [void](Read-Host)
}

# 2. Find Steam.
$steam = $null
try { $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction Stop).SteamPath } catch {}
if (-not $steam) { Fail "Couldn't find Steam. Is it installed?" }
$steam = $steam -replace '/', '\'
Say "Steam found at $steam"

# 3. SteamVR's list of drivers. It only exists after SteamVR has been started once.
$vrpathFile = Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath'
if (-not (Test-Path $vrpathFile)) {
    Fail ("SteamVR hasn't been set up yet.`n" +
          "  1) Install SteamVR from Steam (it's free; search 'SteamVR' in the Store).`n" +
          "  2) Start it once. It will say no headset was found - that's fine. Close it.`n" +
          "  3) Run this installer again.")
}

# 4. Copy the driver to a permanent home, keeping your settings if you're updating.
$src = Join-Path (Split-Path $PSScriptRoot -Parent) 'nogoggles'
$dest = Join-Path $env:LOCALAPPDATA 'NoGoggles\nogoggles'
$ini = Join-Path $dest 'NoGoggles.ini'
$keepIni = $null
if (Test-Path $ini) { $keepIni = Get-Content $ini -Raw }
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item -Path (Join-Path $src '*') -Destination $dest -Recurse -Force
if ($keepIni) {
    # Put your old values into the new settings file, so new options appear
    # but nothing you changed is lost.
    # Read the old file: values by [section] + key, and the raw lines of each section.
    $old = @{}; $oldLines = [ordered]@{}; $sec = ''
    foreach ($line in ($keepIni -split "`r?`n")) {
        if ($line -match '^\s*\[(.+)\]\s*$') { $sec = $matches[1]; $oldLines[$sec] = New-Object System.Collections.ArrayList }
        elseif ($sec -and $line -match '^\s*([A-Za-z0-9]+)\s*=\s*(.*)$') { $old["$sec|$($matches[1])"] = $matches[2] }
        if ($sec) { [void]$oldLines[$sec].Add($line) }
    }
    # Walk the new file, swapping in your old values.
    $newSections = @{}; $sec = ''
    $merged = New-Object System.Collections.ArrayList
    foreach ($line in (Get-Content $ini)) {
        if ($line -match '^\s*\[(.+)\]\s*$') { $sec = $matches[1]; $newSections[$sec] = $true }
        if ($sec -and $line -match '^(\s*)([A-Za-z0-9]+)(\s*=\s*)(.*)$' -and $old.ContainsKey("$sec|$($matches[2])")) {
            [void]$merged.Add($matches[1] + $matches[2] + $matches[3] + $old["$sec|$($matches[2])"])
        } else { [void]$merged.Add($line) }
    }
    # Keep any sections you added yourself (like your own [Game:...] sections).
    foreach ($s in $oldLines.Keys) {
        if (-not $newSections.ContainsKey($s)) { [void]$merged.Add(''); foreach ($l in $oldLines[$s]) { [void]$merged.Add($l) } }
    }
    [System.IO.File]::WriteAllText($ini, (($merged -join "`r`n") + "`r`n"))
    Say "Kept your existing settings and added any new ones."
}
Get-ChildItem $dest -Recurse -File | Unblock-File -ErrorAction SilentlyContinue
Say "Driver copied to $dest"

# 5. Register the driver with SteamVR.
$paths = Get-Content $vrpathFile -Raw | ConvertFrom-Json
$list = @()
if ($paths.PSObject.Properties.Name -contains 'external_drivers' -and $paths.external_drivers) {
    $list = @($paths.external_drivers)
}
if (-not ($list | Where-Object { $_ -ieq $dest })) { $list += $dest }
$paths | Add-Member -NotePropertyName external_drivers -NotePropertyValue ([object[]]$list) -Force
Copy-Item $vrpathFile "$vrpathFile.nogoggles-backup" -Force
[System.IO.File]::WriteAllText($vrpathFile, ($paths | ConvertTo-Json -Depth 20))
Say "Registered with SteamVR."

# 6. A couple of SteamVR settings that make a fake headset behave.
$cfgDir = Join-Path $steam 'config'
$cfgFile = Join-Path $cfgDir 'steamvr.vrsettings'
if (Test-Path $cfgFile) {
    Copy-Item $cfgFile "$cfgFile.nogoggles-backup" -Force
    $cfg = Get-Content $cfgFile -Raw | ConvertFrom-Json
} else {
    New-Item -ItemType Directory -Force -Path $cfgDir | Out-Null
    $cfg = [pscustomobject]@{}
}
function Set-VRSetting($obj, $section, $key, $value) {
    if (-not ($obj.PSObject.Properties.Name -contains $section)) {
        $obj | Add-Member -NotePropertyName $section -NotePropertyValue ([pscustomobject]@{})
    }
    $obj.$section | Add-Member -NotePropertyName $key -NotePropertyValue $value -Force
}
Set-VRSetting $cfg 'steamvr' 'enableHomeApp' $false            # skip SteamVR Home (big 3D lobby)
Set-VRSetting $cfg 'power' 'pauseCompositorOnStandby' $false   # don't freeze when the "headset" sits still
Set-VRSetting $cfg 'dashboard' 'enableDashboard' $false        # no SteamVR menu covering the game (no VR hands to close it)
[System.IO.File]::WriteAllText($cfgFile, ($cfg | ConvertTo-Json -Depth 20))
Say "SteamVR settings updated (backup saved next to it)."

# 7. A ready-made "room setup" for the fake headset, so SteamVR treats it as calibrated.
$universeId = '4206942069'
$chapFile = Join-Path $cfgDir 'chaperone_info.vrchap'
if (Test-Path $chapFile) {
    Copy-Item $chapFile "$chapFile.nogoggles-backup" -Force
    $chap = Get-Content $chapFile -Raw | ConvertFrom-Json
} else {
    $chap = [pscustomobject]@{ jsonid = 'chaperone_info'; universes = @(); version = 5 }
}
$universes = @()
if ($chap.universes) { $universes = @($chap.universes | Where-Object { "$($_.universeID)" -ne $universeId }) }
$universes += [pscustomobject]@{
    collision_bounds = @()
    play_area = @(2.0, 2.0)
    seated = [pscustomobject]@{ translation = @(0.0, 0.0, 0.0); yaw = 0.0 }
    standing = [pscustomobject]@{ translation = @(0.0, 0.0, 0.0); yaw = 0.0 }
    universeID = $universeId
}
$chap | Add-Member -NotePropertyName universes -NotePropertyValue ([object[]]$universes) -Force
[System.IO.File]::WriteAllText($chapFile, ($chap | ConvertTo-Json -Depth 20))
Say "Room setup for the fake headset written."

# 8. Desktop shortcut to the settings file.
try {
    $desktop = [Environment]::GetFolderPath('Desktop')
    $sc = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $desktop 'NoGoggles Settings.lnk'))
    $sc.TargetPath = $ini
    $sc.Save()
    Say "Added a 'NoGoggles Settings' shortcut to your desktop."
} catch { Say "(Couldn't make a desktop shortcut; settings live at $ini)" }

# 9. Show up in Windows Settings > Apps, with a working Uninstall button.
$ngHome = Join-Path $env:LOCALAPPDATA 'NoGoggles'
Copy-Item (Join-Path $PSScriptRoot 'uninstall.ps1') (Join-Path $ngHome 'uninstall.ps1') -Force
$key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\NoGoggles'
New-Item -Path $key -Force | Out-Null
$uninstallCmd = 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $ngHome 'uninstall.ps1') + '"'
$props = @{
    DisplayName     = 'NoGoggles (SteamVR without a headset)'
    DisplayVersion  = $version
    Publisher       = 'cloudhaacker'
    InstallLocation = $ngHome
    UninstallString = $uninstallCmd
    URLInfoAbout    = 'https://github.com/cloudhaacker/NoGoggles'
    DisplayIcon     = 'powershell.exe'
}
foreach ($k in $props.Keys) { Set-ItemProperty -Path $key -Name $k -Value $props[$k] }
Set-ItemProperty -Path $key -Name NoModify -Value 1 -Type DWord
Set-ItemProperty -Path $key -Name NoRepair -Value 1 -Type DWord
Say "Added to Windows Settings > Apps (uninstall from there any time)."

Say ""
Say "All done!" 'Green'
Say "Start Psychonauts in the Rhombus of Ruin from Steam. SteamVR will open with a"
Say "NoGoggles window showing the game. Press F8 for mouse look, F9 to recenter."
Say "You can delete the download folder now - NoGoggles lives in your user folder."
Say ""
[void](Read-Host 'Press Enter to close')
