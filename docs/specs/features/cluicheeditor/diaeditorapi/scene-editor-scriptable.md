# Feature Spec: scene-editor-scriptable

**Parent:** @docs/specs/systems/cluicheeditor/diaeditorapi.md
**Status:** Done

## Summary

Dual-registers all 37 existing `scene_editor.*` WebUIBridge handlers in `DiaSceneEditorPlugin` with the `EditorActionRegistry`, and adds 3 new high-level scripting actions (`get_entities`, `place_entity`, `remove_entity`) that don't currently exist. After migration the full scene authoring surface is callable from Python via `dia_editor.scene_editor.*`.

## Problem

The scene editor has 37 WebUIBridge handlers covering the complete scene lifecycle: project state, stage/scene loading, hierarchy queries, entity/camera/light CRUD, layer management, override editing, template changes, validation, and cross-plugin shortcuts. None are reachable from Python or the AI layer. The new `get_entities`, `place_entity`, and `remove_entity` actions provide a cleaner scripting surface for the most common automation patterns (enumerate, place, remove) without requiring callers to understand the full hierarchy/override model.

## Goals

1. Expose all 37 existing handlers through DiaEditorAPI with correct thread dispatch.
2. Add 3 new scripting-oriented actions: `get_entities`, `place_entity`, `remove_entity`.
3. No existing WebUIBridge behaviour changes — dual-registration only for the 37 existing handlers.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | All 35 migrated handlers plus 5 new actions are registered in `EditorActionRegistry` and appear in `GetManifest()` output (40 total; `new_scene_shortcut` and the dialog `create_asset` handler are excluded) |
| AC2 | All 40 are callable from Python via `dia_editor.scene_editor.*` |
| AC3 | `scene_editor.get_entities` returns a flat list of entity entries from the loaded scene |
| AC4 | `scene_editor.place_entity` adds an entity to the scene with an optional position and returns the new entity ID |
| AC5 | `scene_editor.remove_entity` removes an entity from the scene by ID |
| AC6 | Read-only handlers use `kCallerThread`; mutating handlers use `kMainThread` |
| AC7 | `DeregisterActionsForOwner(StringCRC("DiaSceneEditorPlugin"))` removes all actions on plugin unload |
| AC8 | Python smoke test: `dia_editor.scene_editor.get_project_state()` and `dia_editor.scene_editor.get_entities()` return expected shapes |

## Action Manifest

### Thread assignments

**kCallerThread (read-only, 12 existing actions):**
`get_project_state`, `get_stage_list`, `get_hierarchy`, `get_hierarchy_filtered`, `get_properties`, `get_entity_template_defaults`, `get_available_entity_templates`, `get_dirty_state`, `analyse_change_entity_template`, `validate`, `get_scene_properties`, `get_entities`

**kMainThread (mutating, 25 existing + 5 new actions):**
`load_stage_scene`, `load_scene`, `save_scene`, `set_selection`, `mark_dirty`, `add_item`, `duplicate_item`, `delete_item`, `rename_item`, `set_enabled`, `change_entity_template`, `add_layer`, `delete_layer`, `reorder_layer`, `update_layer`, `set_camera_active`, `set_light_affects_layers`, `set_light_type`, `add_override`, `remove_override`, `update_override`, `set_world_bounds`, `open_entity_template`, `associate_scene_to_stage`, `place_entity`, `remove_entity`, `create_scene`, `create_asset`

Note: `new_scene_shortcut` and the original `create_asset` dialog handler are UI-only and not registered in DiaEditorAPI.

---

### `scene_editor.get_project_state`

```
description:
  Returns whether a valid project is loaded and the path to the active .diagame file.
  Call this before any other scene_editor action to confirm the plugin has a project
  context.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "isValid": bool, "diagamePath": string }
```

---

### `scene_editor.get_stage_list`

```
description:
  Returns the list of all stages declared in the active .diagame manifest. Each entry
  includes the stage name and its associated scene path (if assigned). Use this to
  enumerate valid stagePath values for load_stage_scene and associate_scene_to_stage.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": bool, "stages": [{ "name": string, "scenePath": string }...] }
```

---

### `scene_editor.load_stage_scene`

