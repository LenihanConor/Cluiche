# Feature Spec: asset-catalogue-migration

**Parent:** @docs/specs/applications/cluicheeditor/systems/diaeditorapi/diaeditorapi.md
**Status:** Done

## Summary

Dual-registers all 33 existing `asset_catalogue.*` WebUIBridge handlers in `DiaAssetCatalogueEditorPlugin` with the `EditorActionRegistry`. No handler logic changes — the same lambda executes whether the caller is JS, Python, or MCP. Adds `get_available` as a new thin action (not currently a handler). After migration every asset catalogue operation is callable from Python via `dia_editor.asset_catalogue.*`.

## Problem

The asset catalogue has 33 WebUIBridge handlers covering the full lifecycle of manifest, record, relationship, rule, and file operations. None are reachable from Python or the AI layer. Automation tests and the chat agent must either scrape the UI or have no access at all.

## Goals

1. Expose all 33 existing handlers through DiaEditorAPI with correct thread dispatch.
2. Add one new action: `asset_catalogue.get_available` — checks whether the catalogue plugin is loaded.
3. No existing WebUIBridge behaviour changes — dual-registration only.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | All 33 handlers are registered in `EditorActionRegistry` and appear in `GetManifest()` output |
| AC2 | All 33 are callable from Python via `dia_editor.asset_catalogue.*` |
| AC3 | `asset_catalogue.get_available` returns `{ "loaded": true/false }` |
| AC4 | Read-only handlers use `kCallerThread`; mutating handlers use `kMainThread` (see thread column in Action Manifest) |
| AC5 | `DeregisterActionsForOwner(StringCRC("DiaAssetCatalogueEditorPlugin"))` removes all actions on plugin unload |
| AC6 | Python smoke test: `dia_editor.asset_catalogue.get_state()` returns expected shape when a manifest is open |

## Action Manifest

### Thread assignments

**kCallerThread (read-only, 14 actions):**
`get_state`, `get_manifest_dir`, `browse_source_file`, `get_forward_refs`, `get_reverse_refs`, `discover_files`, `dry_run_rules`, `get_rules`, `query_asset_ids`, `get_asset_types`, `get_record`, `validate`, `query_by_type`, `query_by_tag`

**kMainThread (mutating, 19 actions):**
`load_manifest`, `browse_open`, `save_manifest`, `new_manifest`, `add_relationship`, `remove_relationship`, `browse_rules`, `load_rules`, `apply_rules`, `register_type_editor`, `create_scene`, `create_asset`, `open_in_editor`, `open_in_file`, `create_record`, `update_record`, `delete_record`, `bulk_create_records`, `infer_relationships`

---

### `asset_catalogue.get_available`

```
description:
  Returns whether the Asset Catalogue plugin is currently loaded and ready. Call this
  before any asset_catalogue.* action to confirm the plugin is active. If not loaded,
  use plugin_browser.load to activate it first.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "loaded": bool }
```

---

### `asset_catalogue.get_state`

```
description:
  Returns the full current state of the asset catalogue: the manifest path, dirty flag,
  and the complete list of records. Use this to snapshot the catalogue before making
  bulk changes, or to verify the effect of a previous mutation. Returns an empty records
  array if no manifest is loaded.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": true, "path": string, "dirty": bool, "records": [record...] }
```

---

### `asset_catalogue.load_manifest`

```
description:
  Loads an asset catalogue manifest from the given file path. Replaces the current
  manifest in memory; any unsaved changes are discarded. The path must be an absolute
  path to a valid .diacatalogue file.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  path  string  required  "Absolute path to the .diacatalogue manifest file."
returns:   { "success": bool, "record_count"?: int, "error"?: string }
```

---

### `asset_catalogue.browse_open`

```
description:
  Opens a file-picker dialog for the user to select an asset catalogue manifest.
  Equivalent to File > Open in the catalogue UI. Requires an active window; do not
  call from headless scripts.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.save_manifest`

```
description:
  Saves the current in-memory catalogue to disk. If path is provided it saves to that
  location (Save As); otherwise saves to the currently loaded path. Returns an error
  if no path is known and none is provided.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  path  string  optional  "Absolute save path. Omit to save to the current path."
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.new_manifest`

