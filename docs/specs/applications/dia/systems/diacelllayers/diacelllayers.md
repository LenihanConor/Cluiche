# System Spec: DiaCellLayers

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, spatial

**Origin:** 2026-10-19 world-sim product review. The outcome is "a hex map with multiple layers I can see in debug, one at a time", plus multi-cell inspection. Before this, S-1, A4 and A1 each specced their own tile drawing, colour modes and single-cell inspector.

**Mockup:** [mockups/cell-layer-viewer.html](mockups/cell-layer-viewer.html)
**Sibling:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md (recording, scrubbing, snapshots and compare draw through this viewer)

## Purpose

DiaCellLayers is the one place any game shows per-cell data on a grid map. Systems **register layers**. One **viewer** then provides:
- a picker with exactly one coloured base layer at a time
- any number of line or marker overlays
- a legend and tooltips (from DiaValueSchema)
- multi-cell selection (click, shift-click, box, lasso, region, biome, query)
- a selection inspector with stats for every layer and A-vs-B compare

It draws with CellField. Nothing in it is game-specific: CoW registers terrain, weather and ecology layers, and another game would register its own.

## Responsibilities

- `CellLayerRegistry`: layer providers register a layer id, a `schemaKey`, a value kind (float or category), a grid and a read function. Overlay providers register an overlay id and a draw callback.
- `CellLayerViewerDomain` (an `IDebugDomain`):
  - owns the picker and the current base and overlay state
  - draws the legend
  - uploads to CellField only when the base layer or its values change
- Selection model: `CellSelection` (a cell set), the tools, query parsing and evaluation, pinned selection A.
- Aggregates: per-layer stats for a selection, a histogram for the active layer, category counts.
- DiaAPI commands for every action, so the panel, the console and the editor can all drive it.
- DiaDebugServer topics and queries, so the World Sim Inspector can mirror layers and selection.

## Non-Responsibilities

- **Rendering.** CellField draws; DiaCellLayers decides what to draw.
- **The meaning, label, unit and ramp of a value.** These come from DiaValueSchema (`ui.ramp`, `label`, `unit`, `description`).
- **History.** DiaCellHistory records layers and asks the viewer to show past or delta values.
- **Real map art.** Debug-grade only; game visuals (V-1) are separate.
- **Editing cell values.** Systems keep their own edit commands (A1 `set_field`, paint); the viewer passes the selection to them.

## Design

### Layers and overlays

```cpp
// registered by the owning system at Start, unregistered at Stop
CellLayerDesc d;
d.id        = StringCRC("weather.precipitation");
d.schemaKey = StringCRC("cow.weather.cell/precipitation");   // label, unit, ramp, range
d.group     = "Weather";
d.kind      = CellValueKind::Float;                           // or Category (palette from schema enum)
d.grid      = &mHexGrid;                                      // CellGridDesc (CellField)
d.read      = [this](float* out, uint32 n) { mFields.CopyPrecipitation(out, n); };
d.version   = [this]{ return mStepCounter; };                 // viewer re-reads only when this changes
layers.Register(d);

CellOverlayDesc o;
o.id = StringCRC("weather.wind"); o.group = "Weather"; o.label = "Wind arrows";
o.draw = [this](IDebugDraw& dd, const CellSelection*) { DrawWindArrows(dd); };
layers.RegisterOverlay(o);
```

- The registry is reached through a `ServiceStream`; no singletons.
- `read` copies into a buffer the viewer owns. It runs on the provider's PU (SimPU), only when `version` changed and the layer is visible or being aggregated.
- **Grids:** every layer names its grid. The viewer shows the layers of one grid at a time (the grid of the active base layer).
- **Limits:** CellField's 32 GPU layers are a cache. The registry itself holds up to 128 layers (DiaCore container), and only visible layers occupy a CellField layer.

### Picker semantics

- **Base:** exactly one layer (radio). The default is the grid's first layer whose schema has `ui.default_base: true`. CoW uses Relief.
- **Overlays:** any number (checkbox): wind arrows, region edges, sites, grid lines, selection outline.
- **Overlays from the source specs:**
  - A4's "any number of overlays on at once" now applies to its vector and marker overlays only.
  - A4's scalar fields are base layers.
  - S-1's `ColourMode` enum and A1's overlay list become registered layers.
