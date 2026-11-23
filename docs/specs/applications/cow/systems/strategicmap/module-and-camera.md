# Feature Spec: Strategic Map Module and Camera

## Parent System
@docs/specs/applications/cow/systems/strategicmap/strategicmap.md (Feature 4)

**Gameplay Domains:** rendering, camera, debug
**Depends on:** Strategic Map Features 1 and 2; DraoiHexWorldRender3D (@docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md, `HexWorldMap`, Map Assembly); DiaCellFieldBake and DraoiHexWorldRelief (bake jobs); DiaBgfx3D Feature A (capture readback)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (input handlers, `camFor`, state, UI spec), `lookdev/README.md`

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

### Amendment 2026-11-23: layer split (user)

DiaTerrainMap3D is gone. Its renderer is now the Draoi library DraoiHexWorldRender3D, which has no module, streams or commands (TMAP-011). Everything wired that used to sit in `TerrainMap3DModule` (old DiaTerrainMap3D Feature 4) now lives in this feature, and the `map3d.*` command layer disappears: `cow.map.*` commands call `HexWorldMap` directly. Text below that still says `map3d.*`, `TerrainMap3DModule` or "DiaTerrainMap3D" means this module calling `HexWorldMap`.

**Taken over from the old DiaTerrainMap3D Feature 4:**
1. **Owning the map.** `StrategicMapModule` owns a `Draoi::HexWorldRender3D::HexWorldMap` and the DiaBgfx3D sprite renderer, particle system and post stack it draws with. No `ModuleRef<TerrainMap3DModule>`.
2. **Bake jobs.** On a world snapshot, run DiaCellFieldBake `BakedWorld::Bake` with the DraoiHexWorldRelief post-pass on a worker job (never the render thread), report progress into `HexWorldMap::SetBakeState`, and call `SetWorld` when done. On a class change, `RebakeBegin` on a worker, then `Commit` and `HexWorldMap::ApplyRebake` at a frame boundary.
3. **Stream contract.** The world and channel snapshots published by Feature 2 (`MapSnapshotPublisher`) are consumed here; versions pass through to `HexWorldMap::UpdateChannel`. Old ODQ 1 (snapshot size and cadence: about 150 KB world, 106 KB channel at Large; FrameStream copies by value, so either size the struct for the largest world or use a pointer-free blob on a ServiceStream) is now **ODQ 4 below**.
4. **Map camera mechanism.** Zoom-dependent `pitch(z)` and `heightScale(z)` curves (smooth, monotonic, continuous), bounds clamp (`BoundsClamp3D`), smooth damping, on `DiaCamera3D` Orbit. The height scale is passed to `HexWorldMap::Draw` each frame. (Was `MapCamera` in DiaTerrainMap3D; one consumer, so it stays in CoW until a second game needs it.)
5. **Capture.** `cow.map.capture {path, w, h, frames, seed}` renders `frames` frames at a fixed time, boil frame and seed, then reads the scene target back through DiaBgfx3D Feature A to a PNG. Two runs with the same arguments give byte-identical PNGs; a capture with post on matches the screen.
6. **Status.** `cow.map.status` reports world, counts, versions and the bake state (Idle / Baking n of m / Ready) from `HexWorldMap::GetBakeState`.
7. **Custom slots.** The module fills the 4 custom slots from CoW-only states (for example magic snow) through Feature 2's binding, and the style file binds each slot to a DiaToonShading technique.
8. **Health.** The `IHealthReporter` also reports Failing when the world snapshot is invalid or GPU resources fail.

**Additional acceptance criteria:**
- **AC 13, bake off the render thread.** A world snapshot never blocks a render frame for the bake (frame time during a Large bake stays within budget); the bake state goes Idle → Baking → Ready; a rebake while one is pending merges.
- **AC 14, camera curves.** `pitch(z)` and `heightScale(z)` are monotonic and continuous; at `zMin` and `zMax` they equal the near and far values; panning cannot leave the world rectangle plus margin.
- **AC 15, capture determinism** as in item 5.
- **AC 16, lifetime.** Module stop releases the `HexWorldMap` and every GPU resource; 20 start/stop cycles leak nothing.

