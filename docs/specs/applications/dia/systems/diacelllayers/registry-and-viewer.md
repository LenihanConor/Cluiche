# Feature Spec: DiaCellLayers Registry and Viewer

**Parent:** @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md, @docs/specs/applications/dia/systems/diavalueschema/ramps.md, @docs/specs/applications/dia/systems/diabgfx/cell-field-renderer.md (Step 1)
**Mockup:** @docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html

**Gameplay Domains:** debug, spatial

## Summary

The foundation of DiaCellLayers. Systems register **layers** (a named per-cell value array on a grid) and **overlays** (draw callbacks). One viewer then shows exactly one coloured base layer through CellField plus any number of overlays, with a picker, hotkeys, a legend, and a hover tooltip. It uploads to CellField only when the base layer or its values change.

This feature delivers the new module `Dia/DiaCellLayers/`, the `CellLayerRegistry` and its `ServiceStream`, the `CellLayerViewerDomain` (an `IDebugDomain`), the panel card, the commands `cells.layer.list`, `cells.layer.base_by_id` and `cells.overlay.toggle_by_id` (sim-hosted; amended by Streams Migration, which removed the positional `cells.layer.base <id>` / `cells.overlay.toggle <id>`), the legend, the hover tooltip with a shared read-only `CellPicker`, and the hotkey table. Selection (Feature 2), query (3), the inspector (4) and the editor mirror (5) build on it.

## Problem

S-1, A4, A1 and A3 each planned their own tile drawing, colour modes and single-cell view. A shared registry and one viewer means each system only registers what it has, and every testbed shows the same picker, colours and legend. CoW's world-sim stack needs this before any of its layered views can be built.

## Goals

1. **Register, don't draw.** A system registers layers and overlays at start and removes them at stop; it never draws a map tile.
2. **One base, many overlays.** Exactly one coloured base layer at a time (radio); any number of overlays (checkboxes).
3. **Cheap switching.** Switching the base costs at most one read plus one upload; an unchanged layer costs nothing.
4. **Colours from the schema.** Label, unit, range and ramp or palette come from DiaValueSchema, so every surface shows the same colours.
5. **Legend and hover.** A legend for the active base layer, and a tooltip for the cell under the cursor.
6. **Hotkeys.** Layer hotkeys come from each layer's schema and behave the same in every testbed.
7. **Drivable.** Every action is a DiaAPI command, so the panel, the console and later the editor use the same path.

## Non-Goals

- Selection tools, picking-based selection, the query language, the inspector, pin and compare, and the editor mirror (Features 2 to 5). The shared `CellPicker` is built here for hover and reused there.
- Rendering (CellField), history or delta display (DiaCellHistory asks the viewer to show past or delta values; its hook is defined in that spec).
- Cross-PU providers (see Threading).
- Editing cell values.
- Real map art; this is debug-grade display.

## Architecture

New engine module `Dia/DiaCellLayers/` (AD-001, namespace `Dia::CellLayers`), created with `dia scaffold module`, with a module doc. Dependencies: DiaCore, DiaValueSchema, DiaGraphics (CellField registry and draw request), DiaVisualDebugger (`IDebugDomain`), DiaPathfinding (`HexTopology`), DiaAPI. `dia check deps` must pass; no game types appear anywhere (CL-005).

```
Dia::CellLayers
  CellLayerRegistry        layers, overlays, lookup, grid grouping (ServiceStream-published)
  CellLayerViewerDomain    IDebugDomain on SimPU: picker state, upload, draw, legend, hover, commands
  CellPicker               screen point + camera + grid → cell (q, r, index); read-only
  LayerUploader            read → buffer → CellField Update, version gate, observed min/max
  HotkeyTable              ui.hotkey → layer id, per grid; clash handling
```

### Registry

```cpp
CellLayerDesc d;
d.id        = StringCRC("weather.precipitation");
d.schemaKey = StringCRC("cow.weather.cell/precipitation");
d.group     = "Weather";
d.kind      = CellValueKind::Float;            // or Category
d.grid      = &mHexGrid;                        // CellGridDesc (CellField)
d.read      = [this](float* out, uint32 n) { mFields.CopyPrecipitation(out, n); };
d.version   = [this] { return mStepCounter; };
layers.Register(d);
layers.RegisterOverlay(o);   // { id, group, label, draw(IDebugDraw&, const CellSelection*) }
layers.Unregister(id);
```

