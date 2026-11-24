# Feature Spec: V-1 Stage and World

## Parent System
@docs/specs/applications/cow/systems/v1-tilerendering/v1-tilerendering.md (Feature 1)

**Gameplay Domains:** rendering, debug
**Depends on:** Strategic Map Features 1, 2 and 4; S-1 (world generation, hand-off SD-S1-010, built); F-1 Harness (Boot menu, built); DraoiHexWorldRender3D (hosted by `StrategicMapModule`)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/index.html`, `js/app.js` (world selection, camera presets, `#key=value` state), `tools/convert_golden.py`

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

The V-1 stage: a `.diastage` and `.diaapp` that load an S-1 world and put the Strategic Map on screen, with three **modes** (Correctness, Performance, Look-dev) chosen from the panel and applied through the stage's launch parameters (PD-010). It follows the S-2.5/S-3 stage pattern.

This feature delivers the empty stage: world, modules, mode, time source, camera presets. Stand-ins, panel and checks are Features 2–4.

## Problem

- The map needs somewhere to run before the real game exists, on real S-1 worlds at the three sizes, with a repeatable launch (preset + seed + size).
- The three modes have different worlds and settings (80×52 correctness, 104×64 performance, any size look-dev).
- It must be launchable from the F-1 debug menu and from E2E automation (`dia launch`/`dia run`).

## Goals

1. **Stage files** created with `/new-cow-stage`: `v1_tile_rendering.diastage`, `.diaapp`, a curated-worlds table (`v1_worlds.json`, `cow.v1_worlds`).
2. **World hand-off** via S-1's `WorldGenerationModule` config: `preset`, `seed`, `size` or `world_file`, then wait for `IsReady(1)` or `WorldGenerationComplete` (SD-S1-010).
3. **Modes** as named launch parameters on the `.diastage`:
   - `correctness`: Standard 80×52, a world with every biome present (curated list).
   - `performance`: Large 104×64, everything on, the capture/perf harness visible.
   - `look-dev` (default): Small 66×42, the full panel (Feature 3).
4. **Module list** (manifest order): the baseline modules, `WorldGenerationModule`, `GameSpeedController`, `MapSnapshotPublisher`, `StrategicMapModule` (owns the `HexWorldMap`; there is no separate map module since 2026-11-23), `V1StageModule` (stand-ins in Feature 2, panel in 3).
5. **Camera presets**: strategic overview, regional, tactical (single cell), and a custom store/recall of 4 user cameras.
6. **World picker** (curated worlds, plus preset/seed/size and world file) and "regenerate".
7. **Time source**: the day comes from `GameSpeedController`, with its four presets; V-1 adds a "scrub to day" command for the look-dev mode.
8. **Registered in F-1's debug menu**; `dia validate manifest` passes.

## Non-Goals

