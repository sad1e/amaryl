#Requires -Version 5.1

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

# ============================================================
# Parse arguments
# ============================================================

$BuildConfig  = "debug"
$DoReconfig   = $false

foreach ($arg in $args) {
    switch -Regex ($arg.ToLowerInvariant()) {
        "^debug$" {
            $BuildConfig = "debug"
        }
        "^release$" {
            $BuildConfig = "release"
        }
        "^reconfig$" {
            $DoReconfig = $true
        }
        default {
            Write-Host "[ERROR] Unknown argument: $arg" -ForegroundColor Red
            Write-Host "Usage:"
            Write-Host "  .\build.ps1 [debug|release] [reconfig]"
            exit 1
        }
    }
}

# Configure and build presets share the same name in CMakePresets.json.
$ConfigurePreset = "clang-x64-$BuildConfig"
$BuildPreset     = "clang-x64-$BuildConfig"


# ============================================================
# Verify required tools
# ============================================================

function Test-Command([string]$Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

if (-not (Test-Command "cmake")) {
    Write-Host "[ERROR] cmake.exe was not found in PATH." -ForegroundColor Red
    exit 1
}

if (-not (Test-Command "ninja")) {
    Write-Host "[ERROR] ninja.exe was not found in PATH." -ForegroundColor Red
    exit 1
}

# clang is located and version-checked (LLVM 23.1.5) by
# cmakes/toolchain-clang-x64.cmake; report what is on PATH for reference.
$clangxx = Get-Command "clang++" -ErrorAction SilentlyContinue
if ($clangxx) {
    Write-Host "[INFO] clang++:"
    Write-Host "       $($clangxx.Source)"
} else {
    Write-Host "[INFO] clang++ was not found in PATH (the toolchain will also search Scoop)."
}

Write-Host "[INFO] Configure preset: $ConfigurePreset"
Write-Host "[INFO] Build preset:     $BuildPreset"


# ============================================================
# Configure
# ============================================================

if ($DoReconfig) {
    Write-Host "[INFO] Performing fresh configuration..."
    & cmake --fresh --preset $ConfigurePreset
} else {
    & cmake --preset $ConfigurePreset
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed." -ForegroundColor Red
    exit $LASTEXITCODE
}


# ============================================================
# Build
# ============================================================

& cmake --build --preset $BuildPreset

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "[SUCCESS] Build completed successfully." -ForegroundColor Green
exit 0
