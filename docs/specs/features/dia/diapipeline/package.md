# Feature Spec: package

## Parent System
@docs/specs/systems/dia/diapipeline.md

## Status
`Done` — Stage renamed from `package` to `deploy` as part of pipeline consolidation. The implementation is unchanged; only the stage name in `pipeline.toml` and the CLI surface changed.

## Summary

Implement `dia pipeline --stage deploy` (formerly `package`) — copies runtime dependencies (DLLs, data files, manifests) alongside each built executable. File copy rules are defined per-target in `pipeline.toml`; `$(OutDir)` and `$(Configuration)` variables are resolved at runtime. This stage is the eventual replacement for `xcopy` post-build events scattered across `.vcxproj` files, but coexists with them until an explicit migration sprint (SD-PIPE-006).

## Problem

Each executable (`GoogleTests.exe`, `CluicheTest.exe`, `CluicheEditor.exe`) requires specific runtime files (DLLs, data files, manifests) to be present in the output directory before it can run. Currently these copies are performed by `xcopy` commands in `.vcxproj` post-build events — scattered, inconsistent, and invisible to the CLI pipeline. The `package` stage centralises all copy rules in `pipeline.toml` and executes them reliably from the CLI or Docker.

## Goals

- Read per-target file copy rules from `pipeline.toml [targets.<name>.package]`
- Resolve `$(OutDir)` and `$(Configuration)` in both `src` and `dest` paths
- Support glob patterns in `src` (e.g., `External/SFML/Current-x64/bin/*.dll`)
- Support recursive glob in `src` (e.g., `External/CEF/Resources/locales/**`)
- Copy files only if source is newer than destination (matches `xcopy /D` behaviour)
- Skip-if-staged guard: if all destination files already exist and are up to date, skip with log
- `--force` bypasses skip-if-staged and re-copies everything
- Create destination directories as needed

## Non-Goals

- Removing `xcopy` post-build events from `.vcxproj` files (SD-PIPE-006: coexistence is acceptable)
- Deployment to remote targets — local staging only
- Code signing or notarisation
- Cleaning stale files from `OutDir` (copy-only; a future `--clean-package` flag can add this)

## File Copy Rules (from `pipeline.toml`)

Extracted from actual `xcopy` commands in the `.vcxproj` files:

**googletest:**
```toml
[targets.googletest.package]
files = [
  { src = "External/Python311/python311.dll", dest = "$(OutDir)" },
  { src = "Cluiche/CluicheTest/Data/Manifests/*.diaapp", dest = "$(OutDir)Data/Manifests/" }
]
```

**cluichetest:**
```toml
[targets.cluichetest.package]
files = [
  { src = "Cluiche/pathStoreConfig.json", dest = "$(OutDir)" },
  { src = "External/SFML/Current-x64/bin/*.dll", dest = "$(OutDir)" },
  { src = "Cluiche/CluicheTest/Data/Manifests/*.diaapp", dest = "$(OutDir)Data/Manifests/" }
]
```

**cluicheeditor:**
```toml
[targets.cluicheeditor.package]
files = [
  { src = "External/CEF/Resources/*", dest = "$(OutDir)" },
  { src = "External/CEF/Resources/locales/**", dest = "$(OutDir)locales/" },
  { src = "External/CEF/bin/$(Configuration)/*", dest = "$(OutDir)" }
]
```

## Copy Logic

```python
import glob, shutil
from pathlib import Path

def copy_rule(src_pattern: str, dest_pattern: str, repo_root: Path, config: str, platform: str):
    resolved_src = resolve_variables(src_pattern, config, platform, repo_root)
    resolved_dest = resolve_variables(dest_pattern, config, platform, repo_root)

    matched = glob.glob(str(repo_root / resolved_src), recursive=True)
    if not matched:
        logger.warning(f"package: no files matched {resolved_src}")
        return

    dest_dir = Path(resolved_dest)
    dest_dir.mkdir(parents=True, exist_ok=True)

    for src_file in matched:
        src_path = Path(src_file)
        if src_path.is_dir():
            continue  # glob ** may yield directories; skip them
        dest_file = dest_dir / src_path.name
        if not dest_file.exists() or src_path.stat().st_mtime > dest_file.stat().st_mtime:
            shutil.copy2(src_path, dest_file)
            logger.info(f"  copied {src_path.name} → {dest_file}")
        else:
            logger.debug(f"  skip (up to date) {src_path.name}")
```

## Skip-if-Staged Guard

