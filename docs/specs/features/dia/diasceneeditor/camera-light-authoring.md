# Feature Spec: camera-light-authoring

**System:** DiaSceneEditor
**App:** Dia
**Status:** Draft
**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Summary

Add, edit, and delete cameras and lights in a `.diascene`. Cameras use the blueprint + instance_data pattern (same as entities). Lights additionally have an `affects_layers` field that maps to checkboxes for layer assignment. The editor enforces exactly one active camera.

## Traceability

| Level | Spec |
|---|---|
| System | [diasceneeditor.md](../../systems/dia/diasceneeditor.md) |
| Depends on feature | [scene-hierarchy-panel.md](scene-hierarchy-panel.md) |
| Depends on feature | [layer-authoring.md](layer-authoring.md) |

## Goals

- Authors can configure cameras and lights with the same blueprint+override workflow as entities
- Active camera constraint is enforced at the editor level (prevents runtime load errors)
- Layer-light assignment is discoverable via checkboxes (not raw string arrays)

## Acceptance Criteria

### Add Camera
- Toolbar "+ Camera" button opens a blueprint picker (`.diacamera` files from the asset catalogue)
- New camera created with:
  - `id`: auto-generated as `camera_{N}`
  - `blueprint`: selected blueprint
  - `active`: false (unless this is the first camera, then true)
  - `instance_data`: empty
- Scene is marked dirty

### Active Camera Enforcement
- Exactly one camera must have `active: true` at all times
- Toggling a camera to active automatically deactivates the previously active camera
- Deleting the active camera: if other cameras exist, the first remaining camera becomes active; if no cameras remain, validation warning shown
- Visual: [ACTIVE] badge in hierarchy row

### Add Light
- Toolbar "+ Light" button opens a blueprint picker (`.dialight` files from the asset catalogue)
- New light created with:
  - `id`: auto-generated as `light_{N}`
  - `blueprint`: selected blueprint
  - `enabled`: true
  - `instance_data`: empty
  - `affects_layers`: all layers enabled by default
- Scene is marked dirty

### Affects Layers
- Light property panel shows "Affects Layers" section
- One checkbox per layer defined in the scene
- Checked layers are in the light's `affects_layers` array
- Unchecking all layers is allowed (light affects nothing)
- If a new layer is added to the scene, it does NOT auto-appear as checked on existing lights (explicit opt-in)
- If a layer is deleted, it is automatically removed from all lights' `affects_layers`

### Light Type Badge
- Hierarchy shows [DIR] for directional lights, [PNT] for point lights
- Type determined by blueprint name convention or a `type` field in the `.diaentitytemplatetemplate`

### Delete Camera/Light
- Same flow as entity delete (confirmation dialog)
- Additional check for cameras: cannot delete if it's the only camera and would leave zero cameras (validation error)

### Camera/Light Property Editing
- Same Instance Overrides / Blueprint Defaults tab pattern as entities
- Blueprint link opens DiaEntityTemplateEditor with that blueprint focused
- "+ Add Override..." available for adding specific field overrides

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaSceneEditor/CameraController.h` | New |
| `Dia/DiaSceneEditor/CameraController.cpp` | New |
| `Dia/DiaSceneEditor/LightController.h` | New |
| `Dia/DiaSceneEditor/LightController.cpp` | New |

## Status

`Approved`
