# Feature Spec: Strategic Map Module and Camera

## Parent System
@docs/specs/applications/cow/systems/strategicmap/strategicmap.md (Feature 4)

**Gameplay Domains:** rendering, camera, debug
**Depends on:** Strategic Map Features 1 and 2; DiaTerrainMap3D Module, Presentation and Tooling (`map3d.*`, `MapCamera`)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (input handlers, `camFor`, state, UI spec), `lookdev/README.md`

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

The CoW-side composition: a `StrategicMapModule` that wires the data (Features 1 and 2) to DiaTerrainMap3D, owns the **style parameter file** (load, save, reload), exposes the **`cow.map.*` commands** every tool and panel uses, and sets the **strategic camera defaults**. This is the system's tooling (SM-005): every look-dev control is a command, and tuned values persist.

## Problem

- The renderer (`map3d.*`) is generic. CoW needs its own entry points that load CoW's visual data, apply CoW's presets and camera feel, and persist tuning to CoW's files.
- The look-dev panel (V-1) and later the real game need one way to change the map, not two.
- Tuned values are art direction. They must save to a reviewable file and survive restarts.

## Goals

1. **`StrategicMapModule`** (render side): loads `map_visuals.json` and `map_style.json`, applies them to DiaTerrainMap3D, reacts to hot reload, and stops cleanly.
2. **Commands (DiaAPI)**: a `cow.map.*` set that wraps `map3d.*` with CoW's data:
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

`StrategicMapModule : Module` on the render PU, `consumes`: the terrain streams (published by `MapSnapshotPublisher`, Feature 2), `provides`: nothing. On start: resolves `ModuleRef<TerrainMap3DModule>`, loads visuals and style (a missing file falls back to compiled defaults with a warning), pushes them in. On `FileWatcher` events, reloads and swaps atomically between frames.

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
| `cow.map.view.data {name}` | Data views (class, elevation, condition, zone, temperature, margin, tiles) |
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
7. **Presets.** `preset.apply` calls the DiaTerrainMap3D preset path and the resulting style equals the preset file applied over defaults.
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
| AD-005 | One stage per testbed | Registered in `cow_main.diaapp` and the V-1 stage |
| PD-001 | StringCRC | Command and stream ids |
| PD-004 | No STL in public APIs | Typed argument structs and raw buffers |

## Open Design Questions

1. **Which `--source` file wins?** If a preset file and `map_style.json` both change a key, the preset applies on top at runtime but `style.save` writes the diff against *defaults*, which would bake the preset into the base file. Proposed: `save` diffs against the *currently applied preset*, and `save {name}` writes a named preset file; confirm.
2. **Camera input ownership.** The real game's input handling is F-1/V-4 work. For v1 the module exposes `Pan/Zoom/Orbit` and the V-1 panel (or a simple debug input module) calls them. Is a default input binding module in scope here? Proposed: a minimal `MapDebugInputModule` for testbeds only, with the real input in V-4.
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