```
description:
  Loads the scene file associated with the given stage path. Parses the .diastage
  manifest to find the linked scene, then loads that scene into the editor. Returns
  the full hierarchy and dirty flag on success.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  stagePath  string  required  "Path to the .diastage file."
returns:   { "success": bool, "stage": object, "scene": object, "hierarchy": object, "dirty": bool, "error"?: string }
```

---

### `scene_editor.load_scene`

```
description:
  Loads a scene file directly from a path. Replaces any currently loaded scene.
  Returns the full scene data, hierarchy, and dirty flag on success.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  path  string  required  "Absolute path to the .diascene file."
returns:   { "success": bool, "scene": object, "hierarchy": object, "dirty": bool, "error"?: string }
```

---

### `scene_editor.save_scene`

```
description:
  Saves the current scene to disk. If path is provided it saves to that location;
  otherwise saves to the currently loaded path. Returns the updated dirty flag.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  path   string  optional  "Save-As path. Omit to save to current path."
  scene  object  optional  "Scene data to save. Omit to save the in-memory scene."
returns:   { "success": bool, "dirty": bool, "error"?: string }
```

---

### `scene_editor.get_hierarchy`

```
description:
  Loads and returns the entity hierarchy for an arbitrary scene file without making
  it the active scene. Use to inspect a scene's structure without affecting editor state.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  path  string  required  "Absolute path to the .diascene file."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.get_hierarchy_filtered`

```
description:
  Returns the hierarchy of the currently loaded scene, filtered by a search term.
  An empty filter returns the full hierarchy. Entries that do not match the filter
  are omitted; parent nodes are kept if any child matches.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  filter  string  optional  "Search term to filter hierarchy entries. Omit for full hierarchy."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.get_entities`

```
description:
  Returns a flat list of all entity entries in the currently loaded scene. Each entry
  includes the entity ID, template ID, enabled flag, layer, and any instance_data
  overrides. Simpler than get_hierarchy_filtered for scripting use cases that need
  to enumerate or locate entities without tree navigation.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  type  string  optional  "Item type to filter: 'entity', 'camera', or 'light'. Omit for all."
returns:   { "success": bool, "entities": [{ "id": string, "templateId": string, "enabled": bool, "layerId": string, "overrides": object }...] }
```

---

### `scene_editor.set_selection`

```
description:
  Selects an item in the currently loaded scene and notifies the AppEditorController.
  Pass an empty object or omit type/id to clear the selection. The selection state is
  reflected in app_editor.get_active_context.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  type  string  optional  "Item type: 'entity', 'camera', or 'light'. Omit to clear."
  id    string  optional  "Item ID. Omit to clear."
returns:   { "success": bool, "selection": object }
```

---

### `scene_editor.get_properties`

```
description:
  Returns the full property set for a selected item: all blueprint-defined component
  fields merged with any instance_data overrides. Use to inspect an entity's current
  state before calling add_override or update_override.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  selectionType  string  required  "Item type: 'entity', 'camera', or 'light'."
  selectionId    string  required  "Item ID."
returns:   { "success": bool, "properties": object, "selection": object, "error"?: string }
```

---

### `scene_editor.get_entity_template_defaults`

```
description:
  Returns the default field values for a given entity template type as defined by its
  schema. Use before calling add_override to know what values are available to override.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  entityTemplateId  string  required  "Entity template asset ID."
  itemType          string  required  "Item type: 'entity', 'camera', or 'light'."
returns:   { "success": bool, "data": object, "error"?: string }
```

---

### `scene_editor.get_available_entity_templates`

```
description:
  Returns the list of entity templates registered in the asset catalogue for the given
  item type. Use to enumerate valid templateId values for add_item and place_entity.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  itemType  string  optional  "Item type: 'entity', 'camera', or 'light'. Defaults to 'entity'."
returns:   { "success": bool, "assetType": string, "entityTemplates": [object...] }
```

---

### `scene_editor.get_dirty_state`

```
description:
  Returns the current dirty flag for the loaded scene. True means there are unsaved
  changes. Poll this to decide whether to call save_scene before closing.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": bool, "dirty": bool }
```

---

### `scene_editor.mark_dirty`

