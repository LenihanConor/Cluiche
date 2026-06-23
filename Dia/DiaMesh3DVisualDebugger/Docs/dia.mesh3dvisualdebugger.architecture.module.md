---
schema: dia.module.v1
module_id: dia.mesh3dvisualdebugger
name: DiaMesh3DVisualDebugger
owner_team: TBD
layer: tools
status: active
maturity: dev

path: Dia/DiaMesh3DVisualDebugger
language: cpp
parent_module_id: dia.mesh3d

summary: >
  Read-only debug visualization for 3D mesh instances — wireframe AABB bounds,
  world-space axis crosses, and per-frame draw-count stats written as debug
  primitives without coupling to GPU resource management.

intent: >
  Bridges DiaMesh3D (pure mesh/draw-command data) and DiaGraphics3D (rendering
  primitives) without polluting either. Game code that does not need debug drawing
  links only DiaMesh3D and incurs no visualization overhead.

responsibilities:
  - Visual debug rendering for 3D mesh instances
  - Draws wireframe AABB bounds per draw command (MeshBoundsDrawer)
  - Draws world-space axis cross per draw command (MeshOriginDrawer)
  - Draws ImGui stats panel with per-frame draw counts (MeshStatsDrawer — future)

non_responsibilities:
  - Per-vertex normals, tangents, or wireframe mesh drawing
  - Any mutation of draw commands or assets (strictly read-only)
  - GPU resource management (no DiaBgfx3D dependency)

dependent_modules:
  - dia.core
  - dia.maths
  - dia.graphics3d
  - dia.mesh3d
  - dia.visualdebugger

public_api:
  headers:
    - DiaMesh3DVisualDebugger/MeshBoundsDrawer.h
    - DiaMesh3DVisualDebugger/MeshOriginDrawer.h
  namespaces:
    - Dia::Mesh3D
  entry_points:
    - MeshBoundsDrawer
    - MeshOriginDrawer

dependencies:
  required:
    - dia.mesh3d
    - dia.graphics3d
    - dia.visualdebugger
    - dia.maths
    - dia.core
  forbidden:
    - dia.application
    - dia.bgfx3d
---
