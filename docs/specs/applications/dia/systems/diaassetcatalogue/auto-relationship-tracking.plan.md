**Spec:** @docs/specs/applications/dia/systems/diaassetcatalogue/auto-relationship-tracking.md
**Status:** Done

## Implementation Patterns

### Editor-Driven (DiaSceneEditor)

The `scene_editor.add_item` handler (DiaSceneEditorPlugin.cpp ~line 598) already receives `blueprintId` in the request payload. After a successful `SceneMutator::AddItem()`, invoke `asset_catalogue.add_relationship` via `mBridge->InvokeRequestHandler()` with:
- `from`: the scene's catalogue ID (derived from `mLoadedScenePath` → lookup in catalogue by source_path)
- `rel`: `"uses"`
- `to`: the `blueprintId` from the request

Similarly, `scene_editor.delete_item` (line 674) should call `asset_catalogue.remove_relationship` after `SceneMutator::DeleteItem()`. The scene's catalogue ID needs to be resolved once on load and cached.

For `scene_editor.change_blueprint` (T16), call `remove_relationship` for the old blueprint and `add_relationship` for the new one.

**Scene catalogue ID resolution:** On scene load (`scene_editor.load_scene` / stage-load), query `asset_catalogue.query_asset_ids` with the loaded scene path to find the matching record ID. Cache as `mSceneCatalogueId`. If not found (scene not in catalogue), skip relationship calls silently.

### Scan-Based Inferrer

A new request handler `asset_catalogue.infer_relationships` on DiaAssetCatalogueEditorPlugin. For each record of type `diascene`:
1. Resolve the `.diascene` file path
2. Parse JSON, iterate `scene2d.entities[]`, `scene2d.cameras[]`, `scene2d.lights[]`
3. Extract `item.blueprint.value` from each
4. Map blueprint ID to catalogue record ID (prefix with type: `diaentitytemplate.`, `diacamera.`, `dialight.` based on array)
5. Call `add_relationship` for each (idempotent — duplicates rejected)
6. Return summary: `{ scenes_scanned, edges_added, edges_skipped }`

The UI's Validate tab can surface this as a "Reconcile References" button, or it can run as part of the existing Validate flow.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Cache scene catalogue ID on load in DiaSceneEditor | Manual: load scene, check log for resolved ID | Done | sonnet | Add `mSceneCatalogueId` member, resolve in load_scene/stage-load handlers |
| 2 | Call add_relationship in scene_editor.add_item | Unit: add item, verify forward ref appears in catalogue | Done | sonnet | After SceneMutator::AddItem succeeds; skip if mSceneCatalogueId empty |
| 3 | Call remove_relationship in scene_editor.delete_item | Unit: delete item, verify forward ref removed | Done | sonnet | After SceneMutator::DeleteItem succeeds |
| 4 | Call remove+add in scene_editor.change_blueprint | Unit: change blueprint, verify old ref gone + new ref present | Done | sonnet | T16 handler already has old/new blueprint IDs |
| 5 | Implement asset_catalogue.infer_relationships handler | Unit: create scene file with refs, run inferrer, check edges | Done | sonnet | New handler on DiaAssetCatalogueEditorPlugin |
| 6 | Wire inferrer to Validate UI (button or auto-run in validate) | Manual: click button, see summary | Done | haiku | Add "Reconcile" button or integrate into onValidate flow |
