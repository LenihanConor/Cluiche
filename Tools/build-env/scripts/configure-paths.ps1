# Verify and report PATH configuration inside the container
param([string]$RepoRoot = "C:\repo")
$paths = @(
    "C:\BuildTools\MSBuild\Current\Bin",
    "C:\Program Files\Python311",
    "C:\Program Files\Git\cmd"
)
$envPath = $env:PATH -split ";"
foreach ($p in $paths) {
    if ($envPath -contains $p) {
        Write-Host "OK: $p"
    } else {
        Write-Warning "MISSING: $p"
    }
}
