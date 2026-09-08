---
schema: dia.module.v1
module_id: dia.diascalarfieldinspector
name: DiaScalarFieldInspector
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaScalarFieldInspector
language: cpp
parent_module_id: dia.scalarfield

summary: >
  Editor-side ImGui panel for inspecting DiaScalarField state when the simulation is
  paused. Provides field registry list, per-cell statistics, heatmap thumbnail, cell
  hover tooltip, N-frame history scrubbing, and runtime write authoring.

intent: >
  Complements DiaScalarFieldVisualDebugger (runtime overlay) with capabilities that
  require paused state or persistent history: numeric cell hover, stats, history
  scrubbing, and immediate write injection. Uses IDiaScalarField type erasure so the
  panel holds heterogeneous field references.

responsibilities:
  - ScalarFieldInspectorPanel: IEditorPlugin-derived dockable ImGui panel
  - Selectable field registry list (name, topology type, cell count)
  - Statistics row: min / max / mean across all cells
  - Heatmap thumbnail: inline ImGui coloured table, one coloured cell per scalar field cell
  - Cell hover tooltip: CellIndex + exact float value on mouse hover
  - History scrub slider: drives IDiaScalarField snapshot API (SFI-001/SFI-002)
  - Write authoring controls: WritePoint / WriteRadial / WriteBox form fields

non_responsibilities:
  - Runtime heatmap overlay (DiaScalarFieldVisualDebugger)
  - Scalar field propagation logic (DiaScalarField)
  - Editor layout / docking infrastructure (DiaEditor)
  - Topology type definitions (DiaScalarField)

dependent_modules:
  - dia.scalarfield
  - dia.editor
  - dia.core
  - dia.maths

public_api:
  headers:
    - DiaScalarFieldInspector/ScalarFieldInspectorPanel.h
  namespaces:
    - Dia::ScalarField
  entry_points:
    - ScalarFieldInspectorPanel

dependencies:
  required:
    - dia.scalarfield
    - dia.editor
    - dia.core
    - dia.maths
  optional: []
  forbidden:
    - dia.visualdebugger
    - dia.application
    - dia.graphics
---
