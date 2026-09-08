---
schema: dia.module.v1
module_id: dia.pathfindingvisualdebugger
name: DiaPathfindingVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaPathfindingVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaPathfinding. World-space domain with two
  drawers: PathPolyline (waypoints + start/goal markers) and GridPassability
  (passable/impassable cell overlay). Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library adding pathfinding debug visibility without creating
  a compile-time dependency from DiaPathfinding onto DiaVisualDebugger.

responsibilities:
  - PathfindingVisualDebugger — IDebugDomain implementation
  - PathPolylineDrawer — path segment lines + start/goal circle markers
  - GridPassabilityDrawer — per-cell passable/impassable quad overlay

non_responsibilities:
  - Pathfinding algorithm execution — DiaPathfinding
  - Flow field visualisation — DiaFlowField

dependent_modules:
  - dia.pathfinding

public_api:
  headers:
    - Dia/DiaPathfindingVisualDebugger/PathfindingVisualDebugger.h
  namespaces:
    - Dia::Pathfinding

dependencies:
  required:
    - dia.core
    - dia.pathfinding
    - dia.visualdebugger
  forbidden:
    - dia.entity
    - dia.applicationflow
    - dia.aibudget
---
