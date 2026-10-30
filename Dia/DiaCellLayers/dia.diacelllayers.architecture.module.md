---
schema: dia.module.v1
module_id: dia.celllayers
name: DiaCellLayers
path: Dia/DiaCellLayers
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: stub
version: "1.0"
dependencies:
  required:
    - dia.core
    - dia.valueschema
    - dia.graphics
    - dia.visualdebugger
    - dia.pathfinding
    - dia.api
    - dia.observation
    - dia.applicationflow
    - dia.streams
    - dia.debugserver
  forbidden: []
public_api:
  headers:
    - DiaCellLayers/CellLayersTypes.h
    - DiaCellLayers/CellLayerRegistry.h
    - DiaCellLayers/IViewValueSource.h
    - DiaCellLayers/HotkeyTable.h
    - DiaCellLayers/CellLayerLegend.h
    - DiaCellLayers/CellLayerViewerDomain.h
    - DiaCellLayers/CellLayersModule.h
    - DiaCellLayers/CellSelection.h
    - DiaCellLayers/ICellSelectionView.h
    - DiaCellLayers/LayerReader.h
    - DiaCellLayers/SelectionTools.h
    - DiaCellLayers/SelectionOutline.h
    - DiaCellLayers/SelectionInput.h
    - DiaCellLayers/SelectionStats.h
    - DiaCellLayers/InspectorState.h
    - DiaCellLayers/InspectorModel.h
    - DiaCellLayers/SelectionFingerprint.h
    - DiaCellLayers/ITopicSink.h
    - DiaCellLayers/DebugServerTopicSink.h
    - DiaCellLayers/ViewStatePublisher.h
    - DiaCellLayers/MirrorQueries.h
    - DiaCellLayers/CellDetailRegistry.h
    - DiaCellLayers/QueryError.h
    - DiaCellLayers/QueryLexer.h
    - DiaCellLayers/QueryParser.h
    - DiaCellLayers/QueryResolver.h
    - DiaCellLayers/QueryEvaluator.h
    - DiaCellLayers/Testing/ViewStateTestSupport.h
  namespaces:
    - Dia::CellLayers::
  entry_points:
    - CellValueKind
    - CellLayerRegistry
    - IViewValueSource
    - ViewPresentation
    - PresentedLegend
    - HotkeyTable
    - CellLayerLegend
    - CellLayerViewerDomain
    - CellLayersModule
    - FindCellLayerRegistry
    - CellSelection
    - ICellSelectionView
    - LayerReader
    - SelectionTools
    - SelectionOutline
    - SelectionInput
    - SelectionStats
    - InspectorState
    - InspectorModel
    - SelectionFingerprint
    - ViewStatePublisher
    - ITopicSink
    - DebugServerTopicSink
    - MirrorQueries
    - CellDetailRegistry
    - QueryError
    - QueryLexer
    - QueryParser
    - QueryResolver
    - QueryEvaluator
responsibilities:
  - Per-cell layer registry for generic layer management
  - Viewer integration via CellField display
  - Debug domain for layer visualization and interaction
  - Cell picker and selection utilities
  - Hotkey bindings for layer control
  - Legend generation and display
non_responsibilities:
  - CellField rendering (handled by DiaGraphics)
  - Value schema and type information (handled by DiaValueSchema)
  - History tracking and undo/redo (DiaCellHistory scope)
  - Layer editing operations (application-specific)
---
# DiaCellLayers

Generic library for registering and viewing per-cell "layers" in a CellField grid display.

Provides a registry system that allows any module or application to register cell-based overlay layers for visualization and interaction in grid-based editing or debugging contexts.

## Public API

