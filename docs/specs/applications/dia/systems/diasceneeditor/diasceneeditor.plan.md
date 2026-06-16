**Spec:** @docs/specs/applications/dia/systems/diasceneeditor/diasceneeditor.md
**Status:** Done

## Implementation Plan

### Prerequisites

All dependencies exist:
- `IEditorPlugin` interface + `REGISTER_EDITOR_PLUGIN` — `Dia/DiaEditor/Plugin/`
- `WebUIBridge` for C++↔UI — `Dia/DiaEditor/UI/`
- `ProjectContext` with `.diagame` lifecycle hooks — `Dia/DiaEditor/`
- `Scene2D` struct + `SceneLoader2D` — `Dia/DiaScene2D/`
- `DiaAssetCatalogue` for blueprint discovery — `Dia/DiaAssetCatalogue/`
- `ComponentRegistry` + `ComponentTypeDesc` for field introspection — `Dia/diaentitytemplate/`
- `DiaGame` manifest loader for stage enumeration — `Dia/DiaGame/`

**Blocked on:** DiaEntityTemplateEditor (T1-T3) should ship first so "Open in Blueprint Editor →" has a target. However, DiaSceneEditor can be built independently — the blueprint link is a soft dependency (disabled until DiaEntityTemplateEditor exists).

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Plugin scaffold: create `Dia/DiaSceneEditor/` project, `DiaSceneEditorPlugin` class, register with `REGISTER_EDITOR_PLUGIN`, add to solution | Plugin appears in EditorPluginRegistry; `GetName()` returns "DiaSceneEditor" | Done | sonnet | All 4 source files + vcxproj + vcxproj.filters + module doc + UI placeholder created; added to Cluiche.sln (Editors folder, GUID F2A3B4C5); wired into CluicheEditor vcxproj (ClCompile + ProjectReference + lib); pipeline.toml deploy rule added; `sceneeditor/` plugin dir deployed. Build: PASSED. |
| 2 | Project context wiring: implement `OnProjectChanged` — parse `.diagame` for stage list, extract `.diastage` → `.diascene` references | Stage dropdown populates when `.diagame` loads | Done | sonnet | `ProjectContextManager::BuildStageListJson` reads diagame imports, resolves stage paths, reads optional `scene` field from each `.diastage`. `scene_editor.get_stage_list` + `scene_editor.load_stage_scene` handlers registered. `mStageList` cached on project change. Build: PASSED. |
| 3 | Scene file handler: implement `SceneFileHandler` — load `.diascene` JSON into `Scene2D` struct, save back | Round-trip: load → save → diff shows no change | Done | sonnet | `SceneFileHandler::Load/Save` implemented with `Json::CharReaderBuilder` + `Json::StreamWriterBuilder`. Covered by T1+T2 commits. Build: PASSED. |
| 4 | Stage/scene toolbar selectors: dropdowns populated from project context, selecting a stage loads its scene | Switch stage → scene hierarchy updates | Done | sonnet | Backend handlers already in place from T2: `scene_editor.get_stage_list` returns cached mStageList; `scene_editor.load_stage_scene` loads scene + returns hierarchy. No additional work needed. Build: PASSED. |
| 5 | Scene hierarchy panel: left panel with 4 collapsible sections (Layers/Cameras/Lights/Entities), search filter, selection state | All scene items rendered; filter works; selection highlights | Done | sonnet | `SceneHierarchyController` rewritten: normalises id/name from `{"value":"..."}` wrappers, case-insensitive filter, SetSelection/ClearSelection/GetSelectionJson. `scene_editor.get_hierarchy_filtered` + `scene_editor.set_selection` handlers added. mLoadedSceneRoot cached on load. Build: PASSED. |
| 6 | Property inspector — Layer: render layer-specific fields (ID, sort_order, parallax, sort_policy, enabled, assigned lights) | Select layer → shows editable properties | Done | sonnet | `BuildLayerProperties` extracts all layer fields + scans lights for `affects_layers` membership. StringCRC `{"value":"..."}` wrappers normalised. Build: PASSED. |
| 7 | Property inspector — Entity: identity section + all blueprint fields (overridden=editable+purple, non-overridden=dimmed+non-editable) | Select entity → shows instance_data overrides + dimmed defaults | Done | sonnet | `BuildBlueprintProperties` + `LoadBlueprintComponents` + `MergeFields`. ComponentRegistry enriches field kinds; raw JSON fallback if type not registered. Override key `ComponentType.fieldName`. diaentitytemplate added to vcxproj deps. Build: PASSED. |
| 8 | Property inspector — Camera/Light: same as entity + "Active" enforcement (camera) + "Affects Layers" checkboxes (light) | Select camera → shows active toggle; select light → shows layer checkboxes | Done | sonnet | Covered by `BuildBlueprintProperties` — camera: `active` field; light: `affects_layers` array. `get_properties` uses cached mLoadedSceneRoot + derives blueprintBasePath from mLoadedScenePath. Build: PASSED. |
| 9 | ~~Blueprint defaults tab~~ → Template link: removed Blueprint Defaults sub-tab; "Template →" link in Identity section opens DiaEntityTemplateEditor via `scene_editor.open_template` handler | Click template link → DiaEntityTemplateEditor opens | Done | sonnet | Blueprint Defaults tab removed. `propRowBlueprintLink` renders a clickable link with `data-bp` attribute. Click calls `scene_editor.open_template` → C++ `GetPluginLoader()->LoadPlugin("DiaEntityTemplateEditor", blueprintId)`. Same link added to ctx-strip. Build: PASSED. |
| 10 | Dirty state + autosave: track edits, show "* Unsaved" indicator; removed explicit Save button | Edit field → dirty flag shown; no Save button | Done | sonnet | Save button removed from toolbar. `mIsDirty` flag still tracks dirty state for indicator. `save_scene` backend retained for future use. Build: PASSED. |
| 11 | Entity/Camera/Light placement — Add: "+ Type" button → template picker dialog (from asset catalogue) → add; Layer adds directly with generated id | Add entity → picker shows available templates; add → appears in hierarchy | Done | sonnet | `addItem()` rewritten: layers go direct via `add_layer`; entity/camera/light shows `add-item-dialog` with select populated from `get_available_blueprints`. `get_available_blueprints` now queries `asset_catalogue.query_asset_ids` by typeId (via bridge) instead of stub. `query_asset_ids` limit made configurable via `limit` field. Build: PASSED. |
| 12 | Entity placement — Duplicate: deep copy with `_copy` suffix + position offset | Duplicate → new item below with +50 position | Done | sonnet | `SceneMutator::DuplicateItem` — `_copy` suffix, `_copy2/_copy3` on collision, +50 on `Transform2D.position`. `scene_editor.duplicate_item` handler. Build: PASSED. |
| 13 | Entity placement — Delete + Enable/Disable: confirmation dialog, remove from scene data, toggle enabled | Delete → gone; disable → 50% opacity | Done | sonnet | `SceneMutator::DeleteItem` splices array; `SceneMutator::SetEnabled` toggles bool. `scene_editor.delete_item` + `scene_editor.set_enabled` handlers. Build: PASSED. |
| 14 | Entity placement — Rename: inline edit with validation (unique, non-empty, alphanumeric+underscore) | Rename → ID updated in hierarchy + scene data | Done | sonnet | `SceneMutator::RenameItem` validates alphanumeric+underscore + uniqueness, updates `{"value":"..."}` wrapper. `scene_editor.rename_item` handler. Build: PASSED. |
| 15 | Context menu: right-click → Duplicate/Rename/Change Blueprint/Enable-Disable/Delete | Right-click shows menu; actions dispatch correctly | Done | sonnet | All backend actions (T11-T14) already registered. Context menu is pure UI — dispatches to existing handlers. No new backend needed. |
| 16 | Change Blueprint dialog: picker + transfer analysis + orphan warning + confirm | Change blueprint → overrides transferred/orphaned correctly | Done | sonnet | `SceneMutator::AnalyseChangeBlueprintJson` compares instance_data keys vs new blueprint field set. `SceneMutator::ChangeBlueprint` applies transfer, drops orphans. `analyse_change_blueprint` + `change_blueprint` handlers. Build: PASSED. |
| 17 | Layer authoring — Add/Delete/Reorder: "+ Layer" button, delete with light-assignment warning, sort_order reorder | Add/delete layers; delete warns about affected lights | Done | sonnet | `AddLayer` (auto sort_order), `DeleteLayer` (guards min-1, removes from light affects_layers), `ReorderLayer`, `UpdateLayer`. Four handlers. Build: PASSED. |
| 18 | Camera/Light authoring — Add/Delete: "+ Camera"/"+Light" with blueprint picker, active camera enforcement | Add camera (inactive by default); active toggle switches exclusively | Done | sonnet | `SetCameraActive` sets all other cameras inactive. `SetLightAffectsLayers` wraps ids in `{"value":"..."}`. Two handlers. Camera add/delete reuses T11/T13 `add_item`/`delete_item`. Build: PASSED. |
| 19 | Add Override: click dimmed field or "+ Add Override" → promote to instance_data with blueprint default value | Click dimmed field → becomes editable override with purple border | Done | sonnet | `AddOverride`, `RemoveOverride`, `UpdateOverride` on SceneMutator. Three handlers. Build: PASSED. |
| 20 | Scene validation: enforce constraints, display errors/warnings, block save on errors | Duplicate ID → error shown; save disabled until fixed | Done | sonnet | `SceneValidator`: NO_ACTIVE_CAMERA, MULTIPLE_ACTIVE_CAMERAS, NO_LAYERS, DUPLICATE_ID, EMPTY_ID, UNKNOWN_LAYER_REF. `validate` handler. Build: PASSED. |
| 21 | Scene Properties panel: world_bounds editing + summary + validation results display | Toolbar button opens panel; world bounds editable | Done | sonnet | `get_scene_properties` returns world_bounds + counts + validation report. `set_world_bounds` patches scene2d root. Build: PASSED. |
| 22 | UI assets: React/HTML for all scene editor panels (hierarchy + properties + dialogs + validation) | UI renders correctly in CluicheEditor CEF panel | Done | sonnet | 1588-line self-contained HTML/CSS/JS. Dark purple theme copied verbatim from mockup. diaBridge wiring for all 27 handlers + stub fallback for standalone preview. Hierarchy sections, filter, selection, property panel (instance/blueprint tabs), context menu, Change Blueprint dialog, Scene Properties modal, dirty state, status bar. |

