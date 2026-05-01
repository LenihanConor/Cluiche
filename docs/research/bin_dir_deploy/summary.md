# Research Summary — Bin Directory Layout & Clean Deploy

**Session folder:** docs/research/bin_dir_deploy/
**Date:** 2026-04-27

## One-Line Answer

Branch `OutDir` in `Directory.Build.props` on `$(ConfigurationType)` so each app gets a clean self-contained runnable folder under `Cluiche/bin/<AppName>/`, and update `path_resolver.py` to match.

## Journey

1. **Explored:** The current flat `bin/Debug/x64/` mixes 138 files from all apps and all modules — `.lib`, `.pdb`, `.exe`, DLLs, and UI assets for every project in one directory. `out/<AppName>/` already demonstrates the per-app model to mimic.
2. **Ideated:** 8 candidates generated, ranging from targeted single-file fixes (S) to a full Unreal-style compile/stage pipeline (L).
3. **Evaluated:** Candidate 3 (per-app OutDir + auto post-build deploy) scored highest at 4.00; key differentiator was that it fully honours PD-008/PD-009 and the automation half was already in place.
4. **Chose:** Candidate 3 confirmed — reduces to two file changes since xcopy removal and DIA CLI wiring are already done.

## Chosen Work Item

**Name:** Per-App Bin Directory Layout
**Home module:** `Directory.Build.props` + `Dia/DiaCLI/dia_cli/commands/pipeline/path_resolver.py`
**Suggested spec type:** Feature
**Estimated size:** S

## Key Insights from Exploration

- All post-build events already call `dia pipeline deploy` — Candidate 7 (xcopy removal) is already done; no prerequisite work needed
- `intermediate/` and `log/` are unaffected by any OutDir change — they have their own IntDir/TLogLocation properties
- `bin/exe/` is a stale artifact from a previous layout experiment — safe to delete
- `GoogleTests.vcxproj` passes `$(OutDir)` to the deploy command while all others pass `$(TargetDir)` — inconsistency to fix
- Windows DLL load order requires the exe and all its runtime DLLs in the same directory — per-app folders satisfy this cleanly
- The `ConfigurationType` MSBuild property cleanly distinguishes applications from libraries without per-project changes, satisfying PD-008

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| 2. Flat bin + staging | Doesn't fix root layout; adds a redundant staging copy |
| 4. Libs to IntDir | Breaks linker search paths across 20 projects |
| 5. Manifest install_dir | Extra config for no benefit over convention-based resolution |
| 7. Xcopy removal | Already complete |
| 8. Unreal-style staging | Right long-term direction but out of scope for current need |

## References

- docs/research/bin_dir_deploy/explore.md
- docs/research/bin_dir_deploy/ideate.md
- docs/research/bin_dir_deploy/evaluate.md
- docs/research/bin_dir_deploy/choose.md
