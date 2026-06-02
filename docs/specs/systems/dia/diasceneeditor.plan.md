**Spec:** @docs/specs/systems/dia/diasceneeditor.md
**Status:** In Progress

## Implementation Plan

### Prerequisites

All dependencies exist:
- `IEditorPlugin` interface + `REGISTER_EDITOR_PLUGIN` — `Dia/DiaEditor/Plugin/`
- `WebUIBridge` for C++↔UI — `Dia/DiaEditor/UI/`
- `ProjectContext` with `.diagame` lifecycle hooks — `Dia/DiaEditor/`
- `Scene2D` struct + `SceneLoader2D` — `Dia/DiaScene2D/`
- `DiaAssetCatalogue` for blueprint discovery — `Dia/DiaAssetCatalogue/`
- `ComponentRegistry` + `ComponentTypeDesc` for field introspection — `Dia/DiaEntity/`
- `DiaGame` manifest loader for stage enumeration — `Dia/DiaGame/`

**Blocked on:** DiaBlueprintEditor (T1-T3) should ship first so "Open in Blueprint Editor →" has a target. However, DiaSceneEditor can be built independently — the blueprint link is a soft dependency (disabled until DiaBlueprintEditor exists).

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Plugin scaffold: create `Dia/DiaSceneEditor/` project, `DiaSceneEditorPlugin` class, register with `REGISTER_EDITOR_PLUGIN`, add to solution | Plugin appears in EditorPluginRegistry; `GetName()` returns "DiaSceneEditor" | Done | sonnet | All 4 source files + vcxproj + vcxproj.filters + module doc + UI placeholder created; added to Cluiche.sln (Editors folder, GUID F2A3B4C5); wired into CluicheEditor vcxproj (ClCompile + ProjectReference + lib); pipeline.toml deploy rule added; `sceneeditor/` plugin dir deployed. Build: PASSED. |
| 2 | Project context wiring: implement `OnProjectChanged` — parse `.diagame` for stage list, extract `.diastage` → `.diascene` references | Stage dropdown populates when `.diagame` loads | Done | sonnet | `ProjectContextManager::BuildStageListJson` reads diagame imports, resolves stage paths, reads optional `scene` field from each `.diastage`. `scene_editor.get_stage_list` + `scene_editor.load_stage_scene` handlers registered. `mStageList` cached on project change. Build: PASSED. |
| 3 | Scene file handler: implement `SceneFileHandler` — load `.diascene` JSON into `Scene2D` struct, save back | Round-trip: load → save → diff shows no change | Done | sonnet | `SceneFileHandler::Load/Save` implemented with `Json::CharReaderBuilder` + `Json::StreamWriterBuilder`. Covered by T1+T2 commits. Build: PASSED. |
| 4 | Stage/scene toolbar selectors: dropdowns populated from project context, selecting a stage loads its scene | Switch stage → scene hierarchy updates | Not Started | sonnet | WebUIBridge request handlers for stage list + scene load |
| 5 | Scene hierarchy panel: left panel with 4 collapsible sections (Layers/Cameras/Lights/Entities), search filter, selection state | All scene items rendered; filter works; selection highlights | Not Started | sonnet | One section per `Scene2D` array (layers, cameras, lights, entities) |
| 6 | Property inspector — Layer: render layer-specific fields (ID, sort_order, parallax, sort_policy, enabled, assigned lights) | Select layer → shows editable properties | Not Started | sonnet | No blueprint tab for layers — direct field editing |
| 7 | Property inspector — Entity: identity section + all blueprint fields (overridden=editable+purple, non-overridden=dimmed+non-editable) | Select entity → shows instance_data overrides + dimmed defaults | Not Started | opus | Needs `ComponentRegistry` lookup via blueprint's `.diaentity` file to enumerate all possible fields |
| 8 | Property inspector — Camera/Light: same as entity + "Active" enforcement (camera) + "Affects Layers" checkboxes (light) | Select camera → shows active toggle; select light → shows layer checkboxes | Not Started | sonnet | Camera active toggle deactivates other cameras; light checkboxes update `affects_layers` array |
| 9 | Blueprint defaults tab (read-only): sub-tab showing all blueprint fields disabled, "Open in Blueprint Editor →" link | Tab renders read-only; link click fires editor routing event | Not Started | sonnet | Soft dependency on DiaBlueprintEditor — link disabled if not installed |
| 10 | Dirty state + save: track edits, show "* Unsaved" indicator, Save button writes `.diascene` | Edit field → dirty flag → Save → file updated → clean | Not Started | sonnet | SceneFileHandler::Save() serializes current Scene2D state |
| 11 | Entity placement — Add: "+ Entity" button → blueprint picker (from asset catalogue) → new entity with empty instance_data | Add entity → appears in hierarchy with auto-ID | Not Started | sonnet | Query DiaAssetCatalogue for `.diaentity` assets; generate `{blueprint}_{N}` ID |
| 12 | Entity placement — Duplicate: deep copy with `_copy` suffix + position offset | Duplicate → new item below with +50 position | Not Started | haiku | Mechanical: clone JSON, tweak ID + position |
| 13 | Entity placement — Delete + Enable/Disable: confirmation dialog, remove from scene data, toggle enabled | Delete → gone; disable → 50% opacity | Not Started | haiku | Mechanical: splice from array, toggle bool |
| 14 | Entity placement — Rename: inline edit with validation (unique, non-empty, alphanumeric+underscore) | Rename → ID updated in hierarchy + scene data | Not Started | haiku | Mechanical: validate + update |
| 15 | Context menu: right-click → Duplicate/Rename/Change Blueprint/Enable-Disable/Delete | Right-click shows menu; actions dispatch correctly | Not Started | sonnet | WebUIBridge event handler dispatches to appropriate controller |
| 16 | Change Blueprint dialog: picker + transfer analysis + orphan warning + confirm | Change blueprint → overrides transferred/orphaned correctly | Not Started | opus | Transfer logic: check each `instance_data` key against new blueprint's fields; show green/amber counts |
| 17 | Layer authoring — Add/Delete/Reorder: "+ Layer" button, delete with light-assignment warning, sort_order reorder | Add/delete layers; delete warns about affected lights | Not Started | sonnet | Enforce min 1 layer; update light `affects_layers` on delete |
| 18 | Camera/Light authoring — Add/Delete: "+ Camera"/"+Light" with blueprint picker, active camera enforcement | Add camera (inactive by default); active toggle switches exclusively | Not Started | sonnet | Camera: only one active; Light: default `affects_layers` = all layers |
| 19 | Add Override: click dimmed field or "+ Add Override" → promote to instance_data with blueprint default value | Click dimmed field → becomes editable override with purple border | Not Started | sonnet | Copy value from blueprint defaults into `instance_data`; re-render as editable |
| 20 | Scene validation: enforce constraints, display errors/warnings, block save on errors | Duplicate ID → error shown; save disabled until fixed | Not Started | sonnet | Runs on every mutation; check camera count, unique IDs, layer limits |
| 21 | Scene Properties panel: world_bounds editing + summary + validation results display | Toolbar button opens panel; world bounds editable | Not Started | sonnet | Modal/overlay panel; validation results clickable (select offending item) |
| 22 | UI assets: React/HTML for all scene editor panels (hierarchy + properties + dialogs + validation) | UI renders correctly in CluicheEditor CEF panel | Not Started | sonnet | Dark purple theme matching mockup; reuse patterns from DiaAssetCatalogueEditor UI |

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

- **Plugin scaffold:** Same as DiaBlueprintEditor (T1)
- **Project context:** `context.mModel->OnDiagameProjectChanged()` callback pattern
- **Scene2D struct:** Already defined in `Dia/DiaScene2D/Scene2D.h` — load/save via JsonArchive
- **Blueprint field lookup:** Load `.diaentity` → get component list → cross-reference with `instance_data` keys
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
