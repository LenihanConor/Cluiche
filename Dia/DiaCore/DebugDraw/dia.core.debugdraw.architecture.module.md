---
schema: dia.module.v1
module_id: dia.core.debugdraw
name: DiaCore.DebugDraw
owner_team: TBD
layer: foundation/platform
status: active
maturity: dev

path: Dia/DiaCore/DebugDraw
language: cpp
parent_module_id: dia.debugdraw

summary: >
  Debug draw interfaces hosted under DiaCore for historical reasons — IDebugDraw,
  IVisualDebugger, IDebugContext, DebugLayerNames, DebugColourPalette, CellFieldDrawRequest. Requires
  DiaMaths (Vector2D/3D) so it lives at foundation/services, not foundation/core.

dependent_modules:
  - dia.maths.vector
  - dia.core.crc
  - dia.core.core

public_api:
  headers:
    - Dia/DiaCore/DebugDraw/IDebugDraw.h
    - Dia/DiaCore/DebugDraw/IVisualDebugger.h
    - Dia/DiaCore/DebugDraw/IDebugContext.h
    - Dia/DiaCore/DebugDraw/DebugLayerNames.h
    - Dia/DiaCore/DebugDraw/DebugColourPalette.h
    - Dia/DiaCore/DebugDraw/CellFieldDrawRequest.h
  namespaces:
    - Dia::Core
  entry_points:
    - IDebugDraw
    - IVisualDebugger
    - IDebugContext
    - DebugLayerNames
    - DebugColourPalette
    - CellFieldDrawRequest

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.maths.vector
    - dia.core.crc
    - dia.core.core
---