- **Hotkeys** (amended 2026-10-20, GDD fold-back): layer hotkeys come from each layer's schema `ui.hotkey` (one key, unique per grid). The viewer owns them and uses them in every testbed. A clash logs a Warning (`CEL`), and the later layer gets no hotkey. Testbeds never define their own layer hotkeys. The picker also numbers base layers 1–9 within the current group. Because the picker is shared, every registered layer appears in every testbed that loads its system: S-3 Living mode shows Ground Water too.
- **Legend:** a ramp with schema `min`–`max`, the unit and the "seen in view" range, or a list of category swatches. It is built from DiaValueSchema, so every surface shows the same colours.
- **Tooltip on hover:** cell `(q, r)`, the base value with its unit, and the schema description (shared `renderValueTooltip`).

### Selection

| Tool | Gesture | Command |
|---|---|---|
| Click | click a cell (replaces the selection) | `cells.select.cell <q> <r>` |
| Add/remove | shift-click | `cells.select.toggle <q> <r>` |
| Box | drag | `cells.select.box <q0> <r0> <q1> <r1>` |
| Lasso | alt-drag | `cells.select.poly <q,r…>` |
| Region / biome | pick from list, or ctrl-click a cell | `cells.select.category <layerId> <value>` (any category layer) |
| Query | type an expression | `cells.select.query "<expr>"` |
| Cell list | programmatic (editor inspectors, e.g. T-1 "show cells") | `cells.select.cells <q,r…>` |
| Clear | Esc | `cells.select.clear` |

- **Query language** (small, no scripting):
  - `<layer> <op> <number>` with `< <= > >= == !=`
  - `<categoryLayer> = <EnumId>`
  - joined with `and`, `or`, `not` and parentheses
  - Layer names are the registered ids or their short schema keys.
  - Errors return a message and leave the selection unchanged.
- **Cell list** (added 2026-10-20, T-1 Navigation): `cells.select.cells` replaces the selection with exactly the listed cells. Out-of-bounds cells are ignored. The game enforces a size limit (set in Feature 2) and the result returns the accepted set, so callers can show "Selected N of M". Feature 2 may also consider applying a navigation plan (history view, selection, base layer) in one frame; this is a request from T-1, not a requirement.
- **Picking:** screen → camera inverse → `HexTopology::CellFromPoint`. An out-of-bounds result is ignored.
- **Pin and compare:** `cells.select.pin` stores the current selection as **A**. While A is pinned, the inspector shows A's stats beside the current selection's, plus a delta column.
- **Hand-off to systems:** providers can read the current selection, e.g. so A1's paint can apply a value to the whole selection.

### Selection inspector

- **For every registered layer on the grid:** count, min, mean and max with units (i).
- **For category layers:** counts per value.
- **For the active base layer:** a histogram with 16 bins over the schema range.
- **Single-cell selection:** the same table with exact values, plus a "details" slot that providers fill (e.g. A1's ledger and S-1's biome scores) through an optional `describeCell` callback.

Aggregates are computed on the provider's PU, on demand (when the selection or version changes), and are capped at one recompute per frame.

### Commands, topics, queries

- **Commands:**
  - `cells.layer.list`, `cells.layer.base <id>`, `cells.overlay.toggle <id>`
  - the selection commands above
  - `cells.select.pin`, `cells.select.unpin`, `cells.inspect`, which returns the stats JSON
- **DiaDebugServer:**
  - topic `cells.view.state` (change-detected): grid, base, overlays, selection summary
  - query `cells.layer.values {layer}`: values for the editor mirror, chunked under 1 MB
  - query `cells.select.stats`

### Logging and observation