```
description:
  Clears the in-memory catalogue and creates a fresh empty manifest. Any unsaved
  changes to the previous manifest are discarded. Call save_manifest with a new path
  afterwards to persist.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "success": true }
```

---

### `asset_catalogue.get_manifest_dir`

```
description:
  Returns the directory that contains the currently loaded manifest file. Use this as
  the base when constructing relative asset source paths.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": true, "dir": string }
```

---

### `asset_catalogue.browse_source_file`

```
description:
  Opens a file-picker dialog and returns the selected file path both as an absolute
  path and as a path relative to the manifest directory. Use when the user needs to
  point a record at a source file on disk.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  initial_dir  string  optional  "Directory to open the picker in. Defaults to manifest dir."
returns:   { "success": bool, "path": string, "relative_path": string }
```

---

### `asset_catalogue.create_record`

```
description:
  Creates a new asset record in the catalogue without writing a source file. For types
  that have an associated blank file (e.g. scene, entity_template) a stub file is also
  written to source_path. Runs CreateRecordCommand and auto-saves.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  id           string    required  "Unique asset identifier."
  type         string    required  "Asset type ID, e.g. 'entity_template'."
  source_path  string    required  "Relative path from manifest dir to the source file."
  status       string    required  "Record status, e.g. 'active'."
  scope        string    required  "Scope, e.g. 'project'."
  stage        string    optional  "Stage filter, if applicable."
  tags         string[]  optional  "Tag list."
returns:   { "success": bool, "content_hash": uint, "error"?: string }
```

---

### `asset_catalogue.create_asset`

```
description:
  Creates a new asset record and writes the blank source file in one step. Equivalent
  to create_record but also calls the type-specific file creator. Runs CreateRecordCommand
  and auto-saves.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  assetType    string    required  "Asset type ID."
  id           string    required  "Unique asset identifier."
  source_path  string    required  "Relative source file path."
  tags         string[]  optional  "Tag list."
returns:   { "success": bool, "id": string, "absPath": string, "error"?: string }
```

---

### `asset_catalogue.create_scene`

```
description:
  Creates a new scene asset record and stub file, then loads DiaSceneEditor to open
  it. Shorthand for create_asset with type='scene' plus a navigation step.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  id           string    required  "Unique scene asset ID."
  source_path  string    required  "Relative path for the new .diascene file."
  tags         string[]  optional  "Tag list."
returns:   { "success": bool, "id": string, "absPath": string, "error"?: string }
```

---

### `asset_catalogue.get_record`

```
description:
  Returns the full record descriptor for a single asset by ID. Includes type, source
  path, status, scope, stage, tags, content_hash, and relationship metadata.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  id  string  required  "The unique asset ID."
returns:   { "success": bool, "record": record, "error"?: string }
```

---

### `asset_catalogue.update_record`

```
description:
  Updates an existing asset record's fields. Recomputes content_hash if source_path
  changes. Runs UpdateRecordCommand and auto-saves.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  id           string    required  "Existing asset ID to update."
  type         string    required  "Asset type ID."
  source_path  string    required  "Relative source file path."
  status       string    required  "Record status."
  scope        string    required  "Scope."
  stage        string    optional  "Stage filter."
  tags         string[]  optional  "Tag list."
returns:   { "success": bool, "content_hash": uint, "error"?: string }
```

---

### `asset_catalogue.delete_record`

```
description:
  Deletes an existing asset record from the catalogue. Runs DeleteRecordCommand and
  auto-saves. Does not delete the source file on disk.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  id  string  required  "The unique asset ID to delete."
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.bulk_create_records`

```
description:
  Creates multiple asset records in a single atomic command. Wraps N CreateRecordCommand
  calls in a CompoundCommand so the entire batch is undone together. Auto-saves when done.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  records  object[]  required  "Array of record descriptors; each has the same shape as create_record params."
returns:   { "success": bool, "created": int, "error"?: string }
```

---

### `asset_catalogue.query_by_type`