| Type | Purpose |
|---|---|
| `CellValueKind` | Enumeration of cell value types (Float, Category) |
| `IViewValueSource` / `ViewPresentation` | Optional hook (installed once via `CellLayerRegistry::SetViewValueSource`, second install rejected with Error on CEL) that substitutes the viewer's values while not live (`GetValues` false = no data, `UploadResult::NoData`, never a live fallback). Cached reads keyed by `version()` + `ViewEpoch()`. `CellLayerRegistry::IsLive()` is true with no source or a live one; edit-capable systems refuse edits when false. Optional `GetPresentation` supplies ramp id, range and unit prefix (`CellLayerLegend::BuildPresented` -> `PresentedLegend`; JSON legend gains `unitPrefix`, state gains `noData` only when true). Test fake: `Testing/ViewValueSourceTestSupport.h` |
| `HotkeyTable` | Key -> layer id map per grid from schema `ui.hotkey` (clash -> Warning, later layer none); `GroupNumber` gives picker numbering 1..9 per group |
| `CellLayerLegend` | Wraps ValueSchema BuildLegend for the active base layer: ramp/range/unit/ticks + observed min/max (float) or swatches (category); unresolved schemaKey -> empty legend, one CEL Warning per key; caller supplies the RampResolver |
| `CellLayerViewerDomain` | IDebugDomain (DIA_DEBUG, panel-only) run on SimPU: base choice (ui.default_base else first; unregistered base -> grid default, none left -> empty state), LayerUploader + legend + `RequestDrawCellField`, enabled overlay draw callbacks, hover (cursor from `IDebugDraw::GetMousePixel`, camera via injected `ViewFn` returning `CellPickerView`), GetJSONState (grids, groups, layers, overlays, legend, hover; grid selector only when multi-grid). DiaAPI: `cells.layer.list` (JSON), `cells.layer.base <id>`, `cells.overlay.toggle <id>`; panel commands `base`/`grid`/`toggle`. Health `dia.celllayers.viewer`; metrics `dia.celllayers.viewer.*` |
| `CellLayersModule` | Module (PUAffinity::kAny, no per-frame work) that owns the `CellLayerRegistry` and publishes it on the `CellLayerRegistryService` ServiceStream (CL-006, no singletons). The registry is built in DoStart on the module's PU, which becomes its owner PU (register layers from that PU); DoStop withdraws the stream. Consumes the `ValueSchemaService` stream (declare `consumes` in the manifest) and attaches it to the registry via `CellLayerRegistry::SetSchema` on the first update that sees it (so `ui.default_base` works for manifest-built modules). Seams: `SetSchema` (explicit override, wins over the stream), `SetCurrentPuIdFn`. Consumers hold a `ServiceStreamReader<CellLayerRegistry>` on `kServiceStreamId` and resolve it with `FindCellLayerRegistry(reader)` (null when unavailable) in DoUpdate or later (the store commits after all providers' DoStart); the result feeds `CellLayerViewerDomain`'s `CellLayerRegistry&` constructor |
| `CellSelection` | Mutable cell selection set (bitset + cached count + version) bound to a single grid. One per grid owned by the registry (`SelectionFor`). Grid identity is pointer + width + height + topology; re-binding clears the selection. Version increments on net change only. `BeginEdit` / `EndEdit` batch edits into at most one version bump. Generic with no game types (CL-005) or singletons (CL-006). Accessed via registry interface `ICellSelectionView` (read-only). |
| `ICellSelectionView` | Read-only interface to a cell selection: `Version()`, `Count()`, `Contains(cellIndex)`, `ForEach()` visitor. Handed to overlay draw callbacks by `CellLayerViewerDomain` so systems can visualize the current selection. |
| `LayerReader` | Cached reader over registered layers: reads the layer's values (or substitutes from `IViewValueSource` if installed) with a cache key per layer and source epoch. Unifies value access for selection, query and aggregate operations so each layer reads at most once per version. Not thread-safe; owned by one PU. Metrics `Reads()` (actual read calls) and `CacheHits()` available but not yet registered to DiaObservation. |
| `SelectionTools` | Pure tool functions: `SelectCell` (replace), `ToggleCell`, `SelectBox`, `ClearSelection`, `SelectPolygon` (point-in-polygon), `SelectCells` (by coordinate list, capped at 2048 per call), `SelectCategory` (by layer value). Each returns `ToolResult { ok, changed, count }` or specialized `CellsResult` / `CategoryResult` with error details. Coordinates are cell (q=column, r=row); out-of-bounds ignored. Each call bumps version at most once on net change. Commands wire via `CellLayerViewerDomain` on `cells.select.*` path. |
| `SelectionOutline` | Cached perimeter of a selection (flat-top odd-q hex edges between selected and non-selected neighbours). Capped at 4000 edges; above that marked `TooLarge()` and not stored. Rebuilds only when selection version or grid identity (width, height, cellSize) changes. Supplies `OutlineEdge` array for debug drawing. Metrics `EdgeCount()`, `SelectedCountForBanner()`, `Rebuilds()` available but not yet registered. |
| `SelectionInput` | Mouse gesture state machine for selection commands: SELECT tool (LEFT button, click/drag/lasso on cells with modifiers for ToggleCell/SelectBox/SelectPolygon) and PAN tool (camera owns middle/right). Handles pointer-down, pointer-move, Esc-press; tracks gesture feedback (start cell, current cell, lasso path up to 256 points). Ctrl-click selects by category (group layer configurable per grid via `ui.select_group` schema hint or first category layer). Outputs to `ISelectionCommands` sink (wired by viewer to `cells.select.*` DiaAPI commands). |
| `SelectionStats` | Per-layer aggregates over a selection: float `count`/`nanCount`/`min`/`mean`(double)/`max` with schema label and unit; category counts per enum value (cap 64, `otherCount`, `nanCount`); 16-bin `Histogram` with out-of-range footer. Pure functions over `ICellSelectionView` + `LayerValues`; `Compute` reads through `LayerReader`. |
| `InspectorState` | Selection Inspector compute state owned by the viewer domain. Shown set = active base + layers of expanded groups on the active grid (caps 32 layers, 16 groups); one lazy recompute pass per Tick after a selection/layer-version/expanded-set change; `IsUpdating` marks the pending frame. Pin A (`Pin`, `PinFreeze`, `Unpin`): live A recomputes with B, frozen A stores stats for every grid layer once (`FrozenAvailable` false = "not frozen"); `ComputeDelta` gives B minus A. Metrics `dia.celllayers.inspector.*`; warns once per unresolved schemaKey. |
| `InspectorModel` | Card-independent views of the inspector: `BuildSingleCell` (exact values for shown layers + provider details, only for exactly one selected cell), `BuildStatsJson` (the `cells.inspect` result: shown/group/layers/all, histogram, pinned A + delta, single cell), `BuildCardJson` (panel `inspector` block from the cache). Feature 5 reuses `BuildStatsJson`. |
| `CellDetailRegistry` | Per-grid detail providers (`CellLayerRegistry::Details()`): providers emit sections/rows through `CellDetailBuilder` (`Section`, `Row`, `RowValue` with schema label/unit/tooltip), 64 rows max per provider (one Warning), run in registration order, only via `ShouldRunProviders` (exactly one selected cell, card open). |
| `QueryLexer` / `QueryParser` / `QueryResolver` / `QueryEvaluator` | Cell query language (`vegetation < 0.2 and biome = Grassland`). Lexer and parser produce a fixed-capacity AST (limits: 512 characters, depth 16; `not` > `and` > `or`) and `QueryError { message, column, length, suggestion }` with fixed char buffers. Resolver binds names to layers on the active grid (registered id, then unique short schema key) and enum ids (exact, case-sensitive) with closest-name suggestions (ratio >= 0.6). Evaluator reads each layer once through `LayerReader`, one bitset per comparison, and/or/not as bitset ops; NaN is false for every operator (so `not` of a NaN comparison is true); `ApplyTo` replaces a `CellSelection` with one version bump. Commands `cells.select.query` / `cells.query.check` (`{expr}`), panel commands `query_check` / `query_run`. Metrics `dia.celllayers.query.*`. |

