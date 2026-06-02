# Feature Spec: scene-validation

**System:** DiaSceneEditor
**App:** Dia
**Status:** Draft
**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Summary

Real-time validation of scene constraints while editing. Errors and warnings displayed in a validation panel (accessible via "Scene Properties" toolbar button) and as inline indicators in the hierarchy. Validation prevents common mistakes that would cause runtime load failures.

## Traceability

| Level | Spec |
|---|---|
| System | [diasceneeditor.md](../../systems/dia/diasceneeditor.md) |
| Depends on feature | [scene-hierarchy-panel.md](scene-hierarchy-panel.md) |
| Depends on feature | [camera-light-authoring.md](camera-light-authoring.md) |
| Depends on feature | [layer-authoring.md](layer-authoring.md) |

## Goals

- Authors catch configuration errors before running the game
- Validation rules match DiaScene2D's SceneLoader2D requirements
- Errors block save (hard); warnings allow save (soft)

## Acceptance Criteria

### Validation Rules

**Errors (block save):**
- Exactly 1 camera must have `active: true`
- No duplicate IDs within the same section (e.g. two entities named "player")
- Entity/camera/light IDs must be non-empty
- Layer IDs must be non-empty
- Max 32 layers (uint32 bitmask limit)
- Max 4 cameras
- Max 16 lights
- Max 256 entity placements

**Warnings (allow save):**
- No layer with `id: "default"` exists (SceneLoader2D auto-injects one, but explicit is preferred)
- Entity references a blueprint that has no corresponding `.diaentity` file
- Light's `affects_layers` references a layer ID that doesn't exist in the scene
- Disabled entities present (may be intentional but worth flagging)
- Camera with `active: false` and no other camera is active (impossible if enforcement works, but defensive)

### Scene Properties Panel
- Opened via toolbar "Scene Properties" button
- Shows:
  - World Bounds: Min X/Y, Max X/Y (editable)
  - Summary: counts of each item type
  - Validation results: list of errors (red) and warnings (amber) with descriptions
- Each validation result is clickable — selects the offending item in the hierarchy

### Inline Indicators
- Items with validation errors show a red dot/indicator in the hierarchy row
- Items with validation warnings show an amber dot

### Save Guard
- If validation errors exist, Save button is disabled
- Tooltip on disabled Save: "Fix validation errors before saving"
- Warnings do NOT block save

### Re-validation Timing
- Validation runs on every scene mutation (add, edit, delete, rename)
- Results cached until next mutation
- Performance: O(n) scan of scene items; acceptable for max 256+4+16+32 = 308 items

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaSceneEditor/SceneValidator.h` | New — validation rule engine |
| `Dia/DiaSceneEditor/SceneValidator.cpp` | New |
| `Dia/DiaSceneEditor/ScenePropertiesController.h` | New — world bounds + validation display |
| `Dia/DiaSceneEditor/ScenePropertiesController.cpp` | New |

## Status

`Approved`
