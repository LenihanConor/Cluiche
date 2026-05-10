---
schema: dia.module.v1
module_id: dia.assetruntime
name: DiaAssetRuntime
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaAssetRuntime
language: cpp
parent_module_id: dia

summary: >
  In-game asset path resolution and lifecycle management system.

intent: >
  Loads assets.runtime.json at startup, provides ID-to-absolute-path resolution for all
  known assets, and manages Stage-level load/unload state across Stage transitions. Does
  not load asset content — that is the consuming system's responsibility, triggered by
  IAssetStateListener callbacks.

responsibilities:
  - Manifest loading — deserializes assets.runtime.json via RuntimeManifestLoader
  - Path resolution — resolves StringCRC asset ID to absolute deploy FilePath
  - Asset state machine — tracks Registered/Staged/Loaded/Unloading per asset
  - Stage lifecycle — RequestStageLoad/Unload expands Stage to Assets, manages transitions
  - Reference counting — global assets ref-counted across Stages
  - Event notification — IAssetStateListener callbacks on state transitions
  - Debug query API — GetLoadedAssets, GetStagedAssets, GetStageDependencies
  - Path alias registration — asset IDs registered as aliases in DiaCore PathStore

non_responsibilities:
  - Asset content loading (textures, audio, meshes — owned by DiaGraphics, DiaAudio)
  - DiaApplicationFlow lifecycle integration
  - Manifest authoring or editing (owned by DiaAssetCatalogueEditor)
  - Asset pipeline or transform (owned by DiaAssetPipeline)

dependent_modules:
  - dia.core
  - dia.logger

public_api:
  headers:
    - DiaAssetRuntime/AssetRuntime.h
    - DiaAssetRuntime/IAssetStateListener.h
    - DiaAssetRuntime/AssetState.h
  namespaces:
    - Dia::AssetRuntime
  entry_points:
    - AssetRuntime
    - IAssetStateListener
    - AssetState
    - RuntimeManifestLoader
    - RuntimeAssetEntry
    - RuntimeStageEntry

dependencies:
  required:
    - dia.core
    - dia.logger
  forbidden:
    - dia.application
    - dia.assetcatalogue
    - dia.graphics
    - dia.audio
---
