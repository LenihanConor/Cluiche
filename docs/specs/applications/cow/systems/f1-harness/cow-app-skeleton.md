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

Mirror CluicheTest's structure, trimmed to what a Boot menu and stub stage need. Verified against CluicheTest's `Main.cpp`, `cluichetest.diagame`, `cluiche_main.diaapp`, a stage directory, `pipeline.toml` and DiaCLI.

| Piece | CluicheTest (verified) | CoW |
|-------|------------------------|-----|
| Project | `Cluiche/CluicheTest/CluicheTest.vcxproj` with `ProjectReference`s to the Dia libraries | `Cluiche/CoW/CoW.vcxproj` in `Cluiche.sln`, same references trimmed to what CoW links, plus CluicheGameBaseline |
| Entry | `Main.cpp` (354 lines): `--dump-schema` early exit, compose `.diagame`, register path aliases, validate, create `TestResultsRegistry`, `Application` loop at 30 Hz | `Cluiche/CoW/Main.cpp`: same flow without `TestResultsRegistry`; schema dump per open question 3 |
| Game manifest | `cluichetest.diagame`: window title, `asset_catalogue`, `observation` file, path aliases; imports the main `.diaapp` and every `.diastage` | `cow.diagame` with the same keys (`cow.diaobservation`, a minimal `assets.catalogue.json`) importing the main `.diaapp` and `FrontEnd` stage |
| Main manifest | `cluiche_main.diaapp`: `initial_stage: Boot`; 26 stage entries; 24 streams; 3 PUs (Main, Sim, Render) with global modules; Boot-scoped `LoadingScreenModule`, `BootMenu` | `cow_main.diaapp`: stage list `Boot` and `FrontEnd`; only the streams and modules the Boot path needs (see below) |
| Stage | `.diastage` pointing at a stage `.diaapp` (SimPU stage module with channel roles); plus an entry in the main `.diaapp` stage list with `"transitions": ["Boot"]`; plus a `.diagame` import | Same three pieces for `FrontEnd`, with the stage module in `Cluiche/CoW/` |
| CLI | `cluichetest` entries in `launch.py` exe map, `pipeline.py` help, `deploy_runner.py` list, `pipeline.toml` `[targets.cluichetest]` | `cow` entries in the same places (see Tasks) |

**Module and stream trimming.** Keep what the Boot path uses; drop test-specific modules (`TestStageHUDModule`, the per-stage visual debugger/HUD modules, `PickingModule`, `Physics2DModule`, `AssetRuntime*`). Specific points found:

- `MainStateProducerModule` in CluicheTest maps to `TestMainStateProducerModule`, a CluicheTest type. CoW must use the baseline `MainStateProducerModule`.
- `BootMenuNavRequest` is consumed by `SimNavigationHandlerModule`, so it and the channel stay.
- `BootMenu` refers to `DebugUI` and `DebugServerHost`, so both stay.
- Final module and stream lists are decided in task 3 by trimming and then running `dia validate manifest` and launching, not guessed here.

**Return to Boot.** A stage returns because its entry in the main `.diaapp` stage list has `"transitions": ["Boot"]`, and `Boot` lists the stage in its own transitions. `FrontEnd` gets both.

**Pipeline.** `dia run cow` goes through `pipeline.toml`. CluicheTest's target has stages `compile-code, reflect, build-assets, deploy`, `build_deps` (protobuf, bgfx shaders), `catalogue_manifest`, and a `deploy.files` list of SFML DLLs, manifests, the schema, and one entry per stage file. Deploy entries accept `*` patterns, so CoW can use one glob for its stage assets instead of per-file entries.

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
| 1 | **RED/GREEN:** add the `cow` target to DiaCLI (`launch.py` exe map, `pipeline.py`, `deploy_runner.py`) and a `[targets.cow]` block in `pipeline.toml` (stages, `app_name`, `catalogue_manifest`, `build_deps`, deploy files with a stage-asset glob), with tests mirroring the existing target tests | AC2 | Draft | sonnet |
| 2 | Create `CoW.vcxproj`, filters, and `Cluiche.sln` entry (`dia docs vcxproj-add`) | AC1 | Draft | sonnet |
| 3 | Create `Cluiche/Assets/CoW/`: `cow.diagame`, `cow_main.diaapp` (trimmed per Design), `cow.diaobservation`, minimal `assets.catalogue.json`; the `FrontEnd` stub stage (`.diastage`, stage `.diaapp`, stage module, main-manifest entry with `"Boot"` transition, `.diagame` import); validate | AC3, AC4, AC5 | Draft | sonnet |
| 4 | Write `Cluiche/CoW/Main.cpp` | AC3, AC8 | Draft | sonnet |
| 5 | **RED/GREEN:** add `--app` to `dia scaffold stage` with tests. The command currently hard-codes CluicheTest's namespace, asset paths, `cluiche_main.diaapp`, `.diagame`, `.vcxproj` and filters (9 touch points) and does not touch `pipeline.toml`; `--app` must parameterise all of these | AC6 | Draft | sonnet |
| 6 | Verify: `dia run cow` end to end (menu, launch FrontEnd, return, exit), then `dia run googletest` and a CluicheTest launch | AC1 to AC8 | Draft | haiku |

## Binding Decisions

| Source | Constraint |
|--------|------------|
| AD-001 | Tasks 1 and 5 edit DiaCLI and task 2 edits `Cluiche.sln`; each needs sign-off. |
| AD-005 | `FrontEnd` is one stage; later testbeds follow the same pattern. |
| CLAUDE.md | Run and launch only through `dia run` / `dia launch`; if the CLI cannot do it, fix the CLI. |

## Open Design Questions

1. CluicheTest's `Main.cpp` holds generic bootstrap (path-alias registration, base-dir computation, schema dump). Copying it into CoW duplicates engine-generic code. A shared helper would belong in the baseline or Dia, so it needs your decision. Default here: copy only what CoW needs and log a Dia need for the helper.
2. The `reflect` pipeline stage runs `--dump-schema` on the binary to write `registeredtypes.diaschema`. Including it means copying the ~150-line schema dump into CoW's `main()`. Default here: omit `reflect` from CoW's pipeline for now (the runtime does not need the schema file) and add it when the editor needs CoW's schema.
3. `BootMenuNavRequest` handling lives in a CluicheTest-era `SimNavigationHandlerModule`; task 3 must confirm it is baseline and not test-specific.
