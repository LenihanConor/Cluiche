**Spec:** @docs/specs/applications/cluicheeditor/systems/diaeditorapi/asset-catalogue-migration.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` to `DiaAssetCatalogueEditorPlugin`; call from `OnPluginLoad()` after `RegisterRequestHandlers()`; add `DeregisterActionsForOwner` to `OnPluginUnload()` | `GetManifest()` lists all 34 `asset_catalogue.*` actions | Done | sonnet | PluginServiceLocator.h include required for GetService<T> template instantiation |
| 2 | Register 14 kCallerThread actions (read-only handlers) | Python: `dia_editor.asset_catalogue.get_state()` returns expected shape | Done | sonnet |  |
| 3 | Register 19 kMainThread actions (mutating handlers) + `get_available` new action | Python: `dia_editor.asset_catalogue.create_record(...)` creates a record | Done | sonnet |  |
| 4 | Python smoke test: `get_state`, `query_by_type`, `validate` | Smoke test passes | Done | haiku |  |
| 5 | Commit and update plan/spec | Build passes | Done | haiku |  |
