# Plan: shared-asset-creation

**Spec:** @docs/specs/applications/dia/systems/diaassetcatalogueeditor/shared-asset-creation.md
**Status:** Done

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add asset template registry to DiaAssetCatalogueEditorPlugin | Verify `get_asset_types` still returns all types; templates populated at load | Done | sonnet | `AssetTemplate` struct, `mAssetTemplates` map, `SeedAssetTemplates()` seeded with 4 types. Called from `OnPluginLoad()`. |
| 2 | Implement `asset_catalogue.create_asset` handler | `dia run googletest` + manual: call with each type, verify file written + record created | Done | sonnet | Type-agnostic handler: looks up template, writes file, `CreateRecordCommand`, auto-save. Returns `{success, id, absPath}` — no auto-open. |
| 3 | Redirect `asset_catalogue.create_scene` to `create_asset` | Existing Scene Editor "+New Scene" still works end-to-end | Done | haiku | Thin wrapper: adds `assetType=diascene`, forwards to `create_asset`, then opens DiaSceneEditor. |
| 4 | Asset Catalogue UI: New dialog uses `create_asset` for template types | Open Asset Catalogue, create a Scene via dropdown, verify file + record | Done | sonnet | `templateTypes = ['diascene', 'diaentitytemplate', 'diacamera', 'dialight']` branch in `onCreateRecord()`. |
| 5 | DiaSceneEditor: stage dropdown grey styling for sceneless stages | Load diagame, verify stages without `scene` field show greyed + "(no scene)" | Done | haiku | CSS `--pico-muted-color` + "(no scene)" suffix in `populateStageDropdown`. |
| 6 | DiaSceneEditor: "+New Scene" uses `create_asset` + stage association | Create scene on a sceneless stage → `.diastage` updated → stage reloads with scene | Done | sonnet | `scene_editor.create_asset` forwarder + `scene_editor.associate_scene_to_stage` handler (reads/writes `.diastage`, rebuilds stage list). JS btn calls both. |
| 7 | DiaSceneEditor: empty-stage canvas with "Create Scene" CTA | Select sceneless stage → shows prominent action instead of blank hierarchy | Done | haiku | `.no-scene-overlay` div + `btn-create-scene-cta` delegates to `btn-new-scene`. |
| 8 | DiaEntityTemplateEditor: "+New Template" button | Click button → new `.diaentitytemplate` created + navigated to | Done | sonnet | Toolbar button → prompt name/path → `asset_catalogue.create_asset` → `selectBlueprint` + `loadBlueprintList`. |
| 9 | DiaEntityTemplateEditor: nav-failure "Create" uses `create_asset` | Navigate to missing template → create → file + record created, editor loads it | Done | haiku | `navFailCreateFile()` now calls `asset_catalogue.create_asset` directly. |
| 10 | Remove `entity_template_editor.create_from_template` handler | `dia run googletest` passes; grep confirms no remaining callers | Done | sonnet | 8 tests migrated to `IntegrationTestAssetCatalogueEditorPlugin.cpp` under `create_asset`; handler deleted from `DiaBlueprintEditorPlugin.cpp`. |

## Dependency Graph

```
Task 1 ──→ Task 2 ──→ Task 3 (alias)
                  ├──→ Task 4 (catalogue UI)
                  ├──→ Task 6 (scene editor create + associate)
                  ├──→ Task 8 (blueprint editor button)
                  └──→ Task 9 (blueprint nav-fail)

Task 5 (styling) — independent
Task 7 (empty canvas) — independent, pairs with 6
Task 10 — deferred (after test migration)
```

## Key Implementation Details

### Task 1 — Template Registry

```cpp
struct AssetTemplate { const char* content; const char* extension; };
std::unordered_map<Dia::Core::StringCRC, AssetTemplate> mAssetTemplates;
```

Seeded in `SeedAssetTemplates()` called from `OnPluginLoad()`.

### Task 2 — `create_asset` handler

Extracted from `create_scene`. Parameterized by `assetType`. Returns `{success, id, absPath}`.

### Task 6 — Stage association write-back

`scene_editor.associate_scene_to_stage` reads `.diastage` JSON, writes `"scene"` field with relative path, saves back, rebuilds `mStageList`, pushes `project_changed` to UI.

### Task 8 — Blueprint Editor "+New Template"

Toolbar button prompts for name/path → `asset_catalogue.create_asset` → `selectBlueprint(id, absPath)` + `loadBlueprintList()`.

### Task 10 — Deferred

`create_from_template` still registered; 8 integration tests test it directly. Removing requires either: (a) deleting the tests, or (b) migrating them to test `create_asset` through a full mock catalogue. Neither is trivial. Deferred as a separate cleanup item.
