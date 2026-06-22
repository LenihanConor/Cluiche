# Configure PATH and environment variables for Dia CLI
param(
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent | Split-Path -Parent),
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

Write-Host "Configuring environment paths for Dia CLI ..."
& python -m dia_cli env setup --cli-env
if ($LASTEXITCODE -ne 0) {
    Write-Error "Path configuration failed (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
}

Write-Host "Paths configured. Restart your terminal for PATH changes to take effect."