```
description:
  Marks the scene as dirty and triggers an autosave. Use when you have directly
  mutated scene data outside of the normal handler path and need to ensure the
  dirty flag and autosave are triggered.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:    none
returns:   { "success": bool, "dirty": bool }
```

---

### `scene_editor.add_item`

```
description:
  Adds a new entity, camera, or light to the currently loaded scene using the given
  template ID. Assigns an auto-generated ID unless id is provided. Auto-saves after
  inserting. Returns the updated hierarchy.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType          string  required  "Item type: 'entity', 'camera', or 'light'."
  entityTemplateId  string  required  "Template asset ID."
  id                string  optional  "Explicit ID for the new item. Auto-generated if omitted."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.place_entity`

```
description:
  High-level scripting action: adds an entity to the scene with an explicit position
  and optional overrides in one call. Combines template resolution, add_item, and
  update_override so scripts can place an entity in a single step. Auto-saves after
  inserting.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  templateId  string  required  "Entity template asset ID."
  position    object  optional  "Position override, e.g. { 'x': 100, 'y': 200 }. Uses template default if omitted."
  overrides   object  optional  "Additional instance_data overrides as { fieldPath: value } pairs."
  layerId     string  optional  "Layer to place into. Uses scene default layer if omitted."
  id          string  optional  "Explicit entity ID. Auto-generated if omitted."
returns:   { "success": bool, "entityId": string, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.remove_entity`

```
description:
  Removes an entity from the currently loaded scene by ID. Typed convenience wrapper
  around delete_item with itemType='entity'. Auto-saves after removal.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  entityId  string  required  "ID of the entity to remove."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.duplicate_item`

```
description:
  Creates a deep copy of an existing entity, camera, or light. The copy gets an ID
  with a '_copy' suffix and a position offset of +50 on both axes. Auto-saves after
  inserting.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType  string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId    string  required  "ID of the item to duplicate."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.delete_item`

```
description:
  Removes an entity, camera, or light from the currently loaded scene. Auto-saves
  after removal. For entity-only scripts, prefer remove_entity which is more
  discoverable.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType  string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId    string  required  "ID of the item to remove."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.rename_item`

```
description:
  Renames an entity, camera, or light. Validates that the new ID is non-empty,
  alphanumeric/underscore only, and unique within the scene. Auto-saves after renaming.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType  string  required  "Item type: 'entity', 'camera', or 'light'."
  oldId     string  required  "Current item ID."
  newId     string  required  "New item ID. Must be non-empty, unique, alphanumeric + underscore."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.set_enabled`

```
description:
  Toggles the enabled flag of an entity, camera, or light. Disabled items are excluded
  from the active scene at runtime. Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType  string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId    string  required  "Item ID."
  enabled   bool    required  "True to enable, false to disable."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.analyse_change_entity_template`

```
description:
  Previews the effect of changing an item's template without applying it. Returns
  an analysis of which fields would be transferred from the old template, which would
  become orphaned overrides, and which new defaults would apply.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:
  itemType             string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId               string  required  "Item ID."
  newEntityTemplateId  string  required  "New template asset ID."
returns:   { "success": bool, "analysis": object, "error"?: string }
```

---

### `scene_editor.change_entity_template`

```
description:
  Applies a template change to an existing item. Transfers compatible override fields,
  drops orphaned ones, and updates asset catalogue relationships. Auto-saves after
  update. Call analyse_change_entity_template first to preview the impact.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType             string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId               string  required  "Item ID."
  newEntityTemplateId  string  required  "New template asset ID."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.add_layer`

```
description:
  Adds a new render layer to the scene with the given ID. The layer's sort_order is
  assigned automatically as one above the current highest. Auto-saves after insertion.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  layerId  string  required  "Unique layer ID."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.delete_layer`

```
description:
  Removes a render layer from the scene. At least one layer must remain; returns an
  error if this would leave the scene with no layers. Auto-saves after removal.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  layerId  string  required  "Layer ID to remove."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.reorder_layer`

```
description:
  Moves a layer to a new position in the layer stack by setting its sort_order.
  Auto-saves after reordering.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  layerId   string  required  "Layer ID to reorder."
  newIndex  int     required  "Target position index (0 = bottom)."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.update_layer`

