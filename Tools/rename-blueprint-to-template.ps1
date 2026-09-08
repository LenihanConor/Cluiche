# Renames DiaBlueprintEditor → DiaEntityTemplateEditor and .diaentity → .diaentitytemplate
# Usage: .\rename-blueprint-to-template.ps1           (dry run)
#        .\rename-blueprint-to-template.ps1 --apply   (apply changes)

param([switch]$apply)

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$dryRun = -not $apply

if ($dryRun) { Write-Host "[DRY RUN] Pass --apply to execute changes.`n" -ForegroundColor Yellow }

# ── String substitution rules (order matters — most specific first) ──────────
$replacements = [ordered]@{
    'DiaBlueprintEditorPlugin'   = 'DiaEntityTemplateEditorPlugin'
    'DiaBlueprintEditor'         = 'DiaEntityTemplateEditor'
    'BlueprintEditorPlugin'      = 'EntityTemplateEditorPlugin'
    'BlueprintEditor'            = 'EntityTemplateEditor'
    'blueprint_editor'           = 'entity_template_editor'
    '"diaentity"'                = '"diaentitytemplate"'
    "'diaentity'"                = "'diaentitytemplate'"
    '\.diaentity'                = '.diaentitytemplate'
    'diaentity\b'                = 'diaentitytemplate'
}

# ── File extensions to process ───────────────────────────────────────────────
$textExts = '*.cpp','*.h','*.ts','*.html','*.md','*.json','*.toml','*.vcxproj','*.vcxproj.filters','*.sln','*.diaapp','*.diagame','*.diastage','*.module.md'

# ── Directories to skip ──────────────────────────────────────────────────────
$skipDirs = @('bin','obj','.git','node_modules')

function ShouldSkip($path) {
    foreach ($d in $skipDirs) {
        if ($path -match [regex]::Escape("\$d\")) { return $true }
    }
    return $false
}

# ── 1. String replacements in text files ─────────────────────────────────────
Write-Host "=== STRING REPLACEMENTS ===" -ForegroundColor Cyan
$totalFiles = 0

foreach ($ext in $textExts) {
    Get-ChildItem -Path $repoRoot -Filter $ext -Recurse -ErrorAction SilentlyContinue | Where-Object {
        -not (ShouldSkip $_.FullName)
    } | ForEach-Object {
        $file = $_
        $original = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if (-not $original) { return }

        $updated = $original
        foreach ($kv in $replacements.GetEnumerator()) {
            $updated = $updated -replace $kv.Key, $kv.Value
        }

        if ($updated -ne $original) {
            $totalFiles++
            Write-Host "  $($file.FullName)" -ForegroundColor Gray
            if (-not $dryRun) {
                Set-Content -Path $file.FullName -Value $updated -NoNewline -Encoding UTF8
            }
        }
    }
}
Write-Host "  → $totalFiles file(s) would be updated`n"

# ── 2. Rename .diaentity files ────────────────────────────────────────────────
Write-Host "=== FILE RENAMES (.diaentity → .diaentitytemplate) ===" -ForegroundColor Cyan
Get-ChildItem -Path $repoRoot -Filter '*.diaentity' -Recurse -ErrorAction SilentlyContinue | Where-Object {
    -not (ShouldSkip $_.FullName)
} | ForEach-Object {
    $old = $_.FullName
    $new = $old -replace '\.diaentity$', '.diaentitytemplate'
    Write-Host "  $old`n  → $new" -ForegroundColor Gray
    if (-not $dryRun) { Rename-Item -Path $old -NewName $new }
}
Write-Host ""

# ── 3. Rename DiaBlueprintEditor directory via git mv ────────────────────────
Write-Host "=== DIRECTORY RENAME ===" -ForegroundColor Cyan
$oldDir = Join-Path $repoRoot 'Dia\DiaBlueprintEditor'
$newDir = Join-Path $repoRoot 'Dia\DiaEntityTemplateEditor'

if (Test-Path $oldDir) {
    Write-Host "  git mv Dia/DiaBlueprintEditor Dia/DiaEntityTemplateEditor" -ForegroundColor Gray
    if (-not $dryRun) {
        Push-Location $repoRoot
        git mv "Dia/DiaBlueprintEditor" "Dia/DiaEntityTemplateEditor"
        Pop-Location
    }
} else {
    Write-Host "  (already renamed or not found: $oldDir)" -ForegroundColor DarkGray
}
Write-Host ""

# ── 4. Rename test directory ──────────────────────────────────────────────────
Write-Host "=== TEST DIRECTORY RENAME ===" -ForegroundColor Cyan
$oldTest = Join-Path $repoRoot 'Cluiche\Tests\GoogleTests\DiaBlueprintEditor'
$newTest = Join-Path $repoRoot 'Cluiche\Tests\GoogleTests\DiaEntityTemplateEditor'

if (Test-Path $oldTest) {
    Write-Host "  git mv Cluiche/Tests/GoogleTests/DiaBlueprintEditor Cluiche/Tests/GoogleTests/DiaEntityTemplateEditor" -ForegroundColor Gray
    if (-not $dryRun) {
        Push-Location $repoRoot
        git mv "Cluiche/Tests/GoogleTests/DiaBlueprintEditor" "Cluiche/Tests/GoogleTests/DiaEntityTemplateEditor"
        Pop-Location
    }
} else {
    Write-Host "  (already renamed or not found: $oldTest)" -ForegroundColor DarkGray
}
Write-Host ""

if ($dryRun) {
    Write-Host "Re-run with --apply to execute all changes." -ForegroundColor Yellow
} else {
    Write-Host "Done. Review with: git diff --stat" -ForegroundColor Green
}
