---
schema: dia.module.v1
module_id: dia.assetruntime.handlers
name: DiaAssetRuntime.Handlers
owner_team: TBD
layer: domain/visual/core
status: active
maturity: dev

path: Dia/DiaAssetRuntime/Handlers
language: cpp
parent_module_id: dia.assetruntime

summary: >
  Domain-specific asset type handlers for DiaAssetRuntime. TextureHandler depends
  on DiaBgfx and DiaGraphics, placing it in the visual domain tier.

dependent_modules:
  - dia.core.memory
  - dia.core.crc
  - dia.core.core

dependencies:
  required:
    - dia.assetruntime
    - dia.graphics
    - dia.bgfx
    - dia.threading
    - dia.core.memory
    - dia.core.crc
    - dia.core.core
---
