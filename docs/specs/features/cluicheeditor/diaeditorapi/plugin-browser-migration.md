# Feature Spec: plugin-browser-migration

**Parent:** @docs/specs/systems/cluicheeditor/diaeditorapi.md
**Status:** Done

## Summary

Completes the `plugin_browser.*` DiaEditorAPI surface. `load` and `unload` are already dual-registered; this feature adds the missing `get_available` action and ensures the deregistration path is wired up correctly on plugin unload. After this feature all three plugin-browser actions are callable from Python via `dia_editor.plugin_browser.*`.

## Problem

`plugin_browser.load` and `plugin_browser.unload` were dual-registered during the initial DiaEditorAPI scaffolding, but `plugin_browser.get_available` was not — it remains WebUIBridge-only. There is also no `DeregisterActionsForOwner` call in `OnPluginUnload`, so the two registered actions become stale if the plugin is unloaded and reloaded.

## Goals

1. Register `plugin_browser.get_available` via DiaEditorAPI.
2. Add `DeregisterActionsForOwner` call on plugin unload.
3. Confirm the existing `load` and `unload` registrations match the Action Manifest format expected by Python gen.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | All 3 `plugin_browser.*` actions appear in `GetManifest()` output |
| AC2 | All 3 are callable from Python via `dia_editor.plugin_browser.*` |
| AC3 | `get_available` returns correct `loaded` and `pinned` flags for each registered plugin type |
| AC4 | `DeregisterActionsForOwner(StringCRC("PluginBrowserEditorPlugin"))` is called in `OnPluginUnload` |
| AC5 | Python smoke test: `dia_editor.plugin_browser.get_available()` returns a non-empty list |

## Action Manifest

### Thread assignments

**kCallerThread (read-only, 1 action):**
`get_available`

**kMainThread (mutating, 2 actions):**
`load`, `unload`

---

### `plugin_browser.get_available`

```
description:
  Returns the list of all registered editor plugin types with their current load state.
  Each entry includes the plugin's name, version, description, type ID, whether it is
  currently loaded, and whether it is pinned (pinned plugins cannot be unloaded). Call
  this to discover valid typeId values for plugin_browser.load and plugin_browser.unload.

category:  plugin_browser
owner:     PluginBrowserEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "plugins": [{ "name": string, "version": string, "description": string, "typeId": string, "loaded": bool, "pinned": bool }...] }
```

---

### `plugin_browser.load`

```
description:
  Loads a plugin by type ID, creating a new instance in the editor. Returns an error
  if the plugin is already loaded, if the type ID is not registered, or if no plugin
  loader is available. Use get_available to enumerate valid typeId values.

category:  plugin_browser
owner:     PluginBrowserEditorPlugin
dispatch:  kMainThread
params:
  typeId  string  required  "Plugin type ID as returned by get_available, e.g. 'DiaSceneEditorPlugin'."
returns:   { "ok": bool, "error"?: string }
```

---

### `plugin_browser.unload`

```
description:
  Unloads a currently-loaded plugin by type ID. Returns an error if the plugin is not
  loaded, if it is pinned (built-in plugins cannot be unloaded), or if unload fails.
  Use get_available to check loaded and pinned flags before calling.

category:  plugin_browser
owner:     PluginBrowserEditorPlugin
dispatch:  kMainThread
params:
  typeId  string  required  "Plugin type ID to unload."
returns:   { "ok": bool, "error"?: string }
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Register `plugin_browser.get_available` via `EditorActionRegistry`; call from `OnPluginLoad()` alongside existing load/unload registrations | `GetManifest()` lists all 3 `plugin_browser.*` actions | — | haiku | |
| 2 | Add `DeregisterActionsForOwner(StringCRC("PluginBrowserEditorPlugin"))` in `OnPluginUnload()` | On unload, `GetManifest()` no longer lists `plugin_browser.*` actions | — | haiku | |
| 3 | Verify existing `load` and `unload` registrations have correct descriptors (description, params, dispatch policy) matching this spec; patch if they diverge | Descriptors match Action Manifest above | — | haiku | |
| 4 | Python smoke test: `get_available()` returns non-empty list with correct field shape | Smoke test passes | — | haiku | |

## Modules Touched

| Module | Change |
|--------|--------|
| `Dia/DiaEditor/Plugin/PluginBrowserEditorPlugin` | Register `get_available`; add `DeregisterActionsForOwner` in unload; audit existing descriptors |

## Binding Decisions

No binding constraints beyond the DiaEditorAPI system spec (EAPI-001 through EAPI-009) apply.

## Open Design Questions

| # | Question |
|---|----------|
| ODQ-1 | The existing `load` and `unload` registrations were added as a proof-of-concept; their descriptors may not match the Action Manifest format expected by Python gen (description, param schema, owner). **Resolved:** treat any mismatch as a bug — fix in task 3. Wrong descriptors mean broken `.pyi` stubs and bad MCP `tools/list` output right now. |
