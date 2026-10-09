# Feature Spec: CoW App Skeleton

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (decisions 1 and 3; Test 1, Test 2)
**Depends on:** @docs/specs/applications/cow/systems/f1-harness/bootmenu-promotion.md (the menu must be in the baseline first)
**Status:** `Done`
**Plan:** @docs/specs/applications/cow/systems/f1-harness/cow-app-skeleton.plan.md

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
| Project | `Cluiche/CluicheTest/CluicheTest.vcxproj`: `Application`, `OutDir=$(BinRoot)$(MSBuildProjectName)\$(Configuration)\$(Platform)\` (so `Cluiche/bin/CluicheTest/<cfg>/x64/`), `ProjectReference`s to the Dia libraries, and a PostBuildEvent running `dia pipeline deploy cluichetest` to copy assets into `<OutDir>assets/` | `Cluiche/CoW/CoW.vcxproj` in `Cluiche.sln`: same shape, so output goes to `Cluiche/bin/CoW/<cfg>/x64/CoW.exe`, with a PostBuildEvent running `dia pipeline deploy cow`, references trimmed to what CoW links plus CluicheGameBaseline, and `LocalDebuggerWorkingDirectory=$(OutDir)` so F5 finds `assets/` |
| Entry | `Main.cpp` (354 lines): `--dump-schema` early exit, compose `.diagame`, register path aliases, validate, create `TestResultsRegistry`, `Application` loop at 30 Hz | `Cluiche/CoW/Main.cpp`: same flow without `TestResultsRegistry`; schema dump per open question 3 |
| Game manifest | `cluichetest.diagame`: window title, `asset_catalogue`, `observation` file, path aliases; imports the main `.diaapp` and every `.diastage` | `cow.diagame` with the same keys (`cow.diaobservation`, a minimal `assets.catalogue.json`) importing the main `.diaapp` and `FrontEnd` stage |
| Main manifest | `cluiche_main.diaapp`: `initial_stage: Boot`; 26 stage entries; 24 streams; 3 PUs (Main, Sim, Render) with global modules; Boot-scoped `LoadingScreenModule`, `BootMenu` | `cow_main.diaapp`: stage list `Boot` and `FrontEnd`; only the streams and modules the Boot path needs (see below) |
| Stage | `.diastage` pointing at a stage `.diaapp` (SimPU stage module with channel roles); plus an entry in the main `.diaapp` stage list with `"transitions": ["Boot"]`; plus a `.diagame` import | Same three pieces for `FrontEnd`, with the stage module in `Cluiche/CoW/` |
| CLI | `cluichetest` entries in `launch.py` exe map, `pipeline.py` help, `deploy_runner.py` list, `pipeline.toml` `[targets.cluichetest]` | `cow` entries in the same places (see Tasks) |

**Module and stream trimming.** Keep what the Boot path uses; drop test-specific modules (`TestStageHUDModule`, the per-stage visual debugger/HUD modules, `PickingModule`, `Physics2DModule`, `AssetRuntime*`). Specific points found:

- `MainStateProducerModule` in CluicheTest maps to `TestMainStateProducerModule`, a CluicheTest type. CoW must use the baseline `MainStateProducerModule`.
- `BootMenuNavRequest` is read by `SimNavigationHandlerModule` on SimPU (a baseline module, verified), so both stay.
- `BootMenu` refers to `DebugUI` and `DebugServerHost`, so both stay.
- Final module and stream lists are decided in task 3 by trimming and then running `dia validate manifest` and launching, not guessed here.

**Return to Boot.** A stage returns because its entry in the main `.diaapp` stage list has `"transitions": ["Boot"]`, and `Boot` lists the stage in its own transitions. `FrontEnd` gets both.

**Asset pipeline.** CoW reuses the existing pipeline; no new pipeline code is written. The `build-assets` stage delegates to DiaAssetPipeline (validate, transform, deploy phases), driven entirely by the target's `catalogue_manifest` in `pipeline.toml` (it is skipped when none is configured; CluicheEditor has none). CluicheTest's `assets.catalogue.json` lists each asset with references (global stage manifest, `shader.ui_frag`, UI folders, per-stage assets). For CoW:

- `[targets.cow]` sets `catalogue_manifest = "Cluiche/Assets/CoW/assets.catalogue.json"` and `build_deps.bgfx_shaders = true` (the renderer and debug UI need the compiled shaders).
- The F-1 catalogue is minimal: the main manifest, the `FrontEnd` stage, and only the shaders and UI assets the Boot menu needs. Task 3 builds it by trimming CluicheTest's catalogue and confirming the Boot menu draws.
- Cooked content (textures, meshes, rigs, tiles) is not part of F-1. Each later testbed adds its own catalogue entries and uses the existing asset rules.

**Pipeline.** `dia run cow` goes through `pipeline.toml`. CluicheTest's target has stages `compile-code, reflect, build-assets, deploy`, `build_deps` (protobuf, bgfx shaders), `catalogue_manifest`, and a `deploy.files` list of SFML DLLs, manifests, the schema, and one entry per stage file. Deploy entries accept `*` patterns, so CoW can use one glob for its stage assets instead of per-file entries.

## Existing DiaCLI tooling (surveyed)

| Command | Use here | Limitation found |
|---------|----------|------------------|
| `dia scaffold sln-add` | Register `CoW` in `Cluiche.sln` (`--folder executables`, `--deps`) with all configs | Hard-codes `Dia/<name>/<name>.vcxproj`; CoW is at `Cluiche/CoW/`. Needs a path option (DiaCLI change) |
| `dia docs vcxproj-add <Project> <File>` | Add each new `.h`/`.cpp` to `CoW.vcxproj` and filters | None; idempotent |
| `dia scaffold stage` | Generate stages | CluicheTest-only (see task 5). Skill `new-cluichetest-stage` wraps it; add a `new-cow-stage` skill after `--app` exists |
| `dia scaffold module` / `plugin` | Not used (they create Dia modules and editor plugins) | No `scaffold app` exists. The CoW project is built by hand from CluicheTest's pattern |
| `dia validate manifest` | Check `.diagame`/`.diaapp`/`.diastage` | None |
| `dia pipeline deploy <target>` | The post-build deploy step | Needs the `cow` target (task 1) |
| `dia check sln-sync` | Confirm solution folder layout after adding CoW | Run in verify |
| `dia check deps`, `dia docs precommit` | Final checks | Run in verify |
| `dia docs spec-scaffold` | Spec skeleton | Fails with relative `--output-dir` and writes the wrong parent; not used |

Because no `scaffold app` exists, a `dia scaffold app` generating this whole skeleton would be generic Dia tooling any future game needs. Logged as a Dia need rather than built here.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `CoW.vcxproj` is in `Cluiche.sln` and builds Debug and Release x64. |
| AC2 | `dia run cow` builds and launches; `dia launch cow` launches an already-built binary. |
| AC2a | In Visual Studio, CoW can be set as the startup project; building it produces `Cluiche/bin/CoW/<cfg>/x64/CoW.exe` with `assets/cow.diagame` and the deployed manifests beside it (the PostBuildEvent deploy); F5 launches to the Boot menu. |
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
| 2 | Create `CoW.vcxproj` (output dir, deploy PostBuildEvent, debugger working directory), filters, and `Cluiche.sln` entry via `dia scaffold sln-add` (extended with a path option) and `dia docs vcxproj-add` | AC1, AC2a | Draft | sonnet |
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
