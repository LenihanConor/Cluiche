# Research: Evaluate — Bin Directory Layout & Clean Deploy

**Input:** docs/research/bin_dir_deploy/ideate.md

## Scoring Criteria

- **Engine Value** (0.25): Improves Dia module build system reusability, clarity, or capability
- **Game Value** (0.20): Improves developer experience running CluicheTest or CluicheEditor
- **Implementation Cost** (0.25): Inverse of effort — 5 = very cheap (S), 1 = very expensive (XL)
- **Risk** (0.15): Inverse of uncertainty — 5 = well-understood and safe, 1 = highly uncertain
- **Cluiche Fit** (0.15): Aligns with PD-006/PD-008/PD-009, DIA CLI authority, `out/` pattern

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. Per-App OutDir (ConfigurationType branch) | 4 | 3 | 4 | 4 | 5 | **3.95** |
| 2. Flat bin + staging folder | 2 | 3 | 4 | 4 | 3 | 3.15 |
| 3. Per-App OutDir + auto post-build deploy | 5 | 4 | 3 | 3 | 5 | **4.00** |
| 4. Libs-only to IntDir | 2 | 2 | 4 | 3 | 3 | 2.80 |
| 5. Manifest-driven deploy (install_dir in toml) | 3 | 3 | 3 | 4 | 4 | 3.30 |
| 6. Per-App OutDir + bin/lib/ subfolder | 4 | 3 | 4 | 4 | 5 | **3.95** |
| 7. Xcopy removal + DIA CLI gate | 3 | 2 | 3 | 4 | 4 | 3.10 |
| 8. Full Unreal-style staging pipeline | 5 | 5 | 2 | 2 | 4 | 3.65 |

## Top 3 Candidates

### Rank 1: Candidate 3 — Per-App OutDir + Auto Post-Build Deploy (score: 4.00)
**Why:** Gets the best of both worlds — `Directory.Build.props` branches on `$(ConfigurationType)` to route app binaries to per-app folders (matching PD-009's `out/<AppName>/` pattern), and a companion `Directory.Build.targets` file fires `dia pipeline deploy` automatically on every application build. Developers never need to think about deploying; build in VS or via CLI and the runnable folder is immediately up to date. Both PD-008 and PD-009 are fully honoured. DIA CLI remains the sole deploy authority.
**Watch out for:** Post-build events that invoke CLI tools can silently fail in VS if the tool is not on PATH. Need a robust failure mode — post-build should fail the build on non-zero exit. Also, the auto-deploy will run `ui_builds` (Vite) on every build unless DIA CLI's mtime check gates that correctly.

---

### Rank 2: Candidate 1 — Per-App OutDir via ConfigurationType Branch (score: 3.95)
**Why:** The minimum viable layout fix. One change to `Directory.Build.props` and one update to `path_resolver.py` gives every app its own clean bin folder with no per-project changes. Libs land in the shared pool where the linker already looks. No automation risk from post-build events. Good foundation for adding auto-deploy later.
**Watch out for:** Deploy remains a manual/CI step — a fresh clone requires `dia pipeline deploy` before any app is runnable. Also shares the score with Candidate 6; the choice between them comes down to whether you want a named `bin/lib/` subfolder or just tolerate libs in the shared `bin/Debug/x64/` pool.

---

### Rank 3: Candidate 6 — Per-App OutDir + bin/lib/ Subfolder (score: 3.95)
**Why:** Identical approach to Candidate 1 but with an explicit `Cluiche/bin/lib/Debug/x64/` folder for static libs. The named subfolder makes the intent clear — if you see `bin/lib/` you know those are linker inputs, not runtime artifacts. Slightly more polished than Candidate 1, same risk profile.
**Watch out for:** Requires updating `AdditionalLibraryDirectories` in `Directory.Build.props` to point to `bin/lib/` instead of `bin/`. More chances to miss a project that has a hardcoded lib path override somewhere. Need to verify no `.vcxproj` has hardcoded `bin/Debug/x64` lib paths.

---

## Recommendation

**Candidate 3** is the right target state, but **Candidate 7** (xcopy removal) is its mandatory prerequisite. The current codebase has MSBuild xcopy steps and DIA CLI deploy coexisting — before wiring auto post-build deploy you must remove all xcopy steps from `.vcxproj` files, or the two will fight. The practical implementation order is: (1) strip all xcopy deploy steps from `.vcxproj` files [Candidate 7]; (2) change `Directory.Build.props` to branch on `$(ConfigurationType)` and update `path_resolver.py` [core of Candidate 1/6]; (3) add `Directory.Build.targets` to auto-invoke `dia pipeline deploy` on Application builds [Candidate 3 automation layer]. This satisfies PD-008 (props owns OutDir), PD-009 (per-app layout mirrors `out/<AppName>/`), and the user's requirement that DIA CLI is the sole deploy authority.
