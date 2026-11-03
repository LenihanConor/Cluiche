---
schema: dia.module.v1
module_id: dia.debug.visualdebugger
name: DiaVisualDebugger
owner_team: TBD
layer: 4.0
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
  - DiaAPI commands: debug.layer.enable, debug.layer.disable, debug.scale, debug.pick
  - Global debug scale exposed via SetDebugScale/GetDebugScale
  - Picking seam stubs: SetSelectedEntityId/GetSelectedEntityId
  - Domain state snapshots (Console/): PackDomainStates packs every registered IDebugDomain's panel JSON
    on SimPU into a fixed-size DomainStateSnapshot (whole-domain 128 KB budget, truncated flag);
    DomainStateSnapshotPublisher writes it once per Sim frame only while Main has expressed interest;
    ParseDomainStateSnapshot / QueryDomainState read it on Main. The snapshot (FrameStream) and the
    interest event (EventStream) are the only cross-PU data, no mutex or atomic crosses the boundary.
    DomainStateHealth reports dia.debugvisualiser.domain_snapshot (stale or never published).
  - BuildDomainState replaces NaN/Infinity with null and treats a non-object domain state as {}.

non_responsibilities:
  - Rendering — layers submit to FrameData; DiaSFML owns rendering
  - Frame budget enforcement — DebugFrameData (DiaGraphics) owns capacity limits
  - Editor WebSocket broadcast — removed 2026-11-02 (debug-editor-panel retired; see debug-layer-broadcast-removal)

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
    - DiaVisualDebugger/Domain/SchemaControl.h
    - DiaVisualDebugger/Domain/ISchemaKeyResolver.h
    - DiaVisualDebugger/Domain/ServiceKeyResolver.h
    - DiaVisualDebugger/Console/DomainStateSnapshot.h
    - DiaVisualDebugger/Console/DomainStateSnapshotPublisher.h
    - DiaVisualDebugger/Console/DomainStateQuery.h
  namespaces:
    - Dia::Debug
    - Dia::Debug::LayerNames
  entry_points:
    - IVisualDebugger
    - DebugLayerManager
    - DebugColourPalette
    - DebugLayerNames constants
    - PackDomainStates / DomainStateSnapshot (Sim-side packer and fixed-size snapshot POD)
    - DomainStateSnapshotPublisher, DomainStateHealth (interest-gated producer; health reporter)
    - ParseDomainStateSnapshot, QueryDomainState, ApplySchemaVersion (Main-side readers)
    - SchemaControl (schemaKey/packId control JSON + schemaVersion; ISchemaKeyResolver seam, no DiaValueSchema dependency)

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
