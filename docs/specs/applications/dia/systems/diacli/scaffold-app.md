# Feature Spec: scaffold-app

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diacli/diacli.md | **scaffold-app** |

**Origin:** CoW (@docs/specs/applications/cow/cow.md) was built by hand from CluicheTest's pattern; this feature turns that pattern into a generator any future game can use. Logged as Dia need #7 in `docs/specs/applications/cow/dia-needs.md`.
**Status:** `Approved`

---

## Problem Statement

Creating a Cluiche application means copying and trimming about ten files by hand: the project, solution entry, `Main.cpp`, game manifest, main manifest, observation file, asset catalogue, pipeline target, and the CLI's hard-coded target lists. `dia scaffold stage`, `module` and `plugin` exist but nothing scaffolds an application. Every new game would repeat CoW's trial-and-error trimming.

---

## Solution Overview

`dia scaffold app <Name>` generates a CoW-equivalent skeleton for a new application: a project that builds, boots to the baseline `BootMenuModule`, and lists one placeholder stage. It also makes the CLI's target list data-driven from `pipeline.toml`, so a new app needs only a toml block, not Python edits.

### Generated files (for `<Name>`)

| Path | Content |
|------|---------|
| `Cluiche/<Name>/<Name>.vcxproj` + `.filters` | Application project: `OutDir=$(BinRoot)$(MSBuildProjectName)\...`, deploy PostBuildEvent for the new target, `LocalDebuggerWorkingDirectory=$(OutDir)`, references to the Dia libraries and CluicheGameBaseline |
| `Cluiche/<Name>/Main.cpp` | Compose `.diagame`, register path aliases, validate, run loop at 30 Hz |
| `Cluiche/Assets/<Name>/<name>.diagame` | Window title, observation file, catalogue, path aliases, imports |
| `Cluiche/Assets/<Name>/<name>_main.diaapp` | Stages `Boot` and `Placeholder`; the trimmed module and stream set the Boot path needs; `initial_stage: Boot` |
| `Cluiche/Assets/<Name>/<name>.diaobservation`, `assets.catalogue.json` | Minimal observation config and catalogue |
| `Cluiche/Assets/<Name>/global.stage.json` | Source for the `stage.global` asset stage; the asset service loads it at start |
| `Cluiche/Assets/<Name>/Stages/Placeholder/...` | `.diastage`, stage `.diaapp`; stage module under `Cluiche/<Name>/` that logs and idles; `"Boot"` transition |
| `Cluiche/Cluiche.sln` entry | Via the `sln-add` logic (with a root option), solution folder `executables` |
| `[targets.<name>]` in `pipeline.toml` | Stages `compile-code, build-assets, deploy`, `app_name`, `catalogue_manifest`, `build_deps`, deploy list with an asset glob |

### Data-driven target registry (prerequisite refactor)

Today `launch.py` (`_TARGET_EXE_MAP`), `deploy_runner.py` (`_DEPLOY_TARGETS`) and help text in `pipeline.py` and `run.py` list targets by name. `PipelineConfig` already loads `[targets.*]` including `app_name`. The refactor derives these lists from `pipeline.toml`:

- Launchable targets are those with an executable: `exe` key if set, else `Cluiche/bin/{app_name}/{config}/x64/{app_name}.exe`.
- Deploy targets are every `[targets.*]` block.
- Existing targets (`googletest`, `cluichetest`, `cluicheeditor`, `diasfml`, `diauiultralight`) behave exactly as before; any that differ from the default path pattern set `exe` explicitly.
- Unknown-target errors list the targets from the toml.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Every existing target (`googletest`, `cluichetest`, `cluicheeditor`, `diasfml`, `diauiultralight`) resolves the same executable path and deploy behaviour after the registry refactor | pytest comparing old and new resolution; `dia launch cluichetest` still works |
| AC2 | Adding a `[targets.x]` block to `pipeline.toml` makes `dia launch x`, `dia run x` and `dia pipeline deploy x` recognise it with no Python edit | pytest with a temporary toml |
| AC3 | An unknown target error lists the targets found in `pipeline.toml` | pytest |
| AC4 | `dia scaffold app ScratchApp` writes every file in the table above and nothing outside those paths | pytest on a temp repo tree |
| AC5 | `--dry-run` prints the file list and changes nothing | pytest |
| AC6 | Running it twice is safe: existing files are not overwritten without `--force`; the solution entry and toml block are not duplicated | pytest |
| AC7 | The generated app passes `dia validate manifest` | Manual: run on `ScratchApp` |
| AC8 | The generated app builds with `dia run scratchapp` and reaches an empty-except-`Placeholder` Boot menu; selecting `Placeholder` and returning works; the process exits 0 | Manual: build and launch `ScratchApp`, then delete it |
| AC9 | The generated file layout matches the hand-built CoW skeleton (same file kinds, same structure) | Manual: diff the two trees |
| AC10 | `diacli.md`'s Out of Scope no longer lists project scaffolding; `dia scaffold --help` describes `app` | Review |

---

## Tasks

| # | Task | Test | Status | Model |
|---|------|------|--------|-------|
| 1 | **RED/GREEN:** data-driven target registry from `pipeline.toml` (`launch`, `run`, `pipeline`, `deploy`, error messages); existing targets unchanged | AC1, AC2, AC3 | Done | sonnet |
| 2 | Add Jinja2 templates for the generated files (derived from the hand-built CoW skeleton) | AC4 | Done | sonnet |
| 3 | **RED/GREEN:** `dia scaffold app` command (name derivation, templates, solution entry via `sln-add` logic, toml block, `--dry-run`, `--force`, `--stage <Name>` default `Placeholder`) | AC4, AC5, AC6 | Done | sonnet |
| 4 | Generate `ScratchApp` outside the committed tree, validate, build with `dia run`, boot, return, exit; diff against CoW; delete it | AC7, AC8, AC9 | Draft | haiku |
| 5 | Amend `diacli.md` (Out of Scope, features table) and `dia scaffold --help` text | AC10 | Done | haiku |

---

## Binding Decisions

| Source | Constraint |
|--------|------------|
| Engine vs application boundary | A reusable capability any game can use belongs in Dia; this is DiaCLI, not CoW. |
| SD-CLI-007 | Scaffolding is template-based (Jinja2), consistent with `dia command create`. |
| Sign-off | The registry refactor is a larger DiaCLI change than the `cow` target alone. Signed off by the user together with spec approval. |
| CLAUDE.md | Everything is run through `dia`; if the CLI cannot do it, fix the CLI. |

## Open Design Questions

1. The generated `.vcxproj` project references are copied from CluicheTest; if the baseline grows, the template drifts. Option: generate references from a list the template reads from one place.
2. **Resolved (build order reversed):** templates are derived from CluicheTest plus the baseline `BootMenuModule` (after the promotion), not from a hand-built CoW. CoW itself is generated with `dia scaffold app CoW --stage FrontEnd`, replacing skeleton tasks B3 to B5; CoW's verification (skeleton B6) is the boot proof. Task 4 (`ScratchApp`) reduces to a throwaway check that the default `Placeholder` output validates and builds, and AC9 compares the generated CoW against CluicheTest's file kinds. Needs the promotion complete before task 2.
3. **Resolved:** `--stage <Name>` argument, default `Placeholder`. CoW passes `--stage FrontEnd`.
