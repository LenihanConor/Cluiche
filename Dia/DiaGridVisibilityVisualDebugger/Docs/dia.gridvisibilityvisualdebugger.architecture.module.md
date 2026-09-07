---
schema: dia.module.v1
module_id: dia.gridvisibilityvisualdebugger
name: DiaGridVisibilityVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaGridVisibilityVisualDebugger
language: cpp
parent_module_id: dia.diavisualdebugger

summary: >
  IDebugDomain implementation for the grid visibility system.

intent: >
  Provides GridVisibilityDebugDomain<TGraph>, which visualises per-cell visibility
  state, sight radii, and shadowcast boundaries in DiaDebugPanel. Cell State,
  Sight Radii, and Shadowcast Boundary drawers are registered with DebugLayerManager.

responsibilities:
  - GridVisibilityDebugDomain: IDebugDomain with 3 world-space drawers
  - Cell State Drawer: per-cell colour overlay (Unexplored/Revealed/Visible)
  - Sight Radii Drawer: circle outlines at each registered sight source
  - Shadowcast Boundary Drawer: edge segments at Visible/non-Visible boundaries

non_responsibilities:
  - GridVisibilitySystem evaluation — DiaGridVisibility
  - Fog-of-war render layer or minimap data
  - Any runtime behaviour in Release builds

dependent_modules:
  - dia.debug.visualdebugger
  - dia.gridvisibility
  - dia.entityspatial
  - dia.maths
  - dia.core
  - dia.debugdraw

public_api:
  headers:
    - DiaGridVisibilityVisualDebugger/GridVisibilityDebugDomain.h
  namespaces:
    - Dia::GridVisibilityVisualDebugger
  entry_points:
    - GridVisibilityDebugDomain

dependencies:
  required:
    - dia.debug.visualdebugger
    - dia.gridvisibility
    - dia.entityspatial
    - dia.maths
    - dia.core
    - dia.debugdraw
  forbidden: []
---
