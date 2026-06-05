# Feature Spec: blueprint-panel

**System:** DiaEntityTemplateEditor
**App:** Dia
**Status:** Done

## Summary

Implement the DiaEntityTemplateEditorPlugin scaffold, the blueprint list panel (left, grouped by type), and the blueprint property panel (right, component accordion with field editing). This is the foundational feature of DiaEntityTemplateEditor — it establishes file I/O for `.diaentitytemplate`, `.diacamera`, `.dialight` and the editing workflow for component fields.

## Traceability

| Level | Spec |
|---|---|
| System | [DiaEntityTemplateEditor.md](../../systems/dia/DiaEntityTemplateEditor.md) |
| Depends on system | [diareflect.md](../../systems/dia/diareflect.md) |
| Depends on system | [diaassetcatalogue.md](../../systems/dia/diaassetcatalogue.md) |

## Goals

- Authors can define and modify entity blueprints without hand-editing JSON
- Component field metadata (types, names) comes from DiaReflect's ComponentTypeRegistry
- Blueprint changes are independent of any specific scene instance — they affect all instances that reference the blueprint

## Acceptance Criteria

### Blueprint file I/O
- `EntityTemplateEditorController` loads a `.diaentitytemplate` file when a blueprint is selected in the Blueprints section
- File is parsed into an in-memory representation: list of components, each with type name and field key-value pairs
- Save writes the modified data back to the same `.diaentitytemplate` file path
- Blueprint dirty state tracked independently from scene dirty state (different files)
- Title bar or status bar indicates which blueprint file is being edited when in blueprint view

### Blueprint property panel
- Right panel shows "Blueprint Identity" section:
  - Name (read-only, derived from file)
  - File path (read-only)
  - File type badge: `.diaentitytemplate` / `.diacamera` / `.dialight`
- "Usage" section (cross-scene impact visibility per SED-SCN-012):
  - Lists all scenes that reference this blueprint (derived from asset catalogue relationships)
  - For each scene: shows count of instances using this blueprint
  - Example: "platformer_level_01.diascene — 3 instances (player_spawn, enemy_patrol_01, enemy_patrol_02)"
  - This makes edit blast radius immediately visible before changing any fields
- "Component Fields" section: one collapsible component group per component in the blueprint
  - Each component group shows component type name as header
  - Each field row: field name, type badge (from DiaReflect), editable value input
  - All fields are editable (these are the defaults that instances inherit)
  - All defaults are zero/empty for new fields (per SED-SCN-015)
- "+ Add Component..." button at bottom

### Add Component (cascades to instances — SED-SCN-013)
- "+ Add Component..." opens a dropdown of all registered component types from ComponentTypeRegistry
- Types already present in the blueprint are greyed out / disabled
- Before confirming, shows affected instance count: "This will add {ComponentName} to {N} instances across {M} scenes"
- Selecting a type adds a new component group with all fields set to zero/empty defaults (per SED-SCN-015)
- The new component's default fields automatically become available on all referencing instances (they inherit the new defaults)
- Existing instance overrides are unaffected
- Blueprint is marked dirty

### Remove Component
- Each component group header has a small "x" / remove button
- Clicking shows confirmation: "Remove {ComponentName}? Instances overriding these fields will retain orphaned overrides."
- On confirm: component and all its fields removed from the blueprint
- Blueprint is marked dirty

### Field editing
- Editing a field value marks the blueprint dirty
- Type-aware inputs same as scene property panel:
  - `bool` → checkbox
  - `int` → numeric input
  - `float` → numeric input (3 decimal places)
  - `vec2` → two inputs (x, y)
  - `string` → text input
  - `enum` → dropdown (if DiaReflect provides enum values)

### .diaentitytemplate file format

```json
{
  "entity_template": {
    "id": "player_entity",
    "components": [
      {
        "type": "Transform2D",
        "fields": { "position": [0, 0], "rotation": 0, "scale": [1, 1] }
      },
      {
        "type": "Health",
        "fields": { "max_hp": 50, "current_hp": 50, "invulnerable": false }
      }
    ]
  }
}
```

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntityTemplateEditor/DiaEntityTemplateEditor.vcxproj` | New — static library project |
| `Dia/DiaEntityTemplateEditor/DiaEntityTemplateEditor.vcxproj.filters` | New |
| `Dia/DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.h` | New — `IEditorPlugin` subclass |
| `Dia/DiaEntityTemplateEditor/DiaEntityTemplateEditorPlugin.cpp` | New |
| `Dia/DiaEntityTemplateEditor/BlueprintListController.h` | New |
| `Dia/DiaEntityTemplateEditor/BlueprintListController.cpp` | New |
| `Dia/DiaEntityTemplateEditor/BlueprintPropertyController.h` | New |
| `Dia/DiaEntityTemplateEditor/BlueprintPropertyController.cpp` | New |
| `Dia/DiaEntityTemplateEditor/BlueprintFileHandler.h` | New — `.diaentitytemplate` / `.diacamera` / `.dialight` load/save |
| `Dia/DiaEntityTemplateEditor/BlueprintFileHandler.cpp` | New |
| `Cluiche/Cluiche.sln` | Add `DiaEntityTemplateEditor.vcxproj` |

## Binding Decisions Compliance

| Decision | Compliance |
|---|---|
| SED-BP-002 | Separate extensions: `.diaentitytemplate`, `.diacamera`, `.dialight`. Implemented. |
| SED-BP-003 | Discovery via DiaAssetCatalogue. List populated from asset registry. |
| SED-BP-004 | Cross-scene usage shown. Usage section displays scenes + instance counts. |
| SED-BP-007 | All field defaults zero/empty. New components get zero-value fields. |

## Open Design Questions

None — all resolved.

## Resolved Questions

| # | Question | Resolution |
|---|----------|------------|
| 1 | Should the blueprint editor allow creating new blueprints from scratch? | Deferred to v2 (SED-SCN-011). V1 requires blueprints to pre-exist in the asset catalogue. Future: "New Blueprint..." flow with name + pick initial components. |
| 2 | When a component is removed from a blueprint, should existing instances be warned? | Yes — confirmation dialog shows count of affected instances across all scenes. Orphaned overrides remain on instances with a visual warning. |
| 3 | When a component is added to a blueprint, does it cascade? | Yes (SED-SCN-013) — new fields appear on all instances with zero/empty defaults. Show affected count before confirming. |

## Status

`Approved`