**Additional tasks:** 9 bake jobs and frame-boundary commit (AC 13, sonnet); 10 camera curves and bounds (AC 14, sonnet); 11 capture command (AC 15, sonnet); 12 lifetime and health (AC 16, haiku).

## Summary

The CoW-side composition: a `StrategicMapModule` that owns the DraoiHexWorldRender3D `HexWorldMap`, wires the data (Features 1 and 2) to it, runs the bake jobs, owns the **style parameter file** (load, save, reload), exposes the **`cow.map.*` commands** every tool and panel uses, and sets the **strategic camera defaults**. This is the system's tooling (SM-005): every look-dev control is a command, and tuned values persist.

## Problem

- The renderer (DraoiHexWorldRender3D) is a genre library with no commands of its own. CoW needs its own entry points that load CoW's visual data, apply CoW's presets and camera feel, and persist tuning to CoW's files.
- The look-dev panel (V-1) and later the real game need one way to change the map, not two.
- Tuned values are art direction. They must save to a reviewable file and survive restarts.

## Goals

1. **`StrategicMapModule`** (render side): loads `map_visuals.json` and `map_style.json`, applies them to the `HexWorldMap`, reacts to hot reload, and stops cleanly.
2. **Commands (DiaAPI)**: a `cow.map.*` set that drives `HexWorldMap` with CoW's data:
   - `cow.map.style.set/get/save/load/reload/diff`
   - `cow.map.preset.apply/list`
   - `cow.map.camera.set/get/home/focus_cell`
   - `cow.map.layer.toggle`, `cow.map.view.data`
   - `cow.map.capture`
   - `cow.map.status`, `cow.map.visuals.reload`
3. **Style persistence**: save writes only values that differ from defaults (small diffs), atomically, to the dev source file when launched with `--source`, else to the user data folder.
4. **Strategic camera**: CoW's default pitch/height/zoom curves and bounds, Home, and focus on a cell (smooth).
5. **Keyboard and mouse mapping** hooks for the real game (pan, zoom, orbit), configured in data; the V-1 panel can bind its own.
6. **Registration in `cow_main.diaapp`** (game) and the testbed manifests; `dia validate manifest` passes.

## Non-Goals