Before running any copy rules, the stage checks whether all destination files for the target are already up to date. If all are current and `--force` is not set, logs "package: already staged (use --force to re-copy)" and exits 0.

Implementation: iterate all copy rules, resolve globs, check `src.st_mtime > dest.st_mtime` for each pair. If any file is stale or missing, proceed with the full copy. This is a best-effort check — it does not need to be atomic.

## Implementation

### Files introduced

```
Dia/DiaCLI/
└── dia_cli/
    └── commands/
        └── pipeline/
            └── stages/
                └── package_stage.py   # Stage handler
```

### `package_stage.py` responsibilities

```python
def run(config: PipelineConfig, target: str, build_config: str, force: bool, repo_root: Path) -> int:
    """Returns 0 on success/skip, 1 on failure."""
```

1. Retrieve `config.targets[target].package.files`
2. If not `--force`: run skip-if-staged check; if all staged, log and return 0
3. For each file rule: resolve variables, expand glob, copy newer files, create dest dirs
4. Warn (not fail) on unmatched globs
5. Return 0 on completion; return 1 only on unexpected exceptions (e.g., permission error)

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `pipeline-config` feature | Hard | `PackageConfig.files` and `path_resolver.resolve_variables` |
| `glob` / `shutil` (stdlib) | Python | |

## Acceptance Criteria

1. `dia pipeline --stage package --target googletest --config Debug` copies `python311.dll` and `*.diaapp` files to `Cluiche/bin/Debug/x64/` and `Cluiche/bin/Debug/x64/Data/Manifests/` respectively
2. Running again without `--force` skips all copies and logs "package: already staged"
3. `--force` re-copies all files regardless of timestamps
4. Glob patterns (`*.dll`, `**`, `*`) expand correctly and all matched files are copied
5. Destination directories are created if they do not exist
6. A source glob that matches no files logs a warning but does not fail the stage
7. `$(OutDir)` resolves to `Cluiche/bin/Debug/x64/` for Debug, `Cluiche/bin/Release/x64/` for Release
8. `$(Configuration)` in a src path (e.g., CEF's `bin/$(Configuration)/`) resolves correctly
9. Files that are already up to date (dest newer or equal mtime) are skipped without being re-copied

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaPipeline | @docs/specs/systems/dia/diapipeline.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only |
| PD-005 | Platform | x64 Windows only | Compliant — `$(Platform)` hardcoded `x64`; path conventions are Windows |
| PD-006 | Platform | VS project files are source of truth | Compliant — package stage coexists with `.vcxproj` post-build events; does not modify them |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — `$(OutDir)` resolved to mirror `Directory.Build.props` convention; not overridden |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — 0 success/skip, 1 unexpected failure |
| SD-PIPE-001 | DiaPipeline | `pipeline.toml` is single source of truth | Compliant — all copy rules in `pipeline.toml` |
| SD-PIPE-002 | DiaPipeline | Stage ordering fixed | Compliant — package is stage 4, always last |
| SD-PIPE-003 | DiaPipeline | `$(OutDir)` / `$(Configuration)` resolved at runtime | Compliant — `path_resolver.resolve_variables` used for all paths |
| SD-PIPE-006 | DiaPipeline | Package coexists with `.vcxproj` xcopy events | Compliant — no xcopy events removed by this feature |
| SD-ENV-010 | DiaEnv | Python 3.11 | Compliant |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | CEF `Resources/**` | Does `External/CEF/Resources/**` exist in the repo (gitignored binary)? | CEF is a binary SDK in `External/` managed by `deps.json`. It is gitignored. The package stage copy rules only execute after `dia env setup` has restored the dep. If CEF is not restored, the glob matches nothing and the stage logs a warning rather than failing. |
| 2 | Unmatched glob | Should an unmatched glob be a warning or a failure? | Warning — the dep may simply not have been restored yet (CEF, SFML). The stage continues and exits 0. The developer can re-run after `dia env setup` to get the files. This matches the existing `xcopy` behaviour (which also silently does nothing if the source doesn't exist). |
| 3 | `locales/**` pattern | Does `glob.glob(..., recursive=True)` correctly expand `locales/**` to all files recursively? | Yes — Python's `glob.glob` with `recursive=True` and a `**` wildcard recursively expands. Files within subdirectories are matched. |
| 4 | Concurrent package runs | Is there a race condition if two `dia pipeline` runs execute package simultaneously? | Not a concern in the current single-developer use case. No locking needed. |
