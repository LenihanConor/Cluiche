# Feature Spec: change-blueprint

**System:** DiaSceneEditor
**App:** Dia
**Status:** Draft
**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Summary

Allow reassigning an entity instance to a different blueprint. This is an inherently lossy operation: overrides for fields that exist in the new blueprint transfer; overrides for fields that don't exist in the new blueprint become orphaned. A confirmation dialog shows the transfer analysis before committing.

## Traceability

| Level | Spec |
|---|---|
| System | [diasceneeditor.md](../../systems/dia/diasceneeditor.md) |
| Depends on feature | [scene-hierarchy-panel.md](scene-hierarchy-panel.md) |
| Depends on feature | [entity-placement-crud.md](entity-placement-crud.md) |

## Goals

- Authors can switch an entity's blueprint without recreating the placement from scratch
- The blast radius of the change is clearly communicated before committing
- Orphaned overrides are not silently deleted — they remain in `instance_data` with a visual warning

## Acceptance Criteria

### Trigger
- "Change..." button in entity Identity section (next to blueprint link)
- Right-click context menu → "Change Blueprint..." (entities only)
- Both open the Change Blueprint dialog

### Dialog
- Title: "Change Blueprint"
- Shows the entity ID being modified
- Blueprint dropdown: lists all available `.diaentitytemplatetemplate` files; current blueprint is disabled/greyed out
- Transfer preview (updates live as dropdown selection changes):
  - **Green:** "N overrides will transfer: field1, field2, ..." — fields that exist in both old and new blueprint
  - **Amber:** "N overrides will be orphaned: field3, field4, ..." — fields that exist only in the old blueprint
- Warning banner: "This operation is lossy. Overrides for fields that don't exist in the new blueprint will be orphaned."
- Buttons: Cancel | Change Blueprint (confirm)

### Transfer logic
- For each key in the entity's `instance_data`:
  - If the key exists as a field in the new blueprint → **transfers** (value kept as-is)
  - If the key does NOT exist in the new blueprint → **orphaned** (value kept in `instance_data` but marked)
- No overrides are deleted — orphaned overrides stay in `instance_data`
- The entity's `blueprint` field is updated to the new blueprint name

### Post-change display
- Entity row in hierarchy updates to show new blueprint name
- Property panel re-renders with new blueprint context
- Orphaned overrides shown with amber/warning left-border (distinct from purple override indicator)
- Tooltip on orphaned override: "This field does not exist in the current blueprint"
- Scene is marked dirty
- Entity row in hierarchy updates blueprint name badge

### Orphan cleanup (optional user action)
- Each orphaned override row has a small "x" remove button
- Clicking removes that key from `instance_data`
- Or user can leave orphans in place (they're ignored at runtime but preserved for potential future blueprint switch-back)

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaSceneEditor/ChangeBlueprintController.h` | New — dialog logic + transfer analysis |
| `Dia/DiaSceneEditor/ChangeBlueprintController.cpp` | New |

## Binding Decisions Compliance

| Decision | Compliance |
|---|---|
| SED-SCN-004 | Change Blueprint is lossy — transferring overrides shown, orphaned warned. Implemented exactly. |

## Status

`Approved`
