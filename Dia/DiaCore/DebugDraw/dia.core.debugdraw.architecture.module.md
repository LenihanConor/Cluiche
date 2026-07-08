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
  IVisualDebugger, IDebugContext, DebugLayerNames, DebugColourPalette. Requires
  DiaMaths (Vector2D/3D) so it lives at foundation/services, not foundation/core.

dependencies:
  required:
    - dia.core
    - dia.maths
---
