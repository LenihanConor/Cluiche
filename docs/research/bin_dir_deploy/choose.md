# Research: Choice — Bin Directory Layout & Clean Deploy

**Date:** 2026-04-27
**Chosen candidate:** Candidate 3 — Per-App OutDir + Auto Post-Build Deploy

## Rationale

The layout restructuring is the right target state and the automation prerequisite (Candidate 7 — xcopy removal, DIA CLI wiring) is already fully in place. The implementation reduces to two file changes: branch `OutDir` in `Directory.Build.props` on `$(ConfigurationType)` so application binaries land in per-app folders, and update `path_resolver.py` to derive the correct per-app OutDir from the target name.

Top evaluation factors: highest score (4.00), fully honours PD-008 and PD-009, zero per-project changes required, post-build deploy already wired.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 2. Flat bin + staging folder | Leaves the flat soup in place; doesn't fix the root layout problem |
| 4. Libs-only to IntDir | Breaks linker search paths; high risk for low gain |
| 5. Manifest-driven deploy | Extra complexity in pipeline.toml for no benefit over convention |
| 7. Xcopy removal + DIA CLI gate | Already done — all post-build events already call DIA CLI |
| 8. Full Unreal-style staging | Correct long-term vision but too large for current need |
| 1/6. Per-App OutDir only | Subset of Candidate 3; auto-deploy already in place so no reason to stop short |

## Pre-Spec Commitments

- `Directory.Build.props` branches on `$(ConfigurationType)`: `Application` → `Cluiche/bin/$(MSBuildProjectName)/$(Configuration)/$(Platform)/`; `StaticLibrary`/`DynamicLibrary` → `Cluiche/bin/$(Configuration)/$(Platform)/` (shared linker pool)
- `path_resolver.py` `resolve_out_dir` accepts a target name and returns the per-app path for application targets
- All post-build events pass `$(TargetDir)` (not `$(OutDir)`) — fix the `GoogleTests.vcxproj` inconsistency as part of this work
- `bin/exe/` stale directory cleaned up as part of this work
- No per-project `.vcxproj` changes required for the OutDir change
- DIA CLI remains the sole deploy authority — no new xcopy steps introduced

## Next Step

Run /spec-feature with this candidate as input.
Suggested parent system: DiaBuildSystem (or nearest existing build/deploy system spec)
