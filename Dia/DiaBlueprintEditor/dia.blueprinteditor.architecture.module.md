---
module_id: dia.blueprinteditor
version: dia.module.v1
display_name: DiaBlueprintEditor
parent: dia.editor
description: CluicheEditor plugin for authoring entity, camera, and light blueprint files.
language: cpp
layer: assets/tools

dependencies:
  - dia.editor
  - dia.assetcatalogue
  - dia.entity
  - dia.core
  - dia.observation

public_api:
  headers:
    - DiaBlueprintEditor/DiaBlueprintEditorPlugin.h
    - DiaBlueprintEditor/BlueprintFileHandler.h
    - DiaBlueprintEditor/BlueprintListController.h
    - DiaBlueprintEditor/BlueprintPropertyController.h
  namespaces:
    - Dia::BlueprintEditor
  entry_points:
    - DiaBlueprintEditorPlugin (registered via REGISTER_EDITOR_PLUGIN)

responsibilities:
  - Blueprint file I/O (.diaentity / .diacamera / .dialight)
  - Component CRUD (add/remove from blueprint)
  - Field default editing (type-aware via ComponentRegistry)
  - Blueprint list panel (grouped by type from asset catalogue)
  - Cross-scene usage display (reverse refs via RelationshipIndex)

non_responsibilities:
  - Scene placements or instance overrides (DiaSceneEditor)
  - Runtime entity inspection (DiaEntityInspector)
  - Asset registration/discovery (DiaAssetCatalogueEditor)
  - Component type definition (DiaReflect)

spec: docs/specs/systems/dia/diablueprinteditor.md
---