- **Capacity:** up to 128 layers and 64 overlays (DiaCore containers). A duplicate id or a registration past capacity is rejected with an Error (`CEL`).
- **Grids:** each layer names its grid. The viewer shows the layers of the **active base layer's grid**. A grid selector appears only when more than one grid is registered.
- **Reached through a `ServiceStream`** (CL-006). No singleton.
- **Unregistering the active base layer:** the viewer falls back to the grid's default base (below); if the grid has none left, the map is cleared and the picker shows an empty state.

### Threading

Providers and the viewer share one processing unit, SimPU, in v1: debug drawers already run there, and CellField accepts commands from the simulation thread with copy-on-submit. A registration made from another PU is rejected with an Error naming both. `read` runs directly on the viewer's PU with no locking. The viewer has no mutex at all (amended by Streams Migration: no `StateMutex()`); a `read` delegate or an overlay `draw` callback may call back into the domain, and other threads reach it only through sim-hosted queries/commands and the `debug.domain_state` snapshot (up to one Sim frame old, demand-driven).

### Viewer domain and uploads

`CellLayerViewerDomain` runs each frame on SimPU:

1. **Choose the base.** The active base is the last one chosen; at start, the grid's default is the first layer whose schema has `ui.default_base: true`, else the first registered layer of the grid.
2. **Version gate.** If the base changed, or `version()` differs from the last read, call `read` into the viewer's buffer, compute the **observed** min and max, and submit a full CellField `Update` (copied). Otherwise do nothing.
3. **Colour map.** `ResolveRamp` or `ResolvePalette` (DiaValueSchema Feature 5) gives neutral colour data; the viewer converts it to `CellFieldRampStop` / palette entries and registers it with CellField once per ramp or palette id (idempotent).
4. **Draw.** Submit one `CellFieldDrawRequest` with the layer handle, the colour map id and the schema `min`/`max` as `rampMin`/`rampMax`. Optional schema `ui` keys `inset` and `backdrop` set the inset and backdrop colour.
5. **Hillshade.** If the base layer's schema has `ui.elevation_layer: "<layer id>"`, the viewer keeps that layer uploaded too and sets `elevationLayer` on the request (Relief).
6. **Overlays.** For each enabled overlay, call its `draw` callback with `IDebugDraw` (selection pointer is null until Feature 2).

Only the base layer, and its elevation layer when set, hold CellField layers. Registered layers beyond that cost nothing on the GPU, so the registry's 128 are not limited by CellField's 32. The CellField registry is reached through the kernel's `ServiceStream` (`KernelCellFieldRegistry`, a CluicheGameBaseline glue piece that CellField Step 1 already needs).

### Picker and hotkeys

- **Base:** radio list grouped by `group`; overlays: checkboxes. Labels, units and descriptions come from the schema through the shared tooltip.
- **Numbering:** the picker numbers base layers 1 to 9 within the current group.
- **Hotkeys:** from each layer's schema `ui.hotkey` (one key, unique per grid). A clash logs a Warning (`CEL`) and the later layer gets no hotkey. The table is built from the active grid's layers. A key press runs the panel `base` command, through the panel's key handling. If keyboard input to the panel is not available (the DiaUIUltralight Game Input Bridge is not built), the picker still works by mouse and by console command, and hotkeys are inert. Testbeds never define their own layer hotkeys.

### Legend

The viewer asks DiaValueSchema for `BuildLegend` for the active base layer and passes the result to the panel card:
- **Float layers:** the ramp with schema `min`–`max`, up to 5 ticks, the unit, and the **observed** min and max over all cells at the last read (labelled "observed").
- **Category layers:** one swatch per enum value in order, with label and colour.

### Hover tooltip and `CellPicker`

