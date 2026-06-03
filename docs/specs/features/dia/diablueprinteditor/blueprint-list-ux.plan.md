**Spec:** @docs/specs/features/dia/diablueprinteditor/blueprint-list-ux.md
**Status:** Done

## Implementation Plan

### Prerequisites

- `asset_catalogue.query_by_type` handler exists on DiaAssetCatalogueEditor (confirmed)
- `WebUIBridge::InvokeRequestHandler` allows cross-plugin request dispatch (confirmed — used by OnNavigate)
- `WebUIBridge::NotifyUIDataChanged` broadcasts to all plugin UIs (confirmed)
- `DiaEditor_onDataChanged` listener pattern in UI already handles push events (confirmed)

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Remove private registry — query catalogue directly.** Delete `mRegistry` from `DiaBlueprintEditorPlugin`. Rewrite `get_list` handler to call `asset_catalogue.query_by_type` for each blueprint type (diaentity/diacamera/dialight) via `InvokeRequestHandler`, aggregate results into the grouped format the UI expects. Remove `register_catalogue_asset` handler and its unregister call. | `dia run googletest --filter="BlueprintList*"` — list returns blargh when catalogue has it registered; empty when catalogue is empty | Done | sonnet | `mRegistry` deleted. `BlueprintListController::BuildListJson` now takes 3 `Json::Value` record arrays. `QueryCatalogueByType` helper added. `get_usage` now calls `asset_catalogue.get_reverse_refs`. `register_catalogue_asset` and `RegisterAssetTypeHandlers` removed. |
| 2 | **Add `asset_catalogue.registry_changed` notification.** In DiaAssetCatalogueEditor, after successful create/delete/rename operations (CreateRecordCommand, DeleteRecordCommand, UpdateRecordCommand execution), call `NotifyUIDataChanged("asset_catalogue.registry_changed", {})`. | Manually verify: create a record in catalogue UI → DiaBlueprintEditor console shows "registry_changed received" log | Done | sonnet | Added to `PushRegistryState()` — already called on all mutation paths. |
| 3 | **Subscribe to catalogue changes + manual refresh button.** In DiaBlueprintEditor UI: listen for `asset_catalogue.registry_changed` in `DiaEditor_onDataChanged` → call `loadBlueprintList()`. Add a ↻ button in the list panel title bar that also calls `loadBlueprintList()`. | Open both editors → add asset in catalogue → blueprint list updates without manual action | Done | sonnet | ↻ button added to list panel title area. `DiaEditor_onDataChanged` subscribes to `asset_catalogue.registry_changed`. |
| 4 | **Text filter.** Add an `<input>` above the list. On keyup, filter rendered items by case-insensitive substring match on asset ID. Hide empty group headers. Store filter state so it survives list refresh. | Type "bla" → only blargh visible; clear → all visible | Done | haiku | `allGroups` cached on fetch. `applyFilter()` re-renders on every keyup. Selected item highlight preserved across refresh. |
| 5 | ~~**Scope filter (stage dropdown).**~~ | — | Deferred | — | No scene→blueprint relationship data in catalogue yet. Revisit after DiaSceneEditor models these relationships. |
| 6 | **Remove Save button + dirty indicator + "Ready" status.** Delete Save button, dirty indicator span, `setDirty()`, `onSave()` from UI. Remove `setStatus('Ready')` from `init()`. Add 3-second auto-clear on status messages. | UI loads without Save button; status area is blank on init; shows "Saved" briefly after field edit then clears | Done | haiku | Save button, dirty indicator, `setDirty`, `onSave`, `isDirty` all removed. Status auto-clears after 3s. |
| 7 | **Trace logging on all save paths.** Verify `DIA_LOG_INFO` exists in `BlueprintFileHandler::Save()`. Add `DIA_TRACE_ZONE` to Save if missing. Confirm all three mutation handlers (update_field, add_component, remove_component) log on successful save (they already log — verify). | Grep confirms DIA_LOG_INFO in all 3 mutation handler success paths + Save function | Done | haiku | Added `DIA_LOG_INFO("Blueprint saved: '%s'")` to `BlueprintFileHandler::Save()`. All 3 mutation handlers already log on success. |
| 8 | **Update tests.** Fix any tests that reference `mRegistry` or `register_catalogue_asset`. Add integration test: with catalogue populated, `get_list` returns expected items. | `dia run googletest --filter="Blueprint*"` all pass | Done | sonnet | 115/115 pass. `TestBlueprintListController` rewritten for new signature. `TestBlueprintPropertyController` `BuildUsageJson` rewritten for `Json::Value` signature. Integration test adds `catalogue.query_by_type` stub handler. |

### Implementation Order

```
T1 (fix core bug) → T2 (catalogue notification) → T3 (auto-refresh + button) → T4 (text filter)
T5 DEFERRED
T6 (UI cleanup) can run in parallel with T1-T3
T7 (logging verify) can run in parallel with anything
T8 (tests) after T1
```

T1 is the critical path — once it works, the list populates. T2+T3 give auto-refresh. T4+T5 are additive UX. T6+T7 are cosmetic.

### Note: Task 5 deferred

Stage scope filter requires scene→blueprint relationship data that doesn't exist in the catalogue yet. Will become natural to add when DiaSceneEditor models entity placements as catalogue relationships.

### Verification

- `dia pipeline --target cluicheeditor` → open editor → load `.diagame` → Blueprint Editor shows blargh in Entity group
- Add a new `.diaentity` in asset catalogue → Blueprint Editor list updates automatically
- Type in filter → list narrows; clear → list restores
- Select blargh → components/fields appear → edit a field → "Saved" appears briefly → no Save button
