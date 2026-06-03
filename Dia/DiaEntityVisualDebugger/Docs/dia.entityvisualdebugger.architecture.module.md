---
schema: dia.module.v1
module_id: dia.entityvisualdebugger
name: DiaEntityVisualDebugger
owner_team: TBD
layer: assets/tools
status: active
maturity: dev

path: Dia/DiaEntityVisualDebugger
language: cpp
parent_module_id: dia.entity

summary: >
  In-game debug visualization for DiaEntity state — world-space labels, hierarchy lines,
  component-filter highlights, entity picking feedback, live field inspection, and domain
  statistics rendered as debug primitives and ImGui panels.

intent: >
  Bridges DiaEntity (pure ECS logic, no graphics dependency) and DiaVisualDebugger
  (debug draw infrastructure) without polluting either. Provides spatial debugging
  context that an out-of-game editor cannot — selection in the running viewport,
  hierarchy relationships overlaid on world positions, and live component inspection.

responsibilities:
  - Draw world-space text labels for entities matching a glob filter
  - Draw parent-to-child hierarchy lines between entity positions
  - Highlight all entities carrying a selected component type
  - Draw selection feedback (highlight ring) for the currently picked entity
  - Show live ImGui panel of component fields for the selected entity
  - Show ImGui stats panel with entity count, pool capacity, mutations/frame

non_responsibilities:
  - Hit-testing / picking logic — delegated to PickingModule and PickingService2D
  - Modifying entity state — strictly read-only (reads via IEntityInspectable)
  - Rendering — writes primitives to FrameData; actual rendering is DiaGraphics' concern
  - Application scheduling — module in CluicheGameBaseline handles lifecycle

dependent_modules:
  - dia.core
  - dia.maths
  - dia.graphics
  - dia.entity
  - dia.visualdebugger

public_api:
  headers:
    - DiaEntityVisualDebugger/EntityPositionHelper.h
    - DiaEntityVisualDebugger/EntityLabelsDrawer.h
    - DiaEntityVisualDebugger/EntityStatsDrawer.h
    - DiaEntityVisualDebugger/HierarchyLinesDrawer.h
    - DiaEntityVisualDebugger/ComponentFilterHighlightDrawer.h
    - DiaEntityVisualDebugger/EntityPickingDrawer.h
    - DiaEntityVisualDebugger/SelectionInspectorDrawer.h
  namespaces:
    - Dia::EntityVisualDebugger
  entry_points:
    - EntityLabelsDrawer
    - EntityStatsDrawer
    - HierarchyLinesDrawer
    - ComponentFilterHighlightDrawer
    - EntityPickingDrawer
    - SelectionInspectorDrawer

dependencies:
  required:
    - dia.entity
    - dia.visualdebugger
    - dia.graphics
    - dia.maths
    - dia.core
  forbidden:
    - dia.application
    - dia.rigidbody2d
    - dia.geometry2dpicking
---