- The look-dev panel UI (V-1 Feature 3).
- Selection, hover, picking highlight (DiaCellLayers and the game's input module). `focus_cell` only moves the camera.
- Save-game of the camera or style (the style file is project data, not a save).
- Overlays, icons, fog of war (V-2/V-3).

## Design

### Module

`StrategicMapModule : Module` on the render PU, `consumes`: the terrain streams (published by `MapSnapshotPublisher`, Feature 2), `provides`: nothing. On start: creates the `HexWorldMap`, loads visuals and style (a missing file falls back to compiled defaults with a warning), pushes them in. On `FileWatcher` events, reloads and swaps atomically between frames.

### Commands

All commands register through the existing registry with help text and ranges from the style sidecar. They are **render-side** commands; none needs SimPU state. A summary:

| Command | Effect |
|---|---|
| `cow.map.style.set {path, value}` | Sets one style value (dotted path, e.g. `ink.weight`); clamps and warns out of range |
| `cow.map.style.get {path?}` | Returns a value or the whole style |
| `cow.map.style.save {name?}` | Writes the diff vs defaults to the style file (or `map_presets/<name>.json`) |
| `cow.map.style.load / reload` | Loads the file; reload re-reads the current one |
| `cow.map.style.diff` | Lists values that differ from defaults |
| `cow.map.preset.apply {name}` / `list` | Applies a named preset (Feature 5) |
| `cow.map.camera.set / get / home / focus_cell {cell}` | Camera control; focus eases over 0.4 s |
| `cow.map.layer.toggle {name, on}` | Terrain, props, particles, post, ink, hatch… |
| `cow.map.view.data {name}` | Data views (class, elevation, condition, stratum, temperature, margin, each channel, each custom slot) |
| `cow.map.capture {path, w, h, frames, seed}` | Headless capture |
| `cow.map.visuals.reload` | Re-reads `map_visuals.json` |

The V-1 panel posts exactly these commands (feedback: panel calls the same commands).

### Style persistence

- **Source of truth for edits:** `Cluiche/Assets/CoW/Global/Map/map_style.json` when launched with `--source <path>` (dev). Without it, writes go to `<userdata>/CoW/map_style.user.json` (never into the deployed assets).
- **Diff only:** `save` stores keys whose value differs from the compiled/sidecar default, so the file stays small and merges cleanly in version control.
- **Atomic and safe:** write to a temp file and rename; on failure keep the old file and return the error.

### Strategic camera defaults (in `map_style.json`)

`pitchNear 55°`, `pitchFar 70°`, `heightFactorFar 0.6`, `zoomMin`, `zoomMax` by world size, `panBoundsMargin`, damping. Values are the prototype's, tunable.

## Acceptance Criteria

1. **Load.** With valid files the module starts, applies visuals and style, and the map draws; with a missing style file it falls back to defaults with one warning; with an invalid file it keeps the previous data (or defaults on first load), logs the error and reports Degraded.
2. **Commands registered.** Every command above is registered, with help text and arguments typed from the schema (`dia query` help lists them); calling with a bad argument returns a typed error and changes nothing.
3. **Set/get.** `style.set` then `style.get` returns the value; out-of-range clamps with a warning; the next frame reflects it.
4. **Save diff.** After changing three values and `style.save`, the file contains exactly those three keys; `style.load` in a fresh start reproduces the same style (hash equal); saving with no changes writes an empty diff.
5. **Safe write.** A simulated write failure (read-only target) leaves the previous file untouched and returns an error.
6. **Dev vs deployed.** Without `--source`, saves go to the user data folder and never modify `Assets/`.
7. **Presets.** `preset.apply` applies the preset file over defaults and passes the result to `HexWorldMap::SetStyle`; the resulting style equals the preset file applied over defaults.
8. **Camera.** `home` frames the whole world; `focus_cell` ends centred on that cell within 0.5 s; `camera.get` after `camera.set` returns the same values (within float error); pan and zoom respect the bounds.
9. **Hot reload.** Editing `map_visuals.json` or `map_style.json` in a dev launch updates the map between frames without a restart; a bad edit keeps the previous data.
10. **Manifest.** `cow_main.diaapp` and the V-1 stage manifest list the module in the right order and validate; `requires_tick_after` constraints (if any) hold.
11. **Panel parity.** Every control in the V-1 look-dev panel maps to exactly one command (a table test at V-1 Feature 3).
12. **Observability.**
    - **Logs:** channel `MAP`; info on load, reload, preset, save; warnings on clamps; errors on parse failure.
    - **Profile:** scope `StrategicMap.Frame`.
    - **Metrics:** `map.style_hash`, `map.reloads`.
    - **Health:** `IHealthReporter` Failing if no valid style is loaded, Degraded if the last reload failed.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SM-005 | Tooling ships with the system; every control is a `cow.map.*` command; values persist in a style parameter file | This feature |
| Feedback: editor actions via DiaAPI | Every action and query is a command; the panel calls the same | Commands first, panel later |
| Feedback: UI separate from business logic | Typed command model first | Commands and style structs exist without a UI |
| AD-001 | Engine changes need sign-off | None: this is CoW-Shared |
| TMAP-011 | DraoiHexWorldRender3D is a plain library | All module, stream, command, camera and capture wiring is here |
| AD-005 | One stage per testbed | Registered in `cow_main.diaapp` and the V-1 stage |
| PD-001 | StringCRC | Command and stream ids |
| PD-004 | No STL in public APIs | Typed argument structs and raw buffers |

## Open Design Questions

1. **Which `--source` file wins?** If a preset file and `map_style.json` both change a key, the preset applies on top at runtime but `style.save` writes the diff against *defaults*, which would bake the preset into the base file. Proposed: `save` diffs against the *currently applied preset*, and `save {name}` writes a named preset file; confirm.
2. **Camera input ownership.** The real game's input handling is F-1/V-4 work. For v1 the module exposes `Pan/Zoom/Orbit` and the V-1 panel (or a simple debug input module) calls them. Is a default input binding module in scope here? Proposed: a minimal `MapDebugInputModule` for testbeds only, with the real input in V-4.
4. **Snapshot size and cadence** (moved from the old DiaTerrainMap3D Feature 4 ODQ 1). The world snapshot is about 150 KB at Large and the channel snapshot about 106 KB. FrameStream copies a fixed-size struct by value, so either size the struct for the largest world or carry a pointer-free blob on a ServiceStream. Decide with a spike (`StreamWriter::Write` cost with a 256 KB `T`).
5. **Selection in Map view (decided 2026-11-22: solve now; moved from DiaTerrainMap3D).** The game needs 3D selection as much as 2D, so Map view supports clicking a cell and hover from the first version. One selection state, two pick paths: the flat 2D conversion in Data view, and `HexWorldMap::ScreenToWorld` + `WorldToCell` in Map view. Still to design: routing through `DiaPicking` (`PickRouter`) and the DiaCellLayers `SelectionInput`, and how the selected cell and hover highlight are drawn (system ODQ 1 of DraoiHexWorldRender3D).
6. **Markers and regions (raised 2026-11-22; postponed; moved from DiaTerrainMap3D).** Site markers and region borders are flat 2D debug-draw shapes in the S-1 Data view today and will not line up on a tilted 3D heightfield. **Postponed:** no markers or regions in Map view v1; they stay Data-view-only. Open: lift onto the surface, billboard, or the sprite path; regions as terrain ink, a border line or a tint; whether the engine has a 3D debug-draw path. **Gate:** an HTML mockup of Map view with markers and regions, signed off by the user, before it is specced. Tracked in the backlog.
7. **Map view in the S-1 testbed (decided 2026-11-22; moved from DiaTerrainMap3D).** The S-1 World Generation testbed gets a Data | Map view switch (`IWorldGenView` mode; `cow.worldgen.view` gains the active mode). Data view needs no bake. The first switch to Map bakes with a progress overlay. Edits made while in Map view bake a low-res preview during a slider drag and the full bake on release, with a "baking" badge. The bake state is a DiaAPI status (`cow.map.status`). The module needs a CPU copy of the per-cell layers: `CellFieldRegistry` hands values to the texture upload only, so a drain tap is needed (an AD-001 sign-off for `DiaGraphics/CellField`), plus a layer-to-role mapping owned by the Feature 1 visual table.
3. **Window and DPI.** The prototype sized the canvas to the window with devicePixelRatio. The scene target follows the backbuffer size; confirm resize behaviour with DiaBgfx3D Feature A.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `StrategicMapModule` load/apply/hot reload | AC 1, 9 | Not Started | sonnet | |
| 2 | Style path set/get + clamp | AC 3 | Not Started | sonnet | |
| 3 | Persistence: diff, atomic write, dev vs deployed | AC 4, 5, 6 | Not Started | sonnet | |
| 4 | Command registration with schema help | AC 2 | Not Started | sonnet | |
| 5 | Presets hook | AC 7 | Not Started | haiku | needs Feature 5 files |
| 6 | Camera defaults, home, focus | AC 8 | Not Started | sonnet | |
| 7 | Manifest entries and validation | AC 10 | Not Started | haiku | `dia validate manifest` |
| 8 | Observability | AC 12 | Not Started | haiku | |
