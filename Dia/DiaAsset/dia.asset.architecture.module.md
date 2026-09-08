---
schema: dia.module.v1
module_id: dia.asset
name: DiaAsset
layer: assets/core
path: Dia/DiaAsset
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Core asset identity and interface types — AssetId, AssetHandle, IAssetTypeHandler.
  Consumed by DiaAssetRuntime and DiaAssetCatalogue without pulling in runtime loading.

responsibilities:
  - AssetId / AssetHandle value types
  - IAssetTypeHandler abstract interface
  - Asset type tag constants

non_responsibilities:
  - Asset loading or streaming (DiaAssetRuntime)
  - Asset catalogue / discovery (DiaAssetCatalogue)

dependencies:
  required:
    - dia.core
  forbidden: []
---
