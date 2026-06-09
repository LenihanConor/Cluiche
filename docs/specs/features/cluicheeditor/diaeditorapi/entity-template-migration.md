# Feature Spec: entity-template-migration

**Parent:** @docs/specs/systems/cluicheeditor/diaeditorapi.md
**Status:** Approved

## Summary

Dual-registers all 9 existing `entity_template_editor.*` WebUIBridge handlers in `DiaBlueprintEditorPlugin` with the `EditorActionRegistry`. No handler logic changes. After migration, every entity template operation is callable from Python via `dia_editor.entity_template_editor.*`.

## Problem

The entity template editor has 9 WebUIBridge handlers covering project state, template listing, component inspection, file I/O, field editing, and component add/remove. None are reachable from Python or the AI layer. Automation tests cannot create, inspect, or modify entity templates programmatically.

## Goals

1. Expose all 9 existing handlers through DiaEditorAPI with correct thread dispatch.
2. No existing WebUIBridge behaviour changes — dual-registration only.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | All 9 handlers are registered in `EditorActionRegistry` and appear in `GetManifest()` output |
| AC2 | All 9 are callable from Python via `dia_editor.entity_template_editor.*` |
| AC3 | All 9 handlers use `kMainThread` (shared `mFileHandler`/`mSchemaReader` are not thread-safe) |
| AC4 | `DeregisterActionsForOwner(StringCRC("DiaBlueprintEditorPlugin"))` removes all actions on plugin unload |
| AC5 | Python smoke test: `dia_editor.entity_template_editor.get_project_state()` returns expected shape |

## Action Manifest

### Thread assignments

**kMainThread (all 9 actions):**
All actions use `kMainThread`. The read-only handlers (`get_project_state`, `get_list`, `get_available_components`, `get_usage`, `load`) share `mFileHandler` and `mSchemaReader` with the mutating handlers — these plugin members are not thread-safe, so concurrent `kCallerThread` access would race with main-thread mutations.

---

### `entity_template_editor.get_project_state`

```
description:
  Returns whether a valid project is loaded and the path to the active .diagame file.
  Call this before any other entity_template_editor action to confirm the plugin has
  a project context. isValid is false if no .diagame has been set.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "isValid": bool, "diagamePath": string }
```

---

### `entity_template_editor.get_list`

```
description:
  Returns a list of all entity templates, cameras, and lights registered in the asset
  catalogue for the current project. Use this to enumerate available templates before
  calling load or get_available_components.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:    none
returns:   JSON array of entity template entries (id, name, type, source_path per entry)
```

---

### `entity_template_editor.get_available_components`

```
description:
  Loads the blueprint file at path and returns the list of component types available
  to be added to it — components registered in the schema but not yet present in the
  file. Use before calling add_component to know what can be added.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  path  string  required  "Absolute or catalogue-relative path to the .diablueprint file."
returns:   { "success": bool, "components": [string...], "error"?: string }
```

---

### `entity_template_editor.get_usage`

```
description:
  Returns a list of all assets that reference the given asset ID — the reverse
  relationship edges from the asset catalogue. Use before deleting a template to
  find what scenes or other templates depend on it.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  assetId  string  required  "The asset ID to check usage for."
returns:   { "success": bool, "usage": [{ "rel": string, "source": string }...], "error"?: string }
```

---

### `entity_template_editor.load`

```
description:
  Loads a blueprint file from disk and returns its full property structure: component
  list and field values. This is a read-only operation — it does not open the file
  in the editor UI. Use get_available_components afterwards to see what can still be added.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  path  string  required  "Absolute or catalogue-relative path to the .diablueprint file."
returns:   { "success": bool, "properties": object, "error"?: string }
```

---

### `entity_template_editor.save`

```
description:
  Serialises the given blueprint structure to disk at the specified path. The blueprint
  parameter must be the full JSON structure as returned by load (or as modified by
  update_field, add_component, remove_component). Overwrites the existing file.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  path       string  required  "Absolute path to write the .diablueprint file."
  blueprint  object  required  "Full blueprint JSON structure to persist."
returns:   { "success": bool, "error"?: string }
```

---

### `entity_template_editor.update_field`

```
description:
  Updates a single component field in a blueprint file. Loads the file, applies the
  change via BlueprintMutator, and saves immediately. Passing null as value removes
  the field (sets it to the component default). The path + componentType + fieldName
  triple uniquely identifies the target field.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  path           string  required  "Absolute path to the .diablueprint file."
  componentType  string  required  "Component type name, e.g. 'TransformComponent'."
  fieldName      string  required  "Field name within the component."
  value          any     required  "New value. Pass null to reset to component default."
returns:   { "success": bool, "error"?: string }
```

---

### `entity_template_editor.add_component`

```
description:
  Adds a component of the given type to the blueprint file. The component is initialised
  with its schema defaults. Loads the file, applies the change via BlueprintMutator, and
  saves immediately. Returns an error if the component type is already present or unknown.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  path           string  required  "Absolute path to the .diablueprint file."
  componentType  string  required  "Component type to add. Use get_available_components to enumerate valid types."
returns:   { "success": bool, "error"?: string }
```

---

### `entity_template_editor.remove_component`

```
description:
  Removes a component from the blueprint file. All field values for that component are
  discarded. Loads the file, applies the change via BlueprintMutator, and saves
  immediately. Returns an error if the component type is not present.

category:  entity_template_editor
owner:     DiaBlueprintEditorPlugin
dispatch:  kMainThread
params:
  path           string  required  "Absolute path to the .diablueprint file."
  componentType  string  required  "Component type to remove."
returns:   { "success": bool, "error"?: string }
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` to `DiaBlueprintEditorPlugin`; call from `OnPluginLoad()` after existing `RegisterHandler` calls; call `DeregisterActionsForOwner` in `OnPluginUnload()` | `GetManifest()` lists all 9 `entity_template_editor.*` actions | — | sonnet | |
| 2 | Register 5 kCallerThread actions: `get_project_state`, `get_list`, `get_available_components`, `get_usage`, `load` | Python: `dia_editor.entity_template_editor.get_project_state()` returns expected shape | — | haiku | |
| 3 | Register 4 kMainThread actions: `save`, `update_field`, `add_component`, `remove_component` | Python: `dia_editor.entity_template_editor.add_component(path, type)` adds component and file reflects change | — | sonnet | |
| 4 | Python smoke test: `get_project_state`, `get_list` | Smoke test passes; no errors in output | — | haiku | |

## Modules Touched

| Module | Change |
|--------|--------|
| `Dia/DiaEntityTemplateEditor` | `DualRegisterActions()` + deregister in unload |

## Binding Decisions

No binding constraints beyond the DiaEditorAPI system spec (EAPI-001 through EAPI-009) apply.

## Open Design Questions

| # | Question |
|---|----------|
| ODQ-1 | `load` and other nominally read-only handlers use `mFileHandler` and `mSchemaReader` — plugin member variables not safe for concurrent access. **Resolved:** all 9 actions use `kMainThread`. |
| ODQ-2 | The outbound push events (`entity_template_editor.navigated`, `entity_template_editor.navigate_failed`, `entity_template_editor.project_changed`) are C++→JS notifications — not request handlers, so they cannot be registered as DiaEditorAPI actions. **Resolved:** intentional exclusions. Navigation *to* an entity template is covered by `app_editor.navigate_to_entity` (already implemented). These push events are JS-only observation signals; they cannot be called or awaited from Python. |
