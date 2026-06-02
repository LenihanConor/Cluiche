# Feature Spec: layer-authoring

**System:** DiaSceneEditor
**App:** Dia
**Status:** Draft
**Mockup:** @docs/specs/systems/dia/diasceneeditor.mockup.html

## Summary

Add, edit, reorder, and delete layers in a `.diascene`. Layers define draw order, parallax scrolling, and sort policy. The editor enforces that a "default" layer exists and displays which lights are assigned to each layer.

## Traceability

| Level | Spec |
|---|---|
| System | [diasceneeditor.md](../../systems/dia/diasceneeditor.md) |
| Depends on feature | [scene-hierarchy-panel.md](scene-hierarchy-panel.md) |

## Goals

- Authors can configure the layer stack for a scene without hand-editing JSON
- Sort order and parallax are immediately visible for spatial reasoning
- Layer deletion warns about affected lights

## Acceptance Criteria

### Add Layer
- Toolbar "+ Layer" button creates a new layer:
  - `id`: auto-generated as `layer_{N}`
  - `sort_order`: max existing sort_order + 10
  - `parallax`: [1.0, 1.0] (no parallax by default)
  - `sort_policy`: "insertion"
  - `enabled`: true
- New layer appears at end of Layers section, auto-selected
- Scene is marked dirty

### Edit Layer Properties
- All layer fields editable in the property panel:
  - **ID**: rename via context menu (same validation as entity rename)
  - **Sort Order**: integer input; scene re-sorts display on change
  - **Enabled**: checkbox toggle
  - **Parallax X/Y**: float inputs
  - **Sort Policy**: dropdown (v1: only "insertion" available; field shown but locked)

### Reorder
- Layer display order in hierarchy is by `sort_order` (ascending)
- Editing sort_order immediately repositions the layer in the list
- Future: drag-to-reorder updates sort_order values

### Delete Layer
- Right-click → Delete (or Del key)
- If lights reference this layer in `affects_layers`:
  - Confirmation shows: "Delete layer '{id}'? {N} light(s) reference this layer and will lose the assignment."
- On confirm: layer removed; affected lights have this layer removed from their `affects_layers` array
- Cannot delete the last remaining layer — show error: "Scene must have at least one layer"
- Scene is marked dirty

### Assigned Lights (informational)
- Layer property panel shows "Assigned Lights" section
- Lists all lights whose `affects_layers` includes this layer
- Read-only (editing happens on the light's property panel)

### Validation
- If no layer with `id: "default"` exists, show a validation warning in the status bar or scene properties
- This matches DiaScene2D's SceneLoader2D behaviour (auto-injects "default" if missing, but the editor should encourage explicit definition)

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaSceneEditor/LayerController.h` | New |
| `Dia/DiaSceneEditor/LayerController.cpp` | New |

## Status

`Approved`
