---
schema: dia.module.v1
module_id: dia.assetruntimevisualdebugger
name: DiaAssetRuntimeVisualDebugger
layer: assets/tools
path: Dia/DiaAssetRuntimeVisualDebugger
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Visual debug overlay for DiaAssetRuntime — draws asset load state, reference counts,
  and streaming status via DiaDebugDraw layers.

responsibilities:
  - Register debug draw layers for asset runtime state
  - Draw per-asset load/unload indicators
  - Expose DiaAPI commands for toggling asset debug views

non_responsibilities:
  - Asset loading logic (DiaAssetRuntime)
  - Editor UI (DiaAssetRuntimeInspector)

dependencies:
  required:
    - dia.core
    - dia.assetruntime
    - dia.debugdraw
    - dia.observation
  forbidden: []
---
