**Spec:** @docs/specs/applications/cluicheeditor/systems/diaeditorapi/scene-editor-scriptable.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` + sub-methods to `DiaSceneEditorPlugin`; wire from `OnPluginLoad()`; add `DeregisterActionsForOwner` in `OnPluginUnload()` | `GetManifest()` lists all 40 `scene_editor.*` actions | Done | sonnet |  |
| 2 | Register 12 kCallerThread actions (`get_project_state`, `get_stage_list`, `get_hierarchy`, `get_hierarchy_filtered`, `get_properties`, `get_entity_template_defaults`, `get_available_entity_templates`, `get_dirty_state`, `analyse_change_entity_template`, `validate`, `get_scene_properties`, `get_entities`) | Python: `dia_editor.scene_editor.get_project_state()` returns expected shape | Done | sonnet |  |
| 3 | Register 25 kMainThread actions (all mutating existing handlers except `new_scene_shortcut` and dialog `create_asset`) | Python: `dia_editor.scene_editor.load_scene(path)` loads scene | Done | sonnet |  |
| 4 | Implement `scene_editor.get_entities` — new kCallerThread action; flat list from `mLoadedSceneRoot` | `get_entities()` returns entries with id/templateId/enabled/layerId/overrides | Done | sonnet |  |
| 5 | Implement `scene_editor.place_entity` — new kMainThread action; composition of add_item + position override | `place_entity(templateId, position)` adds entity; appears in `get_entities()` | Done | sonnet |  |
| 6 | Implement `scene_editor.remove_entity` — thin kMainThread wrapper calling `HandleDeleteItem` with itemType='entity' | `remove_entity(entityId)` removes entity from scene | Done | haiku |  |
| 7 | Implement `scene_editor.create_scene` and `scene_editor.create_asset` — new kMainThread actions delegating to `asset_catalogue.*` | `create_scene(id, source_path)` creates asset record and loads scene | Done | sonnet |  |
| 8 | Python smoke test: add `scene_editor.get_project_state` and `scene_editor.get_entities` checks | Smoke test passes | Done | haiku |  |
| 9 | Commit and update plan/spec | Build passes | Done | haiku |  |
