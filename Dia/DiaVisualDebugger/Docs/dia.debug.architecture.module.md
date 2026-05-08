---
schema: dia.module.v1
module_id: dia.debug.visualdebugger
name: DiaVisualDebugger
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaVisualDebugger
language: cpp
parent_module_id: dia.debug

summary: >
  Central debug draw layer registry — IVisualDebugger interface, DebugLayerManager,
  DebugColourPalette, and DebugLayerNames.

intent: >
  Provides the foundational infrastructure for all debug visualisation in the Dia engine.
  Draw classes register themselves with DebugLayerManager, which sorts them by priority
  and calls each enabled layer once per frame. A global debug scale and DiaAPI command
  registration surface are also provided.

responsibilities:
  - IVisualDebugger interface — one interface, one concern per draw class
  - DebugLayerManager — registration, priority sort, per-frame dispatch
  - DebugColourPalette — 9 binding RGBA constants (SD-DBG-010)
  - DebugLayerNames — canonical StringCRC constants for all Dia-owned layer names
  - DiaAPI commands: debug.layer.enable, debug.layer.disable, debug.layer.list, debug.scale, debug.pick
  - Global debug scale exposed via SetDebugScale/GetDebugScale
  - Picking seam stubs: SetSelectedEntityId/GetSelectedEntityId

non_responsibilities:
  - Rendering — layers submit to FrameData; DiaSFML owns rendering
  - Frame budget enforcement — DebugFrameData (DiaGraphics) owns capacity limits
  - Editor WebSocket broadcast — deferred to feature debug-editor-panel (feature 10)

dependent_modules:
  - dia.core
  - dia.maths
  - dia.graphics
  - dia.api

public_api:
  headers:
    - DiaVisualDebugger/IVisualDebugger.h
    - DiaVisualDebugger/DebugLayerManager.h
    - DiaVisualDebugger/DebugColourPalette.h
    - DiaVisualDebugger/DebugLayerNames.h
  namespaces:
    - Dia::Debug
    - Dia::Debug::LayerNames
  entry_points:
    - IVisualDebugger
    - DebugLayerManager
    - DebugColourPalette
    - DebugLayerNames constants

dependencies:
  required:
    - dia.core
    - dia.graphics
    - dia.api
  optional:
    - dia.debug.server  # DebugServerModule* forward-declared; caller passes nullptr if absent
  forbidden:
    - dia.sfml          # No rendering allowed in debug layer manager
---