`CellPicker` turns a screen point into a cell: inverse of the camera transform, then `HexTopology::CellFromPoint`; an out-of-bounds result returns no cell. It is read-only and stateless so Feature 2 reuses it for selection. The viewer reads the cursor position from the panel/game input each frame, picks the cell, reads the base value from the buffer, and sends `(q, r)`, the value with unit and the schema description to the card (rendered by the shared `renderValueTooltip`). Category values show the enum label.

### Panel card and commands

The viewer's `GetJSONState` returns: grids, groups, layers (id, label, unit, hotkey, selected), overlays (id, label, enabled), the legend data and the hover info. The domain is added to the panel contract golden.

| Command | Effect |
|---|---|
| `cells.layer.list` | Returns layers and overlays grouped by grid (JSON) |
| `cells.layer.base_by_id` `{id}` | Makes `id` the active base; changes the active grid if needed (was positional `cells.layer.base <id>`) |
| `cells.overlay.toggle_by_id` `{id}` | Toggles an overlay (was positional `cells.overlay.toggle <id>`) |

All viewer commands are sim-hosted (they run on SimPU through the Sim-Hosted Queries host), not registered in `Dia::API::CommandRegistry`, so they carry no DiaAPI command help metadata.

An unknown id returns an error and changes nothing.

### Logging and observation

- **Channel:** `CEL`.
- **Info:** layer or overlay registered or unregistered, base changed.
- **Warning:** hotkey clash, layer's grid mismatching the active grid, unresolved `schemaKey` (once per key).
- **Error:** duplicate or over-capacity registration, registration from another PU.
- **Traces / Profiling:** `DIA_TRACE_ZONE` per frame update; `DIA_PROFILE_SCOPE` on read and on upload.
- **Metrics:** registered layers, visible layers, reads, uploads, skipped (version-gated) frames, read ms.
- **Health:** `IHealthReporter` degraded when the active base's `read` fails or CellField rejects a command.

## Acceptance Criteria

1. Layers and overlays register and unregister; a duplicate id, a registration past capacity (128 layers, 64 overlays), or one from another PU is rejected with an Error and leaves the registry unchanged.
2. The viewer shows exactly one base layer; `cells.layer.base_by_id` switches it and the active grid follows; an unknown id changes nothing.
3. `read` runs only when the base changed or `version()` changed; a frame with neither does no read and no upload.
4. Switching the base performs at most one read and one upload.
5. The colour map comes from DiaValueSchema: a float layer uses its `ui.ramp` with the schema `min`/`max`; a category layer uses a palette built from the enum values; an unnamed ramp falls back as specified in DiaValueSchema Feature 5.
6. The default base is the first layer with `ui.default_base: true`, else the first layer of the grid; unregistering the active base falls back to the default, or clears the map and shows an empty state when none remain.
7. If the base layer's schema has `ui.elevation_layer`, the draw request carries that layer's handle and hillshade parameters; without it hillshade is off.
8. Enabled overlays' draw callbacks run each frame with `IDebugDraw`; `cells.overlay.toggle_by_id` toggles them.
9. Hotkeys come from `ui.hotkey`, unique per grid; a clash logs a Warning and the later layer gets no hotkey; a key press runs the panel `base` command; with no keyboard path to the panel, mouse and console still work.
10. The picker numbers base layers 1 to 9 within the current group and a grid selector appears only when more than one grid is registered.
11. The legend shows the ramp, ticks, unit and the observed range for float layers, and swatches for category layers.
12. Hovering a cell shows `(q, r)`, the value with its unit, and the schema description (the enum label for category layers); outside the grid shows nothing.
13. `CellPicker` returns the correct cell for every cell centre on a 66 × 42 and a 104 × 64 hex grid under different camera positions and zooms, and no cell outside the grid.
14. At Large (104 × 64), switching the base costs at most one read plus one upload.
15. `DiaCellLayers` has a module doc, `dia check deps` is clean, and no game types or STL appear in public headers.
16. The viewer card is added to the panel contract golden, and the panel page renders it.
17. Visual match against the mockup's picker, legend and hover.

## Test Strategy