- **Channel:** `CEL`.
- **Info:** layer registered/unregistered, base changed.
- **Warning:** query parse errors, a layer's grid mismatching the active grid.
- **Profiling:** `DIA_PROFILE_SCOPE` on read, upload and aggregate.
- **Metrics:** visible layers, selection size, aggregate ms.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Registry and viewer | `CellLayerRegistry` (service), `CellLayerViewerDomain`, base and overlay picker, legend, hover tooltip, CellField upload on version change; shared read-only `CellPicker` for hover (reused by Feature 2); hotkeys | Approved | [registry-and-viewer.md](registry-and-viewer.md) |
| 2 | Selection | `CellSelection`, click/shift/box/lasso/category/cell-list, commands (picking is built in Feature 1) | Approved | [selection.md](selection.md) |
| 3 | Query | Expression parser and evaluator, `cells.select.query`, `cells.query.check` (preview), error reporting | Approved | [query.md](query.md) |
| 4 | Selection inspector | Per-layer stats, category counts, histogram, pin A and compare (live or frozen), `describeCell` slot, `cells.inspect` | Approved | [selection-inspector.md](selection-inspector.md) |
| 5 | Editor mirror | `cells.view.state` topic, `cells.layer.values` and `cells.select.stats` queries (plus `cells.layer.list` and `cells.select.get`, selection hash), for the World Sim Inspector and the World Data Editor | Approved | [editor-mirror.md](editor-mirror.md) |

- **Build order:** 1 → 2 → 4 → 3 → 5. Feature 1 needs CellField Step 1 and DiaValueSchema Features 1 and 5 (ramps).
- **CellField alignment:** write the CellField Step 1 plan after this spec is Approved, so its registry and draw request fit a viewer-owned upload. This spec needs no CellField API change; it only consumes `CreateLayer`, `Update`, `RegisterRamp` / `RegisterPalette` and `RequestDrawCellField`.

## Consumer amendments (applied when Approved)

- **S-1 F7 (layered-debug-rendering):** the tile layer and `ColourMode` become registered layers. Region edges, sites, grid and selection become overlays. `selectCell` becomes `cells.select.cell`.
- **A4:** the seven scalar overlays become base layers and wind becomes an overlay. The "any number on" rule applies to overlays only.
- **A1 F9:** the ecology overlays become layers. The inspector's single-cell view uses `describeCell` (ledger), and paint can apply to the selection.
- **A3:** registers `succession.*` layers and emits transition and recovery events (amended 2026-10-19, consistency pass).
- **S-2.5, S-3:** they host the viewer card. The per-system tile drawers are removed from their specs.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| CellField (DiaBgfx / DiaGraphics) | Drawing, palettes, ramps | Approved, not built |
| DiaValueSchema | Labels, units, ramps, tooltips, legend | Approved, not built |
| DiaVisualDebugger `IDebugDomain`, `DebugLayerManager`, panel | Domain, panel card | Done |
| `HexTopology` (DiaPathfinding) | Picking, neighbours | Done |
| DiaAPI, DiaDebugServer | Commands, topics, queries | Done |

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| CL-001 | Systems register layers; one Dia viewer draws them (user decision 2026-10-19) | Accepted | Yes |
| CL-002 | Exactly one coloured base layer, plus any number of overlays (user decision 2026-10-19) | Accepted | Yes |
| CL-003 | v1 selection: click, shift, box, lasso, category (region or biome) and query (user decision 2026-10-19) | Accepted | Yes |
| CL-004 | The inspector shows stats for every layer, with pin-and-compare (user decision 2026-10-19) | Accepted | Yes |
| CL-005 | Generic and named for cells: no game types; CoW content is registration only | Accepted | Yes |
| CL-006 | No singletons: the registry is a `ServiceStream` | Accepted | Yes |

## Test Strategy

- **GoogleTests:**
  - Register and unregister layers.
  - A version gate skips the read.
  - Category layers take their palette from the schema enum.
  - Each selection tool on a 66×42 hex grid.
  - The query parser covers each operator, precedence and error messages.
  - Aggregates on known data (min, mean, max, histogram bins, category counts).
  - Pin and compare deltas.
- **Panel contract golden:** the viewer card JSON.
- **Visual gate:** the S-3 testbed against the mockup.
- **Perf:** at Large, switching the base layer costs at most one read plus one upload. A selection aggregate over all cells and 20 layers takes ≤ 2 ms in Release.

## Resolved Decisions

1. **Several grids: follow the base layer** (2026-10-19). The picker shows the layers of the active base layer's grid. A grid selector appears only when more than one grid is registered.
2. **No arithmetic in queries in v1** (2026-10-19). Comparisons joined with `and`, `or` and `not` cover S-3's scenario checks. Arithmetic is added only when a scenario needs it.

## Status

`Approved` (2026-10-19). All 5 feature specs `Approved` (2026-10-20); plans written 2026-10-29 for Features 2 to 5 (Feature 1 Done).
