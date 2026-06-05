# Feature Spec: entity-placement-crud

**System:** DiaSceneEditor
**App:** Dia
**Status:** Draft
**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Summary

Add, duplicate, delete, enable/disable, and rename entity instances in a `.diascene`. When adding a new entity, the user selects a blueprint (`.diaentitytemplate`); the new placement starts with empty `instance_data` (inheriting all defaults from the blueprint). Duplicate clones an existing entity with a position offset. Context menu provides all operations.

## Traceability

| Level | Spec |
|---|---|
| System | [diasceneeditor.md](../../systems/dia/diasceneeditor.md) |
| Depends on feature | [scene-hierarchy-panel.md](scene-hierarchy-panel.md) |

## Goals

- Authors can populate a scene with entity instances without hand-editing JSON
- Duplicate enables rapid placement of similar entities (e.g. coin pickups along a path)
- Delete removes entities cleanly with confirmation for safety
- The "+ Add Override..." button lets authors selectively override specific blueprint fields

## Acceptance Criteria

### Add Entity
- Toolbar "+ Entity" button opens a blueprint picker dropdown
- Dropdown lists all registered `.diaentitytemplate` files from the DiaAssetCatalogue (per SED-SCN-010)
- Only blueprints registered in the asset catalogue for the current game are shown (not from other games)
- Blueprint must pre-exist — no inline creation (per SED-SCN-011)
- Selecting a blueprint creates a new entity placement:
  - `id`: auto-generated as `{blueprint_name}_{N}` where N is next sequential number
  - `blueprint`: the selected blueprint name
  - `enabled`: true
  - `instance_data`: empty object `{}` (inherits all zero/empty defaults from blueprint per SED-SCN-015)
- New entity appears at end of Entities section, auto-selected
- Scene is marked dirty

### Duplicate (Clone)
- Right-click context menu → "Duplicate" (or Ctrl+D)
- Creates a deep copy of the selected entity with:
  - `id`: original ID + `_copy` (or `_copy2`, `_copy3` if that exists)
  - All `instance_data` copied
  - If `Transform2D.position` exists in overrides, offset by `[+50, +50]`
- Works for entities, cameras, lights, and layers:
  - Camera duplicate: `active` set to false (only one active allowed)
  - Layer duplicate: `sort_order` incremented by 1
- New item appears below the original, auto-selected
- Scene is marked dirty

### Delete
- Right-click context menu → "Delete" (or Del key)
- Shows confirmation: "Delete {id}? This cannot be undone."
- On confirm: removes the item from the scene data
- If deleted item was selected, selection clears and right panel shows placeholder
- Scene is marked dirty
- Deleting a layer does NOT delete entities/lights that reference it (they just lose that layer assignment)

### Enable/Disable
- Right-click context menu → "Enable / Disable"
- Toggles `enabled` (entities, lights, layers) or `active` (cameras)
- Disabled items render at 50% opacity in hierarchy
- For cameras: toggling active ON also toggles OFF the previously active camera (enforcement)
- Scene is marked dirty

### Rename
- Right-click context menu → "Rename" (or F2)
- Inline edit of the ID text in the hierarchy row
- Validates: no duplicate IDs within the same section; no empty string; alphanumeric + underscore only
- On validation failure: red border on input, tooltip with reason
- Scene is marked dirty on successful rename

### Add Override
- In the entity/camera/light Instance Overrides panel: "+ Add Override..." button
- Opens a dropdown showing all blueprint fields that are NOT currently overridden
- Fields grouped by component prefix
- Selecting a field adds it to `instance_data` with the blueprint's default value
- The new override row appears with purple left-border, ready to edit
- Scene is marked dirty

### Context menu
- Right-click any item in the hierarchy → context menu with:
  - Duplicate (Ctrl+D)
  - Rename (F2)
  - Change Blueprint... (entities only — see change-blueprint feature)
  - Enable / Disable
  - ---separator---
  - Delete (Del) — styled red/danger

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaSceneEditor/EntityPlacementController.h` | New — add/duplicate/delete/rename logic |
| `Dia/DiaSceneEditor/EntityPlacementController.cpp` | New |
| `Dia/DiaSceneEditor/ContextMenuController.h` | New — right-click menu dispatch |
| `Dia/DiaSceneEditor/ContextMenuController.cpp` | New |

## Binding Decisions Compliance

| Decision | Compliance |
|---|---|
| SED-SCN-007 | Duplicate offsets position by +50. Implemented. |

## Status

`Approved`