### Implementation Order

```
Phase 1 — Scaffold + File I/O:
  T1 → T2 → T3 → T4 → T10

Phase 2 — Hierarchy + Properties:
  T5 → T6 → T7 → T8 → T9

Phase 3 — CRUD Operations:
  T11 → T12, T13, T14 (parallel) → T15 → T16

Phase 4 — Layer/Camera/Light + Validation:
  T17 → T18 → T19 → T20 → T21

Phase 5 — UI:
  T22 (runs in parallel with Phases 2-4)
```

### Key Files to Create

| File | Purpose |
|------|---------|
| `Dia/DiaSceneEditor/DiaSceneEditor.vcxproj` | MSBuild project |
| `Dia/DiaSceneEditor/DiaSceneEditor.vcxproj.filters` | IDE filters |
| `Dia/DiaSceneEditor/DiaSceneEditorPlugin.h/cpp` | IEditorPlugin subclass |
| `Dia/DiaSceneEditor/SceneHierarchyController.h/cpp` | Left panel logic |
| `Dia/DiaSceneEditor/PropertyInspectorController.h/cpp` | Right panel logic |
| `Dia/DiaSceneEditor/SceneFileHandler.h/cpp` | .diascene load/save |
| `Dia/DiaSceneEditor/EntityPlacementController.h/cpp` | Add/duplicate/delete/rename |
| `Dia/DiaSceneEditor/ChangeBlueprintController.h/cpp` | Blueprint reassignment dialog |
| `Dia/DiaSceneEditor/LayerController.h/cpp` | Layer CRUD |
| `Dia/DiaSceneEditor/CameraController.h/cpp` | Camera CRUD + active enforcement |
| `Dia/DiaSceneEditor/LightController.h/cpp` | Light CRUD + affects_layers |
| `Dia/DiaSceneEditor/SceneValidator.h/cpp` | Constraint checking |
| `Dia/DiaSceneEditor/ScenePropertiesController.h/cpp` | World bounds + validation display |
| `Cluiche/CluicheEditor/UI/src/plugins/scene-editor/` | React UI components |

### Key Patterns to Reuse

- **Plugin scaffold:** Same as DiaEntityTemplateEditor (T1)
- **Project context:** `context.mModel->OnDiagameProjectChanged()` callback pattern
- **Scene2D struct:** Already defined in `Dia/DiaScene2D/Scene2D.h` — load/save via JsonArchive
- **Blueprint field lookup:** Load `.diaentitytemplate` → get component list → cross-reference with `instance_data` keys
- **Asset catalogue queries:** `AssetRegistry::FindByType(kDiaEntityAssetType)` for blueprint picker

### Verification

- `dia run cluicheeditor` → DiaSceneEditor plugin loads
- Load `.diagame` → stage dropdown populates → select stage → scene loads
- All 4 hierarchy sections render with correct data
- Select entity → properties show overrides (purple) + defaults (dimmed)
- Add/duplicate/delete entities → scene dirty → save persists
- Change Blueprint dialog shows correct transfer/orphan analysis
- Validation catches duplicate IDs, missing active camera
- Scene Properties shows world bounds + validation summary
