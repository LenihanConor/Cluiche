# Feature Spec: DiaTerrainMap3D Module, Presentation and Tooling

## Parent System
@docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md

**Gameplay Domains:** rendering, camera, debug
**Depends on:** DiaTerrainMap3D Features 1–3; DiaBgfx3D Features A and D
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (frame loop, camera, UI spec, state defaults), `tools/shot.sh`

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

The module that makes the pieces a working map: `TerrainMap3DModule` on the render side, fed by FrameStream snapshots, owning the bake cache, textures, scatter, post stack and camera hooks, and exposing the controls, data views and headless capture the look-dev tooling needs.

- **Module and data flow.** Receives world snapshots and per-cell channel snapshots; runs the bake and scatter on the render side; draws each frame.
- **Camera.** A map camera built on `DiaCamera3D`: orbit with zoom-dependent pitch and height scale, bounds clamping.
- **Presentation presets.** Named bundles of post and style settings (strategic, cinematic, debug-flat).
- **Tooling.** Every style value and toggle is a DiaAPI command; data views; headless capture of a fixed camera and parameter set to a PNG.

## Problem

- Sim state lives on SimPU; rendering on the render PU. The rule is streams, not shared memory (feedback: PU boundaries are always streams). The module defines exactly what crosses and when.
- The look-dev tools (sliders, toggles, data views, screenshots) must be reproducible commands, not ad hoc UI (feedback: editor actions via DiaAPI), so the Strategic Map and V-1 can drive them.
- Occlusion behind mountain ranges and the sub-pixel far chunks need camera behaviour, not just rendering.

## Goals

1. **`TerrainMap3DModule`** (render side): connects streams, owns `TileCache`, `TerrainMesh`, `TerrainTextures`, `PropScatter`, `SpriteInstanceRenderer`, `ParticleSystem`, `PostProcessStack`, and draws them in order each frame.
2. **Stream contract** (what the game publishes):
   - `TerrainWorldSnapshot` (once per world / on class change): per-cell class, elevation, relief class, temperature, river inputs, grid description, version.
   - `TerrainChannelSnapshot` (per step): per-cell condition, weather, events, water state, version per channel.
3. **Versioned uploads.** Channels re-upload only when their version changes; world changes invalidate the tile cache.
4. **Map camera**: pan, zoom, orbit; pitch and height scale blend with zoom (flatter and lower when zoomed out); bounds clamp; smooth damping.
5. **Presentation presets** (named): `strategic`, `cinematic`, `debug-flat`; presets are parameter files.
6. **DiaAPI commands** for every control (`map3d.*`), each with help text and ranges from the schema.
7. **Data views** (class, elevation, condition, depth zone, temperature, margin, tile boundaries).
8. **Headless capture**: given a fixed camera, style, preset and seed, render N frames and write a PNG (through the DiaBgfx capture path), usable from E2E and DiaCLI.
9. **Observability**: the five pillars for the module.

## Non-Goals

- Choosing what the channels mean or simulating them (the game and V-1 stand-ins).
- Selection, picking and cell highlighting (DiaCellLayers). The module exposes `ScreenToWorld` and `WorldToCell` helpers.
- The editor panel UI (V-1 Feature 3 and the Strategic Map).
- Saving style files (Strategic Map Feature 4 owns load/save/reload; this module takes a parameter struct).

## Design

### Module and phases

`TerrainMap3DModule : Module`. `OnConnectStreams` reads `TerrainWorldSnapshot` and `TerrainChannelSnapshot` (FrameStream readers). Per render frame:

1. If the world snapshot version changed: rebuild geometry if the grid changed, run the bake request (cache invalidate all), rebuild scatter.
2. If a channel version changed: upload that channel (and rescatter cells for a condition change, particle density for weather and events).
3. Update camera, time, wobble frame (12 Hz), gust.
4. `EnsureResident(visible rect)` on the tile cache; upload new tiles; evict.
5. Draw: terrain → props → particles → translucent sprites → post → UI.

### Map camera

`MapCamera` wraps `Camera3D` and an `OrbitBehaviour`:

- `zoom z ∈ [zMin, zMax]` (view half-height in world units).
- `pitch(z)` and `heightScale(z)` follow smooth curves: `pitchNear` (default 55°) at close zoom, easing to `pitchFar` (default 70°) at far zoom; `heightScale` scales from 100% (near) to `farHeightFactor` (default 60%) at far zoom, so ranges occlude less when the whole world is in view. All three are style parameters.
- Pan is clamped to the world bounds (`BoundsClamp3D`). Yaw is user-controlled.
- Input is not owned here: the module exposes `Pan`, `Zoom`, `Orbit` calls; the game's input module (or the V-1 panel) calls them.

### Commands (DiaAPI)

`map3d.style.set {name, value}`, `map3d.style.get`, `map3d.preset.apply {name}`, `map3d.camera.set {tx, ty, zoom, pitch, yaw}`, `map3d.camera.get`, `map3d.view.data {name}`, `map3d.layer.toggle {name, on}`, `map3d.capture {path, width, height, frames, seed}`, `map3d.status` (counts, versions, cache). All register through the existing command registry with help text; values range-checked from the style schema. Registered on the render side; if a command must reach SimPU state it goes by EventStream request/reply (not needed in v1).

