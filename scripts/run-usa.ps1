param(
    [ValidateSet('Default','ROV')][string]$RenderingPath = 'Default',
    [ValidateSet('Controller','KeyboardMouse')][string]$InputProfile = 'Controller',
    [ValidateSet('Original','60')][string]$FrameRate = '60',
    [ValidateSet('720p','1080p','1440p','4K')][string]$Resolution = '1080p',
    [ValidateSet('1x','2x','3x')][string]$RenderScale = '1x',
    [switch]$Fxaa,
    [switch]$Fullscreen,
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$binDir = Join-Path $root "prototype-usa\out\build\win-amd64-$($Configuration.ToLower())"
$exe = Join-Path $binDir 'majin.exe'

if (-not (Test-Path $exe)) {
    throw "Executable not found at $exe. Run scripts/build-usa.ps1 first."
}

$gameData = Join-Path $root 'game-data-usa'
if (-not (Test-Path (Join-Path $gameData 'default.xex'))) {
    throw "Game data not found in $gameData. Run scripts/extract-iso.ps1 first."
}

# Resolve backend path and cache root
$backendPath = if ($RenderingPath -eq 'ROV') { 'rov' } else { '' }
$cacheRoot = Join-Path $root "private\runtime-usa\cache\d3d12-$($RenderingPath.ToLower())"
$userData = Join-Path $root 'private\runtime-usa\user'
$reportsDir = Join-Path $root 'reports'
New-Item -ItemType Directory -Force -Path $cacheRoot, $userData, $reportsDir | Out-Null

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logFile = Join-Path $reportsDir "runtime-usa-$timestamp.log"

$appArgs = @(
    "--game_data_root=$gameData",
    "--cache_root=$cacheRoot",
    "--user_data_root=$userData",
    "--log_file=$logFile",
    "--gpu_allow_invalid_fetch_constants=true",
    "--d3d12_readback_resolve=false",
    "--vsync=false"
)

if ($backendPath) {
    $appArgs += "--render_target_path_d3d12=$backendPath"
}

# Resolution scaling
$scale = switch ($RenderScale) {
    '1x' { 1 }
    '2x' { 2 }
    '3x' { 3 }
}
$appArgs += "--draw_resolution_scale_x=$scale"
$appArgs += "--draw_resolution_scale_y=$scale"

if ($Fxaa) {
    $appArgs += "--swap_post_effect=fxaa"
}

if ($FrameRate -eq '60') {
    $appArgs += "--target_fps=60"
}

# Window size
$res = switch ($Resolution) {
    '720p'  { @{ W = 1280; H = 720 } }
    '1080p' { @{ W = 1920; H = 1080 } }
    '1440p' { @{ W = 2560; H = 1440 } }
    '4K'    { @{ W = 3840; H = 2160 } }
}
$appArgs += "--window_width=$($res.W)"
$appArgs += "--window_height=$($res.H)"

if ($Fullscreen) {
    $appArgs += "--fullscreen=true"
}

Write-Host "Launching Majin and the Forsaken Kingdom (USA)..."
Write-Host "  Executable   : $exe"
Write-Host "  Backend      : $RenderingPath"
Write-Host "  Resolution   : $Resolution (Internal Scale: $RenderScale)"
Write-Host "  Framerate    : $FrameRate FPS"
Write-Host "  Input Profile: $InputProfile"
Write-Host "  Log File     : $logFile"

Push-Location $binDir
try {
    & $exe @appArgs
} finally {
    Pop-Location
}