- Stand-in drivers, panel, checks (Features 2–4).
- Generating or editing worlds (S-1's own panel does that; V-1 calls S-1 commands).
- Save/load of the stage.

## Design

### Files

```
Cluiche/Assets/CoW/Stages/V1TileRendering/
    v1_tile_rendering.diastage             launch parameters: modes (correctness, performance, look-dev)
    misc/ApplicationFlow/v1_tile_rendering.diaapp
    v1_worlds.json                         envelope "cow.v1_worlds", version 1
Cluiche/CoW/Modules/V1StageModule.h/.cpp   (V1 prefix, feature-local, AD-002)
```

### Curated worlds (`v1_worlds.json`)

| id | preset | size | seed | why |
|---|---|---|---|---|
| `v1_all_biomes` | continental | Standard | chosen so every generated biome appears | Correctness (GDD Tests 1–6) |
| `v1_large` | continental | Large | 1 | Performance (Tests 7–9) |
| `v1_mountains` | highlands | Small | 1 | Relief, snow, lakes |
| `v1_islands` | archipelago | Small | 1 | Coasts, sea, small land |
| `v1_default` | default | Small | 1 | Default look-dev |

Each carries a **precheck** (the biome count present, a minimum of mountain, lake and river cells) so a changed S-1 gives "invalid world" instead of a silently useless test. The seeds are fixed when the S-1 Biome Registry lands (the registry changes biome assignment; re-pick then).

### Modes

A mode is a launch-parameter preset (PD-010) applied through `Module::OnStageLaunch`: it sets the world id, size, time preset, which Feature 3 panel sections show, and which Feature 4 checks are available. Modes never change which modules are built.

### Camera presets

`overview` (whole world, current pitch), `regional` (about 25 × 25 cells), `tactical` (about 8 × 8 cells). `cow.map.camera.set` with stored values; `v1.camera.store/recall {slot}`.

## Acceptance Criteria

1. **Launch.** `dia launch v1tilerendering` (or via the Boot menu) loads, generates the default world, and shows the map within a few seconds with no errors; the stage exits cleanly.
2. **Modes.** Each mode launches with its world and size, and the panel/check availability matches the mode table. Launching a mode does not change the module list.
3. **World hand-off.** The stage waits for S-1's ready signal and never draws a half-generated world; a failed generation shows an error line and keeps the previous world.
4. **Curated worlds.** Each curated world loads, passes its precheck, and `v1_all_biomes` contains every generated biome in the registry; a world that fails its precheck is reported with the reason.
5. **World change.** Switching world or size frees and rebuilds the map resources (resource counters return to baseline), with the camera reset to the overview.
6. **Camera presets.** `overview`, `regional`, `tactical` frame the expected extents (visible cell counts within 20%); store and recall return the same camera.
7. **Time.** The day advances with the chosen `GameSpeedController` speed preset; `v1.time.scrub {day}` sets it, the season weights follow the year clock, and Pause stops it.
8. **Manifest.** `dia validate manifest` passes; the debug menu lists the stage; `requires_tick_after` entries are correct (`MapSnapshotPublisher` after the world and speed modules).
9. **E2E.** An E2E scenario launches each mode, checks `cow.map.status` (world loaded, counts nonzero) and exits.
10. **Observability.** Log channel `V1`; info on mode, world and hand-off; error on failures; metric `v1.world_load_ms`; health Failing when no world loaded.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| AD-005 | One stage per testbed; modes in the panel | One `.diastage`; modes are launch parameters |
| PD-010 | `.diastage` metadata; launch parameters through `OnStageLaunch` | Modes as named presets; no change to built modules |
| SD-S1-010 | S-1 hand-off | Config `preset`/`seed`/`size`/`world_file` and the ready signal |
| V1-001 | V-1 hosts the Strategic Map's tools | The stage composes existing modules; no map logic here |
| AD-002 | Four-tier | `V1StageModule` is feature-local; shared parts stay in CoW-Shared |

## Open Design Questions

1. **Time control.** `GameSpeedController` has four presets (the fastest is about 233 ms per week). The look-dev prototype ran a year in about 30 s (12 days per second). Is the Fastest preset fast enough for seasonal tuning, or does V-1 need a faster "look-dev" speed (a config override, not a built-code change)? Decide when running the stage.
2. **Curated seeds.** `v1_all_biomes` needs a seed where every generated biome occurs. The registry rework changes classification, so seeds are chosen after S-1 Biome Registry lands (build order #1).
3. **Which size is the look-dev default?** Small is fastest to iterate; Standard shows more. Proposed Small; a panel switch changes it.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold stage with `/new-cow-stage`; manifest | AC 1, 8 | Not Started | haiku | |
| 2 | `V1StageModule`: world hand-off, mode application | AC 2, 3, 5 | Not Started | sonnet | |
| 3 | Curated worlds and prechecks | AC 4 | Not Started | sonnet | after Biome Registry for seeds |
| 4 | Camera presets, store/recall | AC 6 | Not Started | sonnet | |
| 5 | Time source and scrub | AC 7 | Not Started | sonnet | |
| 6 | E2E scenario, observability | AC 9, 10 | Not Started | sonnet | |