```
description:
  Updates one or more properties of a render layer: parallax factor, sort policy,
  enabled flag, or other layer-level settings. Pass only the fields to update in
  the fields object. Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  layerId  string  required  "Layer ID to update."
  fields   object  required  "Map of field names to new values."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.set_camera_active`

```
description:
  Marks the given camera as the active camera for the scene. Deactivates all other
  cameras (only one can be active at a time). Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  cameraId  string  required  "Camera item ID to activate."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.set_light_affects_layers`

```
description:
  Assigns the list of layer IDs that this light illuminates. Replaces any existing
  layer assignments. Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  lightId   string    required  "Light item ID."
  layerIds  string[]  required  "Layer IDs this light should affect."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.set_light_type`

```
description:
  Sets the light type to directional ('DIR') or point ('PNT'). Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  lightId  string  required  "Light item ID."
  type     string  required  "Light type: 'DIR' or 'PNT'."
returns:   { "success": bool, "hierarchy": object, "error"?: string }
```

---

### `scene_editor.add_override`

```
description:
  Promotes a blueprint field to an instance_data override on the given item. The field
  is set to defaultValue; call update_override to change it. Auto-saves after insertion.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType      string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId        string  required  "Item ID."
  overrideKey   string  required  "Field path to override, e.g. 'Transform.position.x'."
  defaultValue  any     required  "Initial value for the override."
returns:   { "success": bool, "error"?: string }
```

---

### `scene_editor.remove_override`

```
description:
  Removes an instance_data override from an item, reverting that field to its blueprint
  default. Auto-saves after removal.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType     string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId       string  required  "Item ID."
  overrideKey  string  required  "Field path of the override to remove."
returns:   { "success": bool, "error"?: string }
```

---

### `scene_editor.update_override`

```
description:
  Updates the value of an existing instance_data override. The override must already
  exist (call add_override first if needed). Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  itemType     string  required  "Item type: 'entity', 'camera', or 'light'."
  itemId       string  required  "Item ID."
  overrideKey  string  required  "Field path of the override to update."
  value        any     required  "New value."
returns:   { "success": bool, "error"?: string }
```

---

### `scene_editor.validate`

```
description:
  Runs validation on the currently loaded scene: checks for broken template references,
  duplicate IDs, invalid layer assignments, and other structural issues. Returns an
  error and warning list. An empty array means the scene is clean.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": bool, "validation": [{ "type": string, "severity": string, "message": string }...] }
```

---

### `scene_editor.get_scene_properties`

```
description:
  Returns scene-level metadata: world bounds, entity/camera/light counts, and a
  validation summary. Use to get a quick health overview without loading the full
  hierarchy.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kCallerThread
params:    none
returns:   { "success": bool, "properties": { "world_bounds": object, "counts": object, "validation": object } }
```

---

### `scene_editor.set_world_bounds`

```
description:
  Updates the scene's world boundary rectangle. Used by the camera and physics systems
  to clamp movement. Auto-saves after update.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  world_bounds  object  required  "World bounds rectangle, e.g. { 'x': 0, 'y': 0, 'width': 1920, 'height': 1080 }."
returns:   { "success": bool, "error"?: string }
```

---

### `scene_editor.create_scene`

```
description:
  Creates a new scene asset record and stub .diascene file without opening any dialog,
  then immediately loads the new scene into the scene editor. Use this from scripts
  and automation in place of the UI's new-scene popup. Requires the Asset Catalogue
  plugin to be loaded. Returns the new asset ID and absolute file path on success.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  id           string    required  "Unique scene asset ID."
  source_path  string    required  "Relative path (from manifest dir) for the new .diascene file."
  tags         string[]  optional  "Tag list."
returns:   { "success": bool, "id": string, "absPath": string, "error"?: string }
```

---

### `scene_editor.create_asset`

```
description:
  Creates a new asset record and stub source file of any type without opening any
  dialog. Use this from scripts and automation in place of the UI's create-asset popup.
  Requires the Asset Catalogue plugin to be loaded. Does not auto-load the asset into
  an editor (use asset_catalogue.open_in_editor for that).

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  assetType    string    required  "Asset type ID, e.g. 'entity_template'."
  id           string    required  "Unique asset ID."
  source_path  string    required  "Relative path (from manifest dir) for the new source file."
  tags         string[]  optional  "Tag list."
returns:   { "success": bool, "id": string, "absPath": string, "error"?: string }
```

