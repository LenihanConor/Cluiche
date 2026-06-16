# Feature Spec: scene-validation

**Parent:** [diasceneeditor.md](diasceneeditor.md)
**Status:** Approved

## Summary

Real-time validation of scene constraints while editing. Warnings displayed as inline icons in the hierarchy rows, with a "Needs Attention" filter to scan only problem items. Validation runs on every mutation (autosave means there is no save-gate — warnings are informational).

## Goals

- Authors catch configuration errors before running the game
- Validation rules match DiaScene2D's SceneLoader2D requirements
- Scan through problems quickly via filter without losing hierarchy context

## Binding Decisions

- **SED-SCN-009** — No Save button; autosave on every mutation. Validation is advisory, not a gate.

## Acceptance Criteria

### Validation Rules

**Errors (red icon):**
- Exactly 1 camera must have `active: true`
- No duplicate IDs within the same section
- Entity/camera/light/layer IDs must be non-empty
- Max 32 layers (uint32 bitmask limit)
- Max 4 cameras
- Max 16 lights
- Max 256 entity placements

**Warnings (amber icon):**
- No layer with `id: "default"` exists
- Entity/camera/light references a template that cannot be resolved to a file
- Light's `affects_layers` references a layer ID that doesn't exist in the scene
- Disabled items present (may be intentional)

### Inline Hierarchy Icons

- Items with validation errors show a red warning icon in the hierarchy row
- Items with validation warnings show an amber warning icon
- Icons appear to the right of the item name, before any action buttons

### Needs Attention Filter

- Toggle button in the hierarchy toolbar: filter icon with badge count (e.g. "3")
- When active, hierarchy shows ONLY items that have at least one error or warning
- Sections with no issues collapse/hide entirely
- Clicking an item in filtered view selects it and shows its properties as normal
- Badge count updates on every mutation

### Re-validation Timing

- Validation runs on every scene mutation (add, edit, delete, rename, change template)
- Results cached until next mutation
- Performance: O(n) scan of scene items; acceptable for max 308 items

## Open Design Questions

- Should "Needs Attention" be a separate tab or a toggle filter on the existing hierarchy? (Decided: toggle filter — less UI disruption, items stay in context)

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | Backend: SceneValidator returns per-item diagnostics (item type + id + severity + message) | Todo |
| 2 | Backend: Wire validation into every mutation response (include `diagnostics[]` alongside `hierarchy`) | Todo |
| 3 | UI: Render inline warning/error icons in hierarchy rows | Partial — template-unknown warnings (amber ⚠ icon) ship via `entityTemplate_known` enrichment; full rule-driven icons (errors for duplicate IDs, limits, etc.) require Tasks 1+2 |
| 4 | UI: "Needs Attention" toggle button with badge count + filtered view | Todo |
