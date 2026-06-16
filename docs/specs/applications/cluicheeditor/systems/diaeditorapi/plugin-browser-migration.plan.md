**Spec:** @docs/specs/applications/cluicheeditor/systems/diaeditorapi/plugin-browser-migration.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Register `plugin_browser.get_available` via `EditorActionRegistry`; call from `OnPluginLoad()` alongside existing load/unload registrations | `GetManifest()` lists all 3 `plugin_browser.*` actions | Done | haiku | |
| 2 | Add `DeregisterActionsForOwner(StringCRC("PluginBrowserEditorPlugin"))` in `OnPluginUnload()` | On unload, `GetManifest()` no longer lists `plugin_browser.*` actions | Done | haiku | |
| 3 | Verify existing `load` and `unload` registrations have correct descriptors (description, params, dispatch policy) matching this spec; patch if they diverge | Descriptors match Action Manifest above | Done | haiku | Fixed category plugin→plugin_browser, added typeId param schema, updated descriptions; also fixed PushBack→Add compile error |
| 4 | Python smoke test: `get_available()` returns non-empty list with correct field shape | Smoke test passes | Done | haiku | Added to smoke_test.py |
