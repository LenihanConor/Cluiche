# Restore external dependencies declared in deps.json
param(
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent | Split-Path -Parent),
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$depsJson = Join-Path $RepoRoot "deps.json"
if (-not (Test-Path $depsJson)) {
    Write-Error "deps.json not found at $depsJson"
    exit 1
}

Write-Host "Restoring dependencies from $depsJson ..."
& python -m dia_cli env setup --deps
if ($LASTEXITCODE -ne 0) {
    Write-Error "Dependency restore failed (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
}

Write-Host "Dependencies restored."