```
description:
  Returns all records whose type matches the given typeId. Use to enumerate all scenes,
  all entity templates, etc.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  typeId  string  required  "Asset type ID to filter by, e.g. 'entity_template'."
returns:   { "success": bool, "records": [record...], "error"?: string }
```

---

### `asset_catalogue.query_by_tag`

```
description:
  Returns all records that carry the given tag string.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  tag  string  required  "Tag string to filter by."
returns:   { "success": bool, "records": [record...], "error"?: string }
```

---

### `asset_catalogue.query_asset_ids`

```
description:
  Returns a filtered list of asset IDs. Supports optional prefix match, type filter,
  and result limit. Use for autocomplete or picking an asset ID without fetching full
  records.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  prefix  string  optional  "ID prefix to match against."
  typeId  string  optional  "Restrict to this asset type."
  limit   uint    optional  "Maximum results to return (default: all)."
returns:   { "success": true, "ids": [string...] }
```

---

### `asset_catalogue.get_asset_types`

```
description:
  Returns all registered asset type definitions: their typeId and display name. Use
  to populate type pickers or to validate a typeId before calling create_record.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": true, "types": [{ "typeId": string, "name": string }...] }
```

---

### `asset_catalogue.add_relationship`

```
description:
  Adds a directed relationship edge between two asset records. Runs AddRelationshipCommand
  and auto-saves. The rel parameter names the relationship kind (e.g. "uses", "spawns").

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  from  string  required  "Source asset ID."
  rel   string  required  "Relationship label."
  to    string  required  "Target asset ID."
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.remove_relationship`

```
description:
  Removes an existing directed relationship edge. Runs RemoveRelationshipCommand and
  auto-saves.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  from  string  required  "Source asset ID."
  rel   string  required  "Relationship label."
  to    string  required  "Target asset ID."
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.get_forward_refs`

```
description:
  Returns all outbound relationship edges from the given asset — every (rel, target)
  pair where this asset is the source. Use to find what an asset depends on.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  id  string  required  "Source asset ID."
returns:   { "success": bool, "refs": [{ "rel": string, "target": string }...], "error"?: string }
```

---

### `asset_catalogue.get_reverse_refs`

```
description:
  Returns all inbound relationship edges to the given asset — every (rel, source) pair
  where this asset is the target. Use to find what depends on an asset before deleting it.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  id  string  required  "Target asset ID."
returns:   { "success": bool, "refs": [{ "rel": string, "source": string }...], "error"?: string }
```

---

### `asset_catalogue.infer_relationships`

```
description:
  Scans all .diascene files in the project, detects entity-template references, and
  inserts the corresponding relationship edges into the catalogue. Idempotent — skips
  edges that already exist. Auto-saves when done.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "success": true, "scenes_scanned": int, "edges_added": int, "edges_skipped": int }
```

---

### `asset_catalogue.discover_files`

```
description:
  Walks the directory tree from root_path and returns every file with a suggested
  asset type and ID derived from its path. Use as the first step of a bulk-import
  workflow before calling bulk_create_records.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:
  root_path  string  required  "Absolute directory path to scan."
returns:   { "success": bool, "files": [{ "path": string, "suggested_type": string, "suggested_id": string, "file_size": int64, "last_modified": int64 }...] }
```

---

### `asset_catalogue.validate`

```
description:
  Validates the current catalogue: checks for missing source files, broken
  relationships, duplicate IDs, and unknown types. Returns a list of errors and
  warnings. An empty errors array means the catalogue is clean.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": true, "errors": [{ "assetId": string, "severity": string, "type": string, "message": string }...] }
```

---

### `asset_catalogue.load_rules`

```
description:
  Loads an auto-categorisation rules file from the given path into the rules engine.
  The rules are used by apply_rules to fill in type, status, and tag fields automatically.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  path  string  required  "Absolute path to the rules file."
returns:   { "success": bool, "rule_count"?: int, "error"?: string }
```

---

### `asset_catalogue.browse_rules`

```
description:
  Opens a file-picker dialog for the user to select a rules file, then loads it.
  Requires an active window; do not call from headless scripts.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "success": bool, "path"?: string, "rule_count"?: int, "error"?: string }
```

