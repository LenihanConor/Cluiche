# Feature Spec: CoW App Skeleton

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (decisions 1 and 3; Test 1, Test 2)
**Depends on:** @docs/specs/applications/cow/systems/f1-harness/bootmenu-promotion.md (the menu must be in the baseline first)
**Status:** `Draft`

## Summary

Give CoW a buildable, launchable executable that mirrors CluicheTest: `dia run cow` builds it and opens a `Boot` stage showing the promoted `BootMenuModule`. The Boot menu lists CoW's stages as transitions; at this point the only one is the stub `FrontEnd` stage. Also lets `dia scaffold stage` generate stages under CoW, so the 11 remaining testbeds can be added the same way as CluicheTest's.

## Goals

1. `dia run cow` / `dia launch cow` build and run CoW (Debug and Release x64).
2. CoW boots to a Boot menu built from the baseline `BootMenuModule`.
3. A stub `FrontEnd` stage (logs "front end not yet built" and idles) is reachable from the menu and returns to Boot like any CluicheTest stage.
4. `dia scaffold stage <Name> --app CoW` creates a CoW stage with all its touch points.
5. CluicheTest is unaffected.

## Non-Goals

1. The real front end (G-1) and any of the other 11 testbed stages.
2. Command-line flags or generic argument forwarding in DiaCLI (the front-end jump is a menu entry, see the boot-choice spec).
3. Status badges (CoW adds no `StageStatusModule`).
4. A shared application-bootstrap helper (see open question 1).

## Design

Mirror CluicheTest's structure, trimmed to what a Boot menu and stub stage need:

| Piece | CluicheTest | CoW |
|-------|-------------|-----|
| Project | `Cluiche/CluicheTest/CluicheTest.vcxproj` | `Cluiche/CoW/CoW.vcxproj`, added to `Cluiche.sln`, linking Dia and CluicheGameBaseline |
| Entry | `Main.cpp`: compose manifest, register path aliases, validate, run loop | `Cluiche/CoW/Main.cpp`: same flow, without `TestResultsRegistry` and with the schema dump only if needed (open question 1) |
| Assets | `Cluiche/Assets/CluicheTest/cluichetest.diagame` | `Cluiche/Assets/CoW/cow.diagame` |
| Main manifest | `cluiche_main.diaapp` (`initial_stage: Boot`, global modules, channels) | `cow_main.diaapp`: same global modules and channels minus test-specific ones (HUD, test stage modules); `initial_stage: Boot`; Boot stage runs `BootMenu` with config `title`/`project` |
| Stages | ~25 test stages | `FrontEnd` stub stage only |
| CLI | `cluichetest` target | `cow` target in the launch, run, pipeline and deploy target maps (`pipeline.toml` if needed) |

The `FrontEnd` stub is a transition from `Boot` (so it appears as the "Play" entry) and returns to `Boot` the same way CluicheTest stages do.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `CoW.vcxproj` is in `Cluiche.sln` and builds Debug and Release x64. |
| AC2 | `dia run cow` builds and launches; `dia launch cow` launches an already-built binary. |
| AC3 | CoW opens a Boot menu window using the baseline `BootMenuModule`, titled from config, listing exactly the Boot transitions (`FrontEnd`). |
| AC4 | Launching `FrontEnd` logs "front end not yet built" and idles; returning goes back to the Boot menu. |
| AC5 | `dia validate manifest` passes for the CoW manifests. |
| AC6 | `dia scaffold stage <Name> --app CoW` generates a stage under `Cluiche/CoW/` and its manifest and project entries; without `--app`, behaviour is unchanged. |
| AC7 | CluicheTest still builds and its Boot menu works after these changes. |
| AC8 | Closing the window shuts CoW down cleanly (process exits 0). |

## Tasks

| # | Task | Test | Status | Model |
|---|------|------|--------|-------|
| 1 | **RED/GREEN:** add the `cow` target to DiaCLI (launch, run, pipeline, deploy maps) with tests mirroring the existing target tests | AC2 | Draft | sonnet |
| 2 | Create `CoW.vcxproj`, filters, and `Cluiche.sln` entry (`dia docs vcxproj-add`) | AC1 | Draft | sonnet |
| 3 | Create `Cluiche/Assets/CoW/` manifests (`cow.diagame`, `cow_main.diaapp`) and the `FrontEnd` stub stage (module, `.diastage`); validate | AC3, AC4, AC5 | Draft | sonnet |
| 4 | Write `Cluiche/CoW/Main.cpp` | AC3, AC8 | Draft | sonnet |
| 5 | **RED/GREEN:** add `--app` to `dia scaffold stage` with tests | AC6 | Draft | sonnet |
| 6 | Verify: `dia run cow` end to end (menu, launch FrontEnd, return, exit), then `dia run googletest` and a CluicheTest launch | AC1 to AC8 | Draft | haiku |

## Binding Decisions

| Source | Constraint |
|--------|------------|
| AD-001 | Tasks 1 and 5 edit DiaCLI and task 2 edits `Cluiche.sln`; each needs sign-off. |
| AD-005 | `FrontEnd` is one stage; later testbeds follow the same pattern. |
| CLAUDE.md | Run and launch only through `dia run` / `dia launch`; if the CLI cannot do it, fix the CLI. |

## Open Design Questions

1. CluicheTest's `Main.cpp` holds generic bootstrap (path-alias registration, base-dir computation, schema dump). Copying it into CoW duplicates engine-generic code. A shared helper would belong in the baseline or Dia, so it needs your decision. Default here: copy only what CoW needs and log a Dia need for the helper.
2. How much of `cluiche_main.diaapp` is test-specific? Task 3 starts by listing global modules and channels one by one and keeping only what the Boot menu and stub need.
3. Does the schema dump need to exist for CoW now (editor tooling), or can it wait?
