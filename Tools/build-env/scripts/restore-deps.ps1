# Restore External/ deps from deps.json inside the container
param(
    [string]$RepoRoot = "C:\repo",
    [switch]$Force
)
Set-Location $RepoRoot
$args = @("env", "deps")
if ($Force) { $args += "--force" }
python -m dia_cli @args
