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
  forbidden: []
public_api:
  headers:
    - DiaCellLayers/CellLayersTypes.h
    - DiaCellLayers/CellLayerRegistry.h
    - DiaCellLayers/HotkeyTable.h
    - DiaCellLayers/CellLayerLegend.h
    - DiaCellLayers/CellLayerViewerDomain.h
  namespaces:
    - Dia::CellLayers::
  entry_points:
    - CellValueKind
    - CellLayerRegistry
    - HotkeyTable
    - CellLayerLegend
    - CellLayerViewerDomain
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
| `HotkeyTable` | Key -> layer id map per grid from schema `ui.hotkey` (clash -> Warning, later layer none); `GroupNumber` gives picker numbering 1..9 per group |
| `CellLayerLegend` | Wraps ValueSchema BuildLegend for the active base layer: ramp/range/unit/ticks + observed min/max (float) or swatches (category); unresolved schemaKey -> empty legend, one CEL Warning per key; caller supplies the RampResolver |
| `CellLayerViewerDomain` | IDebugDomain (DIA_DEBUG, panel-only) run on SimPU: base choice (ui.default_base else first; unregistered base -> grid default, none left -> empty state), LayerUploader + legend + `RequestDrawCellField`, enabled overlay draw callbacks, hover (cursor from `IDebugDraw::GetMousePixel`, camera via injected `ViewFn` returning `CellPickerView`), GetJSONState (grids, groups, layers, overlays, legend, hover; grid selector only when multi-grid). DiaAPI: `cells.layer.list` (JSON), `cells.layer.base <id>`, `cells.overlay.toggle <id>`; panel commands `base`/`grid`/`toggle`. Health `dia.celllayers.viewer`; metrics `dia.celllayers.viewer.*` |

## Overview

DiaCellLayers is a foundational system that enables:
- Registration of named cell layers with metadata
- Display of layers via CellField visualization
- Debug inspection and interaction with layers
- Standardized cell picking and hotkey handling
- Legend display for layer values

The system is generic—it knows about grids and cells, but not about game content or specific layer meanings.
