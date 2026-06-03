# Feature Spec: component-picker-and-defaults

**Parent:** @docs/specs/systems/dia/diablueprinteditor.md
**Status:** Done
**Plan:** @docs/specs/features/dia/diablueprinteditor/component-picker-and-defaults.plan.md
**Mockup:** @docs/mockups/blueprint-editor-v2.html

## Summary

Replace the `<select>` Add Component dropdown with a searchable picker panel, and surface component field default values (from C++ constructors via the schema) alongside blueprint-level overrides that persist to the `.diaentity` file.

## Problem

The current `<select>` dropdown doesn't scale beyond ~5 components, has no descriptions, and gives no feedback about what each component does. Field editing also gives no indication of what value a field starts at when a component is added, and there is no way to set a blueprint-level default that differs from the code default.

## Goals

- Component picker that scales to 100+ components with search, keyboard nav, and per-component descriptions
- Code defaults (C++ constructor values) visible in field rows as placeholders and hint text
- Blueprint defaults — per-field overrides stored in the `.diaentity` file — editable in the property panel
- Clear visual distinction between "using code default", "has blueprint override", and "has no default known"
- `--dump-schema` exports code defaults so the editor has them without linking the game binary

## Acceptance Criteria

### AC-1: Component picker replaces `<select>`

- The "Add Component" `<select>` + "Add" button is replaced by a single "+ Add Component" button
- Clicking it opens a picker panel (inline, above the button) with:
  - A text search box, focused on open
  - A scrollable list of available components (those not already on the blueprint)
  - Each row shows: display name (bold), type ID (monospace, smaller), description (greyed, truncated)
  - A count line: "N of M components available"
- Search filters by display name, type ID, and description simultaneously
- Keyboard: ↑↓ to move focus, Enter to add focused item, Esc to close
- Double-click an item to add it immediately
- "Add" button in the footer is disabled until an item is focused
- Closing without selecting does nothing
- The picker closes and the component appears immediately after adding

### AC-2: Schema exports code defaults

- `--dump-schema` (on the game binary) serialises a default-constructed instance of each component via its `SaveToJsonFn`
- The resulting JSON gains a `"default_values"` object on each component entry alongside `"fields"`:
  ```json
  {
    "type_id": "cluichetest.transform",
    "debug_name": "TransformComponent",
    "fields": [
      { "name": "x", "kind": "primitive" },
      { "name": "y", "kind": "primitive" },
      { "name": "scale", "kind": "primitive" }
    ],
    "default_values": { "x": 0.0, "y": 0.0, "scale": 1.0 }
  }
  ```
- If `SaveToJsonFn` is null for a component, `"default_values"` is still emitted with all fields set to `0` / `""` / `false` based on kind — never omitted
- `SchemaReader` (in `DiaBlueprintEditor`) exposes `GetDefaultValues(typeId)` returning the parsed `default_values` object

### AC-3: Field rows show code defaults

- For each field in the property panel:
  - If the field has no value in the `.diaentity` file AND a code default is known: the input is empty, the code default is shown as `placeholder` text (grey), and a hint line reads `"code default: <value> · leave blank to keep"`
  - If the field has no value AND no code default is known: input is empty, no placeholder, hint reads `"no default"`
- The value shown in the input is always the stored blueprint value — never the code default

### AC-4: Blueprint defaults (overrides)

- Editing a field and committing a value stores it in the `"fields"` object of the `.diaentity` component entry and saves the file
- An input with a stored blueprint value is styled with a green border (`#4ec9a0`)
- The hint line beneath it reads: `"blueprint default · code: <value>  [clear]"`
- Clicking **clear** removes the field from the `"fields"` object, saves the file, and reverts the input to the code-default placeholder state
- Clearing a value that equals the code default is also treated as "no override" (not stored)

### AC-5: Adding a component pre-populates nothing

- When a component is added via the picker, its entry in `"fields"` is an empty object `{}`
- The property panel immediately shows all fields as empty inputs with code-default placeholders (per AC-3)
- No code defaults are auto-written into the file on add — only explicit user edits are stored

### AC-6: `dia reflect` unchanged

- The reflect pipeline stage and `dia reflect` CLI command require no changes — they already write `registeredtypes.diaschema`
- Only `DumpSchema()` in `CluicheTest/Main.cpp` and `SchemaReader` need updating for the new schema format

## Value Hierarchy (reference)

```
Code default (C++ constructor, from schema)
    ↓ overridden by
Blueprint default (stored in .diaentity fields object)
    ↓ overridden by
Instance override (per-placement in .diascene — out of scope for this feature)
```

## File Format Change

No format change — the existing `.diaentity` `"fields"` object already stores explicit values. The change is behavioural: empty `"fields"` now means "use code default" rather than "undefined".

The schema file (`registeredtypes.diaschema`) gains `"default_values"` per component (AC-2).

## Binding Decisions

| Decision | Compliance |
|----------|------------|
| SED-BP-007 (updated): defaults come from C++ constructors, surfaced via schema | This feature implements the mechanism. Code defaults flow from `SaveToJsonFn` → schema → editor UI. Compliant. |
| PD-001 StringCRC for all IDs | Component type IDs remain StringCRC throughout. Compliant. |

## Open Design Questions

1. **`SaveToJsonFn` not implemented on all components.** The fallback is zero/empty per field kind (AC-2). This means the editor will show `0` as the code default for fields that actually construct to something else — a silent lie. Acceptable at current component count; worth a DiaCLI warning when a component's `SaveToJsonFn` is null so the gap is visible.

2. **Blueprint default equals code default — store or discard?** AC-4 says "clearing a value that equals the code default is treated as no override." The inverse — does the user explicitly set `scale = 1.0` (same as code default) get stored? The mockup treats it as an override (green border). This could cause noise in git diffs. Recommend: if the committed value equals the code default, discard it silently. Confirm during implementation.