---

### `scene_editor.open_entity_template`

```
description:
  Opens the entity template with the given ID in the Entity Template Editor. Loads
  the plugin if not already active. Resolves a bare name to the full catalogue ID
  before opening.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  entityTemplateId  string  required  "Entity template asset ID or bare name."
  itemType          string  optional  "Item type hint: 'entity', 'camera', or 'light'. Defaults to 'entity'."
returns:   { "success": bool, "error"?: string }
```

---

### `scene_editor.associate_scene_to_stage`

```
description:
  Writes a scene-to-stage association into the .diastage manifest file. After calling
  this, load_stage_scene will find the scene when given the stage path.

category:  scene_editor
owner:     DiaSceneEditorPlugin
dispatch:  kMainThread
params:
  stagePath  string  required  "Path to the .diastage file."
  scenePath  string  required  "Path to the .diascene file to associate."
returns:   { "success": bool, "error"?: string }
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `DualRegisterActions()` to `DiaSceneEditorPlugin`; call from `OnPluginLoad()` after existing `RegisterHandler` calls; call `DeregisterActionsForOwner` in `OnPluginUnload()` | `GetManifest()` lists all 40 `scene_editor.*` actions (35 migrated + 5 new) | — | sonnet | |
| 2 | Register 12 kCallerThread actions (all read-only handlers listed above, excluding new ones) | Python: `dia_editor.scene_editor.get_project_state()` returns expected shape | — | sonnet | |
| 3 | Register 25 kMainThread actions (all mutating handlers listed above, excluding new ones) | Python: `dia_editor.scene_editor.load_scene(path)` loads scene successfully | — | sonnet | |
| 4 | Implement `scene_editor.get_entities` — new action; iterate `mLoadedSceneRoot`, build flat entity list | `get_entities()` returns entries with id/templateId/enabled/layerId/overrides fields | — | sonnet | |
| 5 | Implement `scene_editor.place_entity` — new action; combines add_item + position override + additional overrides in one call | `place_entity(templateId, position)` adds entity at correct position; entity appears in `get_entities()` output | — | sonnet | |
| 6 | Implement `scene_editor.remove_entity` — new action; thin wrapper calling `HandleDeleteItem` with itemType='entity' | `remove_entity(entityId)` removes entity; entity absent from `get_entities()` output | — | haiku | |
| 7 | Python smoke test: `get_project_state`, `get_entities`, `place_entity`, `remove_entity` | Smoke test passes; no errors in output | — | haiku | |

## Modules Touched

| Module | Change |
|--------|--------|
| `Dia/DiaSceneEditor` | `DualRegisterActions()` + deregister in unload; 3 new action implementations |

## Binding Decisions

No binding constraints beyond the DiaEditorAPI system spec (EAPI-001 through EAPI-009) apply.

## Open Design Questions

| # | Question |
|---|----------|
| ODQ-1 | `place_entity` is a new composition action. Should it call `HandleAddItem` + `HandleUpdateOverride` internally, or directly manipulate `mLoadedSceneRoot`? **Resolved:** composition. Double-autosave cost is negligible; direct manipulation would silently miss any future logic added to `HandleAddItem` (relationship updates, validation hooks, notifications). |
| ODQ-2 | The UI's `new_scene_shortcut` and `create_asset` handlers open dialogs and forward to the Asset Catalogue. **Resolved:** replaced with `scene_editor.create_scene` and `scene_editor.create_asset` — scriptable actions that skip the dialog entirely. `create_scene` also auto-loads the result into the scene editor. The original dialog handlers remain for UI use only and are not registered in DiaEditorAPI. |
| ODQ-3 | 40 actions is the largest single-plugin registration so far. Should `DualRegisterActions()` be split into sub-methods? **Resolved:** yes — `RegisterFileActions`, `RegisterHierarchyCRUDActions`, `RegisterLayerActions`, `RegisterOverrideActions`, `RegisterTemplateActions`, `RegisterCrossPluginActions`. Each ~5–8 registrations. `DualRegisterActions()` calls each in sequence. |
