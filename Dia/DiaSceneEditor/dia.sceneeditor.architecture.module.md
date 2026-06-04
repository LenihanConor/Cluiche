---
schema: dia.module.v1
module_id: dia.sceneeditor
name: DiaSceneEditor
layer: domain/visual/tools
path: Dia/DiaSceneEditor

dependencies:
  - dia.editor
  - dia.assetcatalogue
  - dia.scene2d
  - dia.game
  - dia.entity
  - dia.core
  - dia.observation

public_api:
  headers:
    - DiaSceneEditor/DiaSceneEditorPlugin.h
    - DiaSceneEditor/SceneFileHandler.h
    - DiaSceneEditor/SceneHierarchyController.h
    - DiaSceneEditor/PropertyInspectorController.h
  namespaces:
    - Dia::SceneEditor
  entry_points:
    - DiaSceneEditorPlugin (registered via REGISTER_EDITOR_PLUGIN)

responsibilities:
  - Scene hierarchy panel (layers, cameras, lights, entities)
  - Property inspector for selected scene items
  - Scene file I/O (.diascene JSON read/write)
  - Entity/camera/light placement CRUD
  - Layer authoring
  - Scene validation (unique IDs, active camera, default layer)
  - Stage/scene selection from .diagame project context

non_responsibilities:
  - Blueprint authoring (DiaBluprintEditor)
  - 2D spatial viewport (future feature spec)
  - Runtime entity inspection (DiaEntityInspector)
  - Asset creation or pipeline integration

spec: docs/specs/systems/dia/diasceneeditor.md
---
