#Requires -Version 5.1

param(
    [string]$Target = "amaryl-editor.exe"
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$RadDbgPath   = "C:\Applications\raddebugger\build\raddbg.exe"
$WorkspaceDir = Join-Path $PSScriptRoot "out\clang-x64-debug"

if (-not (Test-Path $RadDbgPath)) {
    Write-Host "[ERROR] raddbg.exe was not found: $RadDbgPath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path (Join-Path $WorkspaceDir $Target))) {
    Write-Host "[ERROR] Target was not found: $(Join-Path $WorkspaceDir $Target)" -ForegroundColor Red
    Write-Host "Build first with: .\build.ps1 debug"
    exit 1
}

# raddbg uses the current directory as the target's working directory.
Push-Location $WorkspaceDir
try {
    & $RadDbgPath --project ".\$Target" --auto_run -q
} finally {
    Pop-Location
}