---

### `asset_catalogue.get_rules`

```
description:
  Returns the list of currently loaded auto-categorisation rules: name, match criteria,
  and action for each rule.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": true, "rules": [{ "name": string, "match": string, "matchValue": string, "action": string, "actionParam": string }...] }
```

---

### `asset_catalogue.dry_run_rules`

```
description:
  Evaluates all loaded rules against the current catalogue and returns the proposed
  changes without applying them. Use to preview the effect of apply_rules before
  committing. The truncated flag is true if the result set was capped.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": true, "changes": [change...], "conflict_count": int, "truncated": bool }
```

---

### `asset_catalogue.apply_rules`

```
description:
  Applies all loaded rules to the catalogue. Records matching a rule's criteria have
  their type, status, or tags updated automatically. Runs ApplyRulesCommand and
  auto-saves. Excluded IDs are skipped. Pass overwrite_manuals=true to overwrite
  fields that were set manually.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  excluded          string[]  optional  "Asset IDs to skip."
  overwrite_manuals bool      optional  "Overwrite manually-set fields (default: false)."
returns:   { "success": true, "applied_count": int }
```

---

### `asset_catalogue.register_type_editor`

```
description:
  Registers a mapping from an asset type to the editor plugin that handles it. Called
  by domain plugins (e.g. DiaSceneEditorPlugin) at load time so that open_in_editor
  knows which plugin to activate for a given asset type.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  assetType         string  required  "Asset type ID, e.g. 'scene'."
  editorPluginType  string  required  "Plugin type ID that handles this asset type."
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.open_in_editor`

```
description:
  Opens the given asset in its registered editor plugin — loads the plugin if not
  already loaded, then navigates to the asset. Requires register_type_editor to have
  been called for the asset's type.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  id  string  required  "The asset ID to open."
returns:   { "success": bool, "error"?: string }
```

---

### `asset_catalogue.open_in_file`

```
description:
  Opens the asset's source file in the default OS application (Explorer/Finder for
  the containing folder). Use to hand off to an external editor. Requires an active
  desktop session.

category:  asset_catalogue
owner:     DiaAssetCatalogueEditorPlugin
dispatch:  kMainThread
params:
  id  string  required  "The asset ID whose source file to open."
returns:   { "success": bool, "error"?: string }
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` method to `DiaAssetCatalogueEditorPlugin`; call from `OnPluginLoad()` after existing `RegisterHandler` calls; call `DeregisterActionsForOwner` in `OnPluginUnload()` | `GetManifest()` lists all 34 `asset_catalogue.*` actions | — | sonnet | |
| 2 | Register 14 kCallerThread actions (all read-only handlers listed above) | Python: `dia_editor.asset_catalogue.get_state()` returns expected shape | — | sonnet | |
| 3 | Register 19 kMainThread actions (all mutating handlers listed above) | Python: `dia_editor.asset_catalogue.create_record(...)` creates a record | — | sonnet | |
| 4 | Implement `asset_catalogue.get_available` (new, thin — check plugin is loaded and registry is non-null) | `get_available` returns `{ "loaded": true }` when plugin is active | — | haiku | |
| 5 | Python smoke test: `get_state`, `query_by_type`, `validate` | Smoke test passes; no errors in output | — | haiku | |

## Modules Touched

| Module | Change |
|--------|--------|
| `Dia/DiaAssetCatalogueEditor` | `DualRegisterActions()` + deregister in unload |

## Binding Decisions

No binding constraints beyond the DiaEditorAPI system spec (EAPI-001 through EAPI-009) apply.

## Open Design Questions

| # | Question |
|---|----------|
| ODQ-1 | `browse_open` and `browse_rules` open OS file dialogs — they are tagged `kMainThread` for safety but are still unsafe in headless/CI mode. **Resolved:** return `{ "success": false, "reason": "not_supported" }` when no window is present. One `IsWindowAvailable()` guard at the top of each handler. |
| ODQ-2 | `open_in_file` calls `ShellExecuteExW` — desktop-only. Same question as ODQ-1. **Resolved:** same treatment — `not_supported` guard. |
