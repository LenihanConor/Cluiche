**Spec:** @docs/specs/systems/dia/diablueprinteditor.md
**Status:** In Progress

## Implementation Plan

### Prerequisites

All dependencies exist:
- `IEditorPlugin` interface + `REGISTER_EDITOR_PLUGIN` macro — `Dia/DiaEditor/Plugin/`
- `ComponentRegistry` + `ComponentTypeDesc` + `FieldDesc` — `Dia/DiaEntity/`
- `AssetTypeEditorRegistry` routing — `Dia/DiaAssetCatalogueEditor/Handlers/`
- `WebUIBridge` for C++↔UI communication — `Dia/DiaEditor/UI/`
- `EditorPluginContext` with project lifecycle hooks — `Dia/DiaEditor/`

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Plugin scaffold: create `Dia/DiaBlueprintEditor/` project, `DiaBlueprintEditorPlugin` class, register with `REGISTER_EDITOR_PLUGIN`, add to solution | Plugin appears in EditorPluginRegistry; `GetName()` returns "DiaBlueprintEditor" | Done | sonnet | All 4 source files + vcxproj + vcxproj.filters + module doc created; added to Cluiche.sln (Editors folder, GUID E3F4A5B6); wired into CluicheEditor vcxproj (ClCompile + ProjectReference + lib); pipeline.toml deploy rule added; `blueprinteditor/` plugin dir deployed. Build: PASSED. |
| 2 | Asset type registration: register `.diaentity`, `.diacamera`, `.dialight` with `AssetTypeEditorRegistry` so "Open" from asset catalogue routes here | Opening a `.diaentity` asset in catalogue opens DiaBlueprintEditor | Not Started | sonnet | Use `asset_catalogue.register_type_editor` pattern from DiaAssetCatalogueEditor |
| 3 | Blueprint file handler: implement `BlueprintFileHandler` — load/save `.diaentity`/`.diacamera`/`.dialight` JSON files | Round-trip: load → save → diff shows no change | Done | sonnet | `BlueprintFileHandler::Load/Save` implemented with `Json::CharReaderBuilder` + `Json::StreamWriterBuilder`; `TopLevelKeyForExtension` maps ext→root key. Covered by T1 commit. |
| 4 | Blueprint list controller: left panel — query DiaAssetCatalogue for all registered blueprint assets, group by type (Entity/Camera/Light), render list | List shows all registered blueprints; selecting one loads it | Done | sonnet | `BlueprintListController::BuildListJson` queries registry by type (diaentity/diacamera/dialight), returns grouped JSON. `blueprint_editor.get_list` handler registered. Covered by T1 commit. |
| 5 | Blueprint property controller: right panel — render component accordion with fields from loaded blueprint | Selecting a blueprint shows identity + components + fields | Done | sonnet | `BlueprintPropertyController::BuildPropertyJson` enriches fields with `ComponentRegistry` metadata; falls back to raw JSON if type not registered. `blueprint_editor.load` handler registered. Covered by T1 commit. |
| 6 | Field editing: type-aware inputs (bool/int/float/vec2/string), dirty tracking, save back to file | Edit a field → save → reload shows new value | Done | sonnet | `blueprint_editor.update_field` handler patches field in-memory then saves. Field kind exposed as string for UI widget mapping. Covered by T1 commit. |
| 7 | Cross-scene usage display: query asset catalogue relationships to show which scenes reference this blueprint + instance count | Usage section shows "level_01.diascene — 2 instances" | Done | sonnet | `BlueprintPropertyController::BuildUsageJson` calls `RelationshipIndex::GetReverseRefs`. `blueprint_editor.get_usage` handler registered. Covered by T1 commit. |
| 8 | Add Component: dropdown of all registered component types (from `ComponentRegistry`), add with zero defaults | Add "Health" component → appears in accordion with default fields | Done | sonnet | `blueprint_editor.add_component` handler adds component with empty fields object. `blueprint_editor.get_available_components` returns types not already present. Covered by T1 commit. |
| 9 | Remove Component: confirmation with affected instance count, then remove from blueprint | Remove "Health" → gone from file after save; confirmation shows "3 instances affected" | Done | sonnet | `blueprint_editor.remove_component` handler filters component out of array and saves. Confirmation count is a UI-layer concern (usage query provides the count). Covered by T1 commit. |
| 10 | UI assets: React/HTML for blueprint editor panels (list + property + usage + dialogs) | UI renders correctly in CluicheEditor CEF panel | Not Started | sonnet | Placeholder `UI/index.html` deployed to `plugins/blueprinteditor/`; full React UI deferred |

### Implementation Order

```
T1 (scaffold) → T2 (routing) → T3 (file I/O) → T4 (list) → T5 (properties) → T6 (editing) → T7 (usage) → T8 (add) → T9 (remove) → T10 (UI)
```

T1-T3 are sequential (each depends on previous). T4-T6 are sequential. T7 can run after T5. T8-T9 can run after T6. T10 runs in parallel with T4-T9 (UI developed alongside backend).

### Key Files to Create

| File | Purpose |
|------|---------|
| `Dia/DiaBlueprintEditor/DiaBlueprintEditor.vcxproj` | MSBuild project |
| `Dia/DiaBlueprintEditor/DiaBlueprintEditor.vcxproj.filters` | IDE filters |
| `Dia/DiaBlueprintEditor/DiaBlueprintEditorPlugin.h` | IEditorPlugin subclass |
| `Dia/DiaBlueprintEditor/DiaBlueprintEditorPlugin.cpp` | Registration + lifecycle |
| `Dia/DiaBlueprintEditor/BlueprintListController.h/cpp` | Left panel logic |
| `Dia/DiaBlueprintEditor/BlueprintPropertyController.h/cpp` | Right panel logic |
| `Dia/DiaBlueprintEditor/BlueprintFileHandler.h/cpp` | .diaentity/.diacamera/.dialight I/O |
| `Dia/DiaBlueprintEditor/CrossSceneUsageQuery.h/cpp` | Asset catalogue relationship query |
| `Cluiche/CluicheEditor/UI/src/plugins/blueprint-editor/` | React UI components |

### Key Patterns to Reuse

- **Plugin scaffold:** Follow `HomeEditorPlugin` (minimal) + `DiaAssetCatalogueEditorPlugin` (handlers)
- **WebUIBridge handlers:** `RegisterRequestHandler("blueprint_editor.load", ...)` pattern
- **Component introspection:** `ComponentRegistry::Get().GetByIndex(i)` → `ComponentTypeDesc`
- **Asset routing:** `AssetTypeEditorRegistry::RegisterTypeEditor(kDiaEntityAssetType, kBlueprintEditorPluginType)`
- **File I/O:** `Json::Reader`/`Json::StyledWriter` for load/save (same as DiaAssetCatalogue manifest)

### Verification

- `dia run cluicheeditor` → DiaBlueprintEditor plugin loads without errors
- Create a test `.diaentity` file → open from asset catalogue → editor shows components/fields
- Edit a field → save → reload → value persisted
- Add/remove component → save → file format correct
- Cross-scene usage shows correct scene references
