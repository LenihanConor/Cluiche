**Spec:** @docs/specs/applications/dia/systems/diaeditor/editor-deep-link.md
**Status:** Done

## Implementation Patterns

### OnNavigate hook
- Added as virtual method with default no-op to `IEditorPlugin` — backward compatible
- PluginLoaderModule calls it in two places: (1) after OnLoad on fresh load, (2) instead of reloading when already loaded

### AppEditor navigation
- Pushes `app_editor.navigate_to_stage` via NotifyUIDataChanged
- Stage name extracted from instanceId string by splitting on first '.'
- React StagesTab listens for this event and scrolls/highlights

### Blueprint/Scene navigation
- Uses `asset_catalogue.get_record` to resolve instanceId → source_path
- Then invokes existing load handler internally (self-call via InvokeRequestHandler)
- Graceful no-op if catalogue not loaded

### Catalogue navigation
- Pushes `asset_catalogue.navigate_to_record` via NotifyUIDataChanged
- UI listener clears type filter, sets selectedId, re-renders, scrolls row into view

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `OnNavigate` default no-op to `IEditorPlugin` | Default impl compiles; existing plugins unaffected | Done | haiku | Backward compatible — default no-op |
| 2 | Add `asset_catalogue.get_record` handler | Handler returns record by id; returns error for unknown id | Done | haiku | Used by Blueprint + Scene OnNavigate for source path lookup |
| 3 | PluginLoaderModule: call OnNavigate on fresh load and on already-loaded | OnNavigate called in both paths | Done | haiku | Already-loaded path was previously a silent skip |
| 4 | DiaApplicationFlowEditorPlugin::OnNavigate | Pushes app_editor.navigate_to_stage; no crash on missing manifest | Done | sonnet | Extracts stage name from instanceId by splitting on '.' |
| 5 | DiaEntityTemplateEditorPlugin::OnNavigate | Loads blueprint via get_record + entity_template_editor.load; no crash when catalogue absent | Done | sonnet | Graceful fallback; logs warning |
| 6 | DiaSceneEditorPlugin::OnNavigate | Loads scene via get_record + scene_editor.load_scene; graceful fallback | Done | sonnet | Same pattern as Blueprint |
| 7 | DiaAssetCatalogueEditorPlugin::OnNavigate | Pushes asset_catalogue.navigate_to_record; no crash for unknown id | Done | sonnet | Verifies record exists before pushing |
| 8 | DiaApplicationEditor UI: handle navigate_to_stage | Stage scrolls into view and briefly highlights | Done | sonnet | CSS transition fades highlight after 1.5s |
| 9 | DiaAssetCatalogueEditor UI: handle navigate_to_record | Clears filter, selects row, scrolls into view | Done | haiku | Resets type filter so record is always visible |
| 10 | Tests: IntegrationTestEditorDeepLink.cpp (12 tests) | No-crash tests for all 4 OnNavigate implementations | Done | sonnet | Null UISystem — tests code paths not UI output |
