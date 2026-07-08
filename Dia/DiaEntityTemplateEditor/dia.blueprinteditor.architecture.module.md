---
schema: dia.module.v1
module_id: dia.EntityTemplateEditor
name: DiaEntityTemplateEditor
layer: domain/visual/tools
path: Dia/DiaEntityTemplateEditor

dependencies:
  - dia.editor
  - dia.assetcatalogue
  - dia.entity
  - dia.core
  - dia.observation

public_api:
  headers:
    - DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h
    - DiaEntityTemplateEditor/BlueprintFileHandler.h
    - DiaEntityTemplateEditor/BlueprintListController.h
    - DiaEntityTemplateEditor/BlueprintPropertyController.h
  namespaces:
    - Dia::EntityTemplateEditor
  entry_points:
    - DiaEntityTemplateEditorPlugin (registered via REGISTER_EDITOR_PLUGIN)

responsibilities:
  - Blueprint file I/O (.diaentitytemplate / .diacamera / .dialight)
  - Component CRUD (add/remove from blueprint)
  - Field default editing (type-aware via ComponentRegistry)
  - Blueprint list panel (grouped by type from asset catalogue)
  - Cross-scene usage display (reverse refs via RelationshipIndex)

non_responsibilities:
  - Scene placements or instance overrides (DiaSceneEditor)
  - Runtime entity inspection (DiaEntityInspector)
  - Asset registration/discovery (DiaAssetCatalogueEditor)
  - Component type definition (DiaReflect)

spec: docs/specs/systems/dia/DiaEntityTemplateEditor.md
---
