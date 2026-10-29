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

## Overview

DiaCellLayers is a foundational system that enables:
- Registration of named cell layers with metadata
- Display of layers via CellField visualization
- Debug inspection and interaction with layers
- Standardized cell picking and hotkey handling
- Legend display for layer values

The system is generic—it knows about grids and cells, but not about game content or specific layer meanings.
