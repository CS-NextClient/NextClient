# Compiles the studio render test maps and installs them next to the game's other maps.
#
#   powershell -File tools/studio_tests/build_maps.ps1 [-GameDir "D:\...\cstrike"] [-Maps studio_flags,studio_perf]
#
# vis runs in full: the whole point of the corridor is that the two rooms end up in
# separate PVS, and -fast produces a solution coarse enough to lose that. rad stays on
# -fast, since a good light solve on a lit box buys nothing while costing minutes.

param(
    [string]$GameDir = "D:\Counter Strike 1.6 steam next client\cstrike",
    [string[]]$Maps = @("studio_test")
)

$tools = Join-Path (Split-Path $GameDir -Parent) "tools"
$src = Join-Path $GameDir "maps\src"

foreach ($exe in @("hlcsg.exe", "hlbsp.exe", "hlvis.exe", "hlrad.exe")) {
    if (-not (Test-Path (Join-Path $tools $exe))) {
        throw "missing compiler: $exe in $tools"
    }
}

foreach ($map in $Maps) {
    $path = Join-Path $src $map

    if (-not (Test-Path "$path.map")) {
        Write-Host "skipping $map, no source" -ForegroundColor Yellow
        continue
    }

    Write-Host "=== $map ===" -ForegroundColor Cyan

    # -wadinclude embeds the textures, so the finished bsp needs no wad on the client.
    & (Join-Path $tools "hlcsg.exe") -nowadtextures -wadinclude halflife -wadinclude studio_test "$path.map" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "hlcsg failed for $map" }

    & (Join-Path $tools "hlbsp.exe") "$path.map" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "hlbsp failed for $map" }

    & (Join-Path $tools "hlvis.exe") "$path.map" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "hlvis failed for $map" }

    & (Join-Path $tools "hlrad.exe") -fast "$path.map" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "hlrad failed for $map" }

    Copy-Item "$path.bsp" (Join-Path $GameDir "maps\$map.bsp") -Force
    $size = (Get-Item (Join-Path $GameDir "maps\$map.bsp")).Length
    Write-Host ("installed {0}.bsp ({1:N0} bytes)" -f $map, $size) -ForegroundColor Green
}
