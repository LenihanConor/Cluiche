---
schema: dia.module.v1
module_id: dia.lighting3dvisualdebugger
name: DiaLighting3DVisualDebugger
owner_team: TBD
layer: domain/visual/tools
status: active
maturity: dev

path: Dia/DiaLighting3DVisualDebugger
language: cpp
parent_module_id: dia.lighting3d

summary: >
  Read-only debug visualization for 3D lights — per-light sphere/arrow widgets
  and spline arc previews drawn as debug primitives without coupling to lighting
  or rendering internals.

intent: >
  Bridges DiaLighting3D (no graphics dependency) and DiaVisualDebugger without
  polluting either. Games that don't need debug drawing link only DiaLighting3D
  and incur no visualization overhead.

responsibilities:
  - Visual debug rendering for 3D light instances
  - Draws sphere/arrow widgets per light type (LightWidgetsDrawer)
  - Draws sampled spline arc when LightPathBehaviour3D is attached (LightPathArcDrawer)
  - Exposes ImGui shelf per drawer for toggles and tunable parameters

non_responsibilities:
  - Light volume, shadow cone, or falloff visualization
  - Editor UI panels
  - 2D light visualization
  - Any mutation of lights (read-only)

dependent_modules:
  - dia.core
  - dia.maths
  - dia.geometry3d
  - dia.lighting3d
  - dia.visualdebugger

public_api:
  headers:
    - DiaLighting3DVisualDebugger/LightWidgetsDrawer.h
    - DiaLighting3DVisualDebugger/LightPathArcDrawer.h
  namespaces:
    - Dia::Lighting3D
  entry_points:
    - LightWidgetsDrawer
    - LightPathArcDrawer

dependencies:
  required:
    - dia.lighting3d
    - dia.geometry3d
    - dia.visualdebugger
    - dia.maths
    - dia.core
  forbidden:
    - dia.application
    - dia.bgfx3d
    - dia.bgfx
    - dia.graphics3d
---