## Overview

DiaCellLayers is a foundational system that enables:
- Registration of named cell layers with metadata
- Display of layers via CellField visualization
- Debug inspection and interaction with layers
- Standardized cell picking and hotkey handling
- Legend display for layer values

The system is generic—it knows about grids and cells, but not about game content or specific layer meanings.

## Selection

The registry owns one `CellSelection` per grid, created lazily via `SelectionFor(grid)` (up to 8 grids). Each selection is a bitset with cached count and version, bound to a specific grid identity (pointer, width, height, topology). The `CellLayerViewerDomain` integrates selection with mouse gestures (`SelectionInput`: click/drag/lasso with modifiers for replace/toggle/box/polygon operations) and displays the outline via `SelectionOutline` (capped at 4000 edges). Overlay draw callbacks receive the selection as a read-only `ICellSelectionView` so systems can react (e.g., a paint system applies a value to selected cells). Commands are routed through `cells.select.*` DiaAPI and JSON forms. The selection group (for ctrl-click category selection) defaults to the first category layer with `ui.select_group: true`, or the first category layer of the grid. Cell identity is row-major grid index (r * width + q). The Selection Inspector (`InspectorState`/`InspectorModel`) aggregates the selection per layer; commands `cells.select.pin` (optional `freeze`), `cells.select.unpin`, `cells.inspect [group|layers|all]` and panel command `cells.inspector.expand`.

## View State and Mirror

`SelectionFingerprint` (Feature 5) computes a stable hash of the inspector state (selected cells, shown layers, expanded groups) to detect changes across frames. `ViewStatePublisher` broadcasts the current view state to `ITopicSink` implementations: `DebugServerTopicSink` publishes to the `cells.view.state` topic on DiaDebugServer, enabling remote inspection. `MirrorQueries` provides query functions for remote clients: `cells.layer.list` (grid layers), `cells.layer.values` (grid layer values), `cells.select.get` (selected cell indices), `cells.select.stats` (per-layer statistics). Testing support via `Testing/Conformance` fixtures validates mirror behavior. Mirror metrics: `dia.celllayers.mirror.*` track publication and query activity.
