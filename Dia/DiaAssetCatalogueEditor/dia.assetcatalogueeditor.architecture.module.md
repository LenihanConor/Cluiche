---
schema: dia.module.v1
module_id: dia.assetcatalogueeditor
name: DiaAssetCatalogueEditor
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaAssetCatalogueEditor
language: cpp
parent_module_id: dia

summary: >
  Build-time editor plugin for authoring and maintaining the asset catalogue manifest (assets.catalogue.json).

intent: >
  Provides a CluicheEditor plugin implementing IEditorPlugin that lets users create, edit, delete,
  and relate asset records. All data operations delegate to DiaAssetCatalogue's API; this module
  owns only presentation and user interaction.

responsibilities:
  - Manifest load/save via CatalogueManifestSerializer
  - Asset record CRUD (create, update, delete) with full undo/redo
  - File discovery (filesystem scan, suggest unregistered files)
  - Relationship editing (add/remove edges per record)
  - Validation display (per-record errors from AssetRegistry::Validate)
  - Asset type routing (open action — registered editor or OS default)
  - Catalogue rules UI (dry run preview, apply, manual overrides)
  - Relationship graph view (direct edges via VisJS)
  - Session context persistence (.context.json per SED-021)

non_responsibilities:
  - Any business logic — delegated to DiaAssetCatalogue
  - Asset pipeline execution — owned by DiaAssetPipeline
  - Asset content loading or preview
  - Live game connection

dependent_modules:
  - dia.editor
  - dia.assetcatalogue
  - dia.core
  - dia.logger

public_api:
  headers:
    - DiaAssetCatalogueEditor/DiaAssetCatalogueEditorPlugin.h
  namespaces:
    - Dia::AssetCatalogue::Editor
  entry_points:
    - DiaAssetCatalogueEditorPlugin

dependencies:
  required:
    - dia.editor
    - dia.assetcatalogue
    - dia.core
    - dia.logger
  forbidden:
    - dia.application
    - dia.assetpipeline
    - dia.assetruntime
---
