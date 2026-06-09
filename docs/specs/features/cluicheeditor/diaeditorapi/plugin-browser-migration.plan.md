**Spec:** @docs/specs/features/cluicheeditor/diaeditorapi/plugin-browser-migration.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Register `plugin_browser.get_available` via `EditorActionRegistry`; call from `OnPluginLoad()` alongside existing load/unload registrations | `GetManifest()` lists all 3 `plugin_browser.*` actions | Pending | haiku | |
| 2 | Add `DeregisterActionsForOwner(StringCRC("PluginBrowserEditorPlugin"))` in `OnPluginUnload()` | On unload, `GetManifest()` no longer lists `plugin_browser.*` actions | Pending | haiku | |
| 3 | Verify existing `load` and `unload` registrations have correct descriptors (description, params, dispatch policy) matching this spec; patch if they diverge | Descriptors match Action Manifest above | Pending | haiku | |
| 4 | Python smoke test: `get_available()` returns non-empty list with correct field shape | Smoke test passes | Pending | haiku | |