### Capture

`map3d.capture` renders `frames` frames at a fixed wobble frame and time with a fixed seed, then reads the scene back through DiaBgfx's capture path to a PNG. Used by V-1's capture set and the golden captures of Features 1–3.

## Acceptance Criteria

1. **Stream contract.** Publishing a world snapshot and a channel snapshot from a test SimPU module results in the map drawing them; no render-side code reads SimPU memory directly; versions gate uploads (a snapshot with an unchanged version causes no upload, counted).
2. **World change.** A new world snapshot with a different grid size rebuilds geometry, invalidates the cache and rescatters; the old world's resources are freed (resource counters return to baseline).
3. **Class change.** A succession change of K cells rebakes only affected tiles and rescatters only those cells (counters); the frame after shows the change.
4. **Camera curves.** `pitch(z)` and `heightScale(z)` are monotonic and continuous across zoom; at `zMin` they equal the near values and at `zMax` the far values; no jump when zooming through any value.
5. **Bounds.** Panning cannot move the view centre outside the world rectangle plus margin.
6. **Commands.** Each `map3d.*` command is registered with help; setting a style value through a command changes the next frame; an out-of-range value is clamped with a warning; `map3d.style.get` returns the value set.
7. **Presets.** Applying `strategic`, `cinematic` and `debug-flat` sets the documented parameter bundles exactly; `debug-flat` disables post, ink boil and sprites.
8. **Data views.** Each view renders its raw channel; switching view does not rebuild the world.
9. **Capture determinism.** Two `map3d.capture` runs with the same arguments produce byte-identical PNGs on the same machine (fixed seed, time, wobble); a different seed changes the output.
10. **Capture works with post.** A capture with the post pass on matches what the screen shows (the scene target is read back, not the backbuffer garbage).
11. **Helpers.** `ScreenToWorld` and `WorldToCell` round-trip a set of screen points through the camera and the grid (error under half a texel), including under tilt.
12. **Lifetime.** Module stop frees all GPU resources; start/stop 20 times leaks nothing.
13. **Observability.**
    - **Logs:** channel `TMAP`; info on world and channel snapshots and on preset changes; errors for rejected snapshots.
    - **Traces and profiling:** scopes `TerrainMap.Frame`, `TerrainMap.Bake`, `TerrainMap.Scatter`, `TerrainMap.Draw`.
    - **Metrics:** `tmap.frame_ms`, `tmap.bake_ms`, `tmap.tiles_resident`, `tmap.channels_uploaded`.
    - **Health:** an `IHealthReporter` that reports Failing when the world snapshot is invalid or GPU resources fail, Degraded when the tile cache is over budget.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| PD-002 / AD-004 | PU/Phase/Module | Module on the render PU; streams only across PUs (AC 1) |
| Feedback: editor actions via DiaAPI | Every action is a command | `map3d.*` commands; the panel calls the same commands |
| Feedback: UI separate from business logic | Typed command model first | Commands and parameter structs exist without any UI |
| PD-001 | StringCRC ids | Command, stream and parameter names |
| TMAP-005 | Per-cell channels | The channel snapshot is per cell |
| AD-001 | Engine changes need sign-off | New module doc; stream types registered in the stream type registry |

## Open Design Questions

1. **Snapshot size and cadence.** The world snapshot is about 150 KB at Large (per-cell arrays) and the channel snapshot about 106 KB. FrameStream copies a fixed-size struct by value, so the struct must be sized for the largest world, or a ServiceStream carries a pointer-free blob. Decide with a spike (`StreamWriter::Write` cost with 256 KB `T`).
2. **Who builds the snapshot?** The CoW Strategic Map module reads the S-1/A1/A4/A5 state and publishes the snapshot types. This module only consumes them (Strategic Map Feature 2).
3. **Camera ownership.** Should `MapCamera` live here (generic map camera) or in the Strategic Map (game-specific feel)? Proposed: the *mechanism* (curves, bounds, API) here; default curve values in the Strategic Map's style file.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Snapshot types and stream registration; spike on snapshot cost | AC 1; ODQ 1 | Not Started | sonnet | |
| 2 | `TerrainMap3DModule`: connect, version gating, world/channel handling | AC 1, 2, 3, 12 | Not Started | sonnet | |
| 3 | Frame loop: tile residency, draw order, post integration | AC 10 | Not Started | sonnet | |
| 4 | `MapCamera` curves and bounds | AC 4, 5 | Not Started | sonnet | |
| 5 | Commands and style binding | AC 6 | Not Started | sonnet | |
| 6 | Presets | AC 7 | Not Started | haiku | |
| 7 | Data views | AC 8 | Not Started | sonnet | |
| 8 | Headless capture command | AC 9, 10 | Not Started | sonnet | |
| 9 | `ScreenToWorld` / `WorldToCell` | AC 11 | Not Started | sonnet | |
| 10 | Observability pillars | AC 13 | Not Started | haiku | |