- **GoogleTests (TDD):**
  - Registry: register, unregister, duplicates, capacity, other-PU rejection, grid grouping.
  - Version gate: no read when version and base unchanged; one read on change.
  - Colour maps from schema fixtures (float, category, unknown ramp).
  - Default base and unregister fallback.
  - Hotkey table: unique, clash, 1 to 9 numbering.
  - Legend data for float and category layers.
  - `CellPicker`: cell centres, edges and out-of-bounds, for both grid sizes and several cameras.
  - Commands: `list`, `base`, `toggle`, unknown ids.
  - Uploader against a fake CellField registry (records `Update` calls and the draw request).
- **Panel contract golden:** the viewer card JSON; the page tests feed the same file.
- **Test utilities** live in `DiaCellLayers/Testing/`: a fake CellField registry, a stub provider with a settable version, and fixture schema entries.
- **Perf:** base switch at Large costs one read plus one upload (counters), per frame cost with no change is near zero.
- **Visual gate:** the S-1 or S-3 testbed against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CL-001 | Systems register layers; one Dia viewer draws them | Registry plus `CellLayerViewerDomain` |
| CL-002 | Exactly one base, any number of overlays | Radio base, checkbox overlays |
| CL-005 | Generic, no game types | No CoW types; CoW content is registration only |
| CL-006 | No singletons | Registry is a `ServiceStream` |
| AD-001 (Dia) | New module has a module doc and sign-off for shared build edits | `dia scaffold module`; solution and registry edits via the scaffold flow |
| AD-003 (Dia) | Namespace | `Dia::CellLayers` |
| PD-004 | No STL in public APIs | DiaCore containers |
| RB-006 (CellField) | No backend types in public headers | Uses the backend-agnostic registry and POD draw request only |
| Game logic on SimPU | Providers and viewer on SimPU | Same-PU contract |
| Panel contract golden | Panel JSON and page share golden files | Viewer card added |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| CellField Step 1 | Registry, draw request, hillshade, `KernelCellFieldRegistry` | Approved, plan written, not built |
| DiaValueSchema Features 1, 3, 5 | Labels, units, tooltip, ramps, legend | Approved, not built |
| DiaVisualDebugger `IDebugDomain`, panel | Domain and card | Done |
| `HexTopology` (DiaPathfinding) | Picking | Done |
| DiaUIUltralight Game Input Bridge | Hotkeys, cursor position | Spec only; hotkeys inert until built |
| DiaAPI | Commands | Done |

## Open Design Questions

1. **`ui.elevation_layer`.** The Relief hillshade needs an elevation layer, and the schema key that names it is new here. Confirm the key with S-1's layered-debug-rendering amendment, which defines what Relief reads.
2. **Cursor and camera access.** `CellPicker` needs the mouse position and the camera in the domain's update. Check how existing debug domains get them (panel input or the game's input and camera services) when this is planned, and follow that path.
3. **History and delta display (resolved 2026-10-20, DiaCellHistory Feature 2).** The viewer owns an optional `IViewValueSource` (`GetValues(layer, out, n)`, `IsLive()`, `ViewEpoch()`). `LayerUploader` asks the installed source first; while it reports live, or when no source is installed, it uses the layer's own `read`. While not live, `GetValues` returning false means "no data in this view", never a fallback to live values. At most one source can be installed (a second is rejected with an Error). The viewer's cached reads are keyed by the layer's `version()` and `ViewEpoch()`. The registry service also exposes `IsLive()` for systems that must refuse edits while a past sample is shown. DiaCellHistory implements the source. The interface has one more optional method, `GetPresentation(layer, ViewPresentation&)` (default: returns false), through which a source supplies a ramp id, value range and unit prefix; the viewer then uses them instead of the schema's colouring and the legend shows the prefix (added 2026-10-20 for DiaCellHistory Compare's zero-centred delta ramp and "Δ" unit).

## Status

`Done` (2026-10-28). Plan: [registry-and-viewer.plan.md](registry-and-viewer.plan.md). Built: registry, uploader, picker, hotkeys, legend, viewer domain, panel card (CluicheTest and CoW panels), `CellLayersModule` (ServiceStream), `IViewValueSource`, JSON commands for E2E, CellLayersViewer test stage. Known gaps: per-layer info icon in the picker (layer JSON has no description); `noData` message not on the panel pages; panel state not assertable over E2E (no DebugServer domain-state query).
