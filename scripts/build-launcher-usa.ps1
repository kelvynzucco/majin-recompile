$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

New-Item -ItemType Directory -Force -Path (Join-Path $root 'assets\launcher') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root 'launcher') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $root 'private\build\launcher') | Out-Null

. (Join-Path $PSScriptRoot 'vs-env.ps1')

$compiler = Join-Path $llvmBin 'clang-cl.exe'
$linker = Join-Path $llvmBin 'lld-link.exe'
$source = Join-Path $root 'launcher'
$iconRoot = Join-Path $root 'assets\launcher'
$output = Join-Path $root 'private\build\launcher'
$exe = Join-Path $root 'Majin FK Launcher.exe'

Push-Location $source
try {
    Write-Output '[1/3] Compiling icon resources and application manifest (rc.exe)...'
    & rc.exe /nologo /i $iconRoot /fo (Join-Path $output 'launcher.res') 'resource.rc'
    if ($LASTEXITCODE -ne 0) { throw "rc.exe failed ($LASTEXITCODE)" }

    Write-Output '[2/3] Compiling main.cpp (clang-cl.exe)...'
    $object = Join-Path $output 'launcher.obj'
    & $compiler /nologo /c /O2 /MT /DNDEBUG /std:c++17 /EHsc /Brepro "/Fo$object" 'main.cpp'
    if ($LASTEXITCODE -ne 0) { throw "clang-cl.exe failed ($LASTEXITCODE)" }

    Write-Output '[3/3] Linking Majin FK Launcher.exe (lld-link.exe)...'
    & $linker "/OUT:$exe" /SUBSYSTEM:WINDOWS /DEBUG:NONE /INCREMENTAL:NO /Brepro $object (Join-Path $output 'launcher.res') user32.lib shell32.lib
    if ($LASTEXITCODE -ne 0) { throw "lld-link.exe failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}

Write-Output "Validating Majin FK Launcher.exe (--validate-only)..."
$proc = Start-Process -FilePath $exe -ArgumentList '--validate-only' -WorkingDirectory $root -PassThru -Wait
if ($proc.ExitCode -ne 0) {
    throw "Majin FK Launcher.exe validation failed with exit code $($proc.ExitCode)"
}
Write-Output "SUCCESS: Majin FK Launcher.exe built and validated at: $exe"
