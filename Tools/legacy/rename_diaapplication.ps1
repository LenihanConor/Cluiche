<#
.SYNOPSIS
    Renames the DiaApplication directory and project to DiaApplicationFlow.

    Changes made:
      - Renames Dia\DiaApplication\           -> Dia\DiaApplicationFlow\
      - Renames DiaApplication.vcxproj        -> DiaApplicationFlow.vcxproj
      - Renames DiaApplication.vcxproj.filters-> DiaApplicationFlow.vcxproj.filters
      - In all .sln / .vcxproj / .vcxproj.filters / .cpp / .h / .md files:
          replaces every occurrence of "DiaApplication" (when NOT followed by
          "Editor" or "Flow") with "DiaApplicationFlow".

    Leaves DiaApplicationEditor and any existing DiaApplicationFlow references
    untouched.
#>

param(
    [string]$Root = "C:\GitHub\Cluiche"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$oldName    = "DiaApplication"
$newName    = "DiaApplicationFlow"
$oldDir     = Join-Path $Root "Dia\$oldName"
$newDir     = Join-Path $Root "Dia\$newName"
$pattern    = 'DiaApplication(?!Editor|Flow)'

# ──────────────────────────────────────────────────────────────────────────────
# Step 1 — rename the directory
# ──────────────────────────────────────────────────────────────────────────────

if (-not (Test-Path $oldDir)) {
    Write-Error "Source directory not found: $oldDir"
    exit 1
}
if (Test-Path $newDir) {
    Write-Error "Target already exists: $newDir  (nothing to do?)"
    exit 1
}

Write-Host "Step 1: Renaming directory..."
Rename-Item -Path $oldDir -NewName $newName
Write-Host "  $oldDir  ->  $newDir"

# ──────────────────────────────────────────────────────────────────────────────
# Step 2 — rename .vcxproj and .vcxproj.filters inside the new directory
# ──────────────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "Step 2: Renaming project files..."

$oldVcx     = Join-Path $newDir "$oldName.vcxproj"
$newVcx     = Join-Path $newDir "$newName.vcxproj"
$oldFilters = Join-Path $newDir "$oldName.vcxproj.filters"
$newFilters = Join-Path $newDir "$newName.vcxproj.filters"

if (Test-Path $oldVcx)     { Rename-Item $oldVcx     $newVcx;     Write-Host "  $oldName.vcxproj         -> $newName.vcxproj" }
if (Test-Path $oldFilters) { Rename-Item $oldFilters $newFilters; Write-Host "  $oldName.vcxproj.filters -> $newName.vcxproj.filters" }

# ──────────────────────────────────────────────────────────────────────────────
# Step 3 — text substitution across the repo
# ──────────────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "Step 3: Replacing text in source files..."

$extensions = @("*.sln","*.vcxproj","*.vcxproj.filters","*.cpp","*.h","*.md","*.txt")

$allFiles = Get-ChildItem -Path $Root -Recurse -Include $extensions -File |
    Where-Object { $_.FullName -notmatch '\\.git\\' }

$changedFiles = @()

foreach ($file in $allFiles) {
    # Read as raw bytes, decode as UTF-8 (preserves BOM if present)
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $enc   = New-Object System.Text.UTF8Encoding($false)   # UTF-8 without BOM
    # Detect BOM
    $hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
    if ($hasBom) {
        $enc = New-Object System.Text.UTF8Encoding($true)  # UTF-8 with BOM
    }

    $text    = $enc.GetString($bytes)
    $newText = [System.Text.RegularExpressions.Regex]::Replace($text, $pattern, $newName)

    if ($newText -ne $text) {
        [System.IO.File]::WriteAllBytes($file.FullName, $enc.GetBytes($newText))
        $changedFiles += $file.FullName
        Write-Host "  Updated: $($file.FullName)"
    }
}

# ──────────────────────────────────────────────────────────────────────────────
# Summary
# ──────────────────────────────────────────────────────────────────────────────

Write-Host ""
Write-Host "Done. $($changedFiles.Count) file(s) updated."
Write-Host ""
Write-Host "Git steps to record the rename:"
Write-Host "  git add Dia/DiaApplicationFlow"
Write-Host "  git rm -r --cached Dia/DiaApplication --ignore-unmatch"
Write-Host "  git status"
