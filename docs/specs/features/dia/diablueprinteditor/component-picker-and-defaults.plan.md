**Spec:** @docs/specs/features/dia/diablueprinteditor/component-picker-and-defaults.md
**Status:** Not Started

## Implementation Patterns

### T1 — DumpSchema default_values (C++)
- In `DumpSchema()` (`CluicheTest/Main.cpp`), after building the `fields` array for each component, call `desc.saveToJson` on a zero-initialised instance of the component to get default values.
- Allocate a zeroed byte buffer of `desc.size` bytes on the heap, cast to `IComponent*`, call `desc.saveToJson(ptr, outJson)`.
- If `desc.saveToJson` is null, emit `"default_values": {}` with all fields set to `0`/`""`/`false` based on `FieldKind`.
- Add `"default_values": { "field": value, ... }` as a sibling to `"fields"` on each component entry.
- Emit a `// warn` comment (via `fprintf(stderr, ...)`) when `saveToJson` is null so the gap is visible in pipeline output.

### T2 — SchemaReader exposes default_values
- Add `SchemaDefaultValues` struct: `char fieldName[64]` + `Json::Value value` + `DynamicArrayC<..., 32>`.
- Add `GetDefaultValues(uint32_t componentIndex)` returning `const SchemaDefaultValues&`.
- In `LoadFromFile`, after parsing `fields`, parse `"default_values"` object into the new struct.

### T3 — BlueprintPropertyController: code defaults in BuildPropertyJson
- `BuildPropertyJson` already enriches fields from `ComponentRegistry` — change it to also look up code defaults from `SchemaReader` (passed by ref, same as `BuildAvailableComponentsJson`).
- Each field JSON entry gains `"codeDefault": <value>` when a default is known for that field name.
- Pass `const SchemaReader& schema` as a new parameter (same pattern as T6 from previous feature).

### T4 — Blueprint editor UI: component picker
- Replace the `<select id="add-component-select">` + "Add" button block in `index.html` with a `<button>+ Add Component</button>` trigger and a `.picker-panel` div (from mockup).
- JS: `openPicker()`, `closePicker()`, `renderPickerList(query)`, `onPickerFilter()`, `onPickerKey(event)`, `onPickerAdd()` — all self-contained, no C++ changes needed.
- Picker data comes from the existing `blueprint_editor.get_available_components` response — already returns `typeId`, `label`, `description`.

### T5 — Blueprint editor UI: field default display + blueprint overrides
- In `buildFieldRow()`: read `field.codeDefault` from the property JSON (added in T3).
- If `field.value === undefined` and `field.codeDefault !== undefined`: input is empty, `placeholder = String(codeDefault)`, hint = "code default: X · leave blank to keep".
- If `field.value !== undefined`: input shows stored value, green border, hint = "blueprint default · code: X  [clear]".
- Clear button calls `blueprint_editor.update_field` with an empty-string sentinel, or a new `blueprint_editor.clear_field` handler (simpler: reuse `update_field` with `value: null` and handle on C++ side — see T6).
- If committed value equals code default, send `value: null` to discard (no override stored).

### T6 — BlueprintMutator / C++ side: handle null/clear field value
- `blueprint_editor.update_field` handler: if `data["value"].isNull()`, call a new `BlueprintMutator::ClearField(root, topKey, componentType, fieldName)` that removes the key from `"fields"`.
- `BlueprintMutator::ClearField` is a one-liner: `root[topKey]["components"][i]["fields"].removeMember(fieldName)`.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **`DumpSchema` exports `default_values`.** Allocate zeroed instance per component, call `saveToJson`, emit `"default_values"` object. Null `saveToJson` → zero-fill all fields by kind. | `dia reflect --target cluichetest` → schema has `default_values` on every component; `scale` is `1.0` for TransformComponent | Not Started | sonnet | stderr warn when saveToJson null. Heap-alloc + free per component, aligned to `desc.alignment`. |
| 2 | **`SchemaReader` exposes default_values.** Add `SchemaDefaultValues` struct + `GetDefaultValues(i)` accessor. Parse `"default_values"` from schema JSON in `LoadFromFile`. | GoogleTest: load schema with default_values → `GetDefaultValues` returns correct values; missing key → empty struct | Not Started | sonnet | Add to existing `TestSchemaReader.cpp`. |
| 3 | **`BuildPropertyJson` includes `codeDefault` per field.** Pass `const SchemaReader& schema` into `BuildPropertyJson`. Look up `default_values` by component typeId and inject `"codeDefault"` into each field entry. | GoogleTest: `BuildPropertyJson` with loaded schema → fields have `codeDefault`; missing schema → fields have no `codeDefault` | Not Started | sonnet | Signature change mirrors existing `BuildAvailableComponentsJson` pattern. Update plugin call-site + existing tests. |
| 4 | **Component picker UI.** Replace `<select>` + Add button with `+ Add Component` button + picker panel. Search, keyboard nav (↑↓/Enter/Esc), double-click, count line, description row. | Open picker → type partial name → list filters → Enter adds component → accordion appears | Not Started | sonnet | Pure HTML/JS in `DiaBlueprintEditor/UI/index.html`. No C++ changes. |
| 5 | **Field default display + blueprint override UX.** `buildFieldRow` reads `field.codeDefault`; shows placeholder when no stored value; green border + clear button when override present. Discard if committed value equals code default. | Add component → fields show code-default placeholders. Edit `scale` to `2.0` → green border. Clear → placeholder returns. Edit to same as code default → no green border stored. | Not Started | sonnet | Pure JS changes in index.html. Depends on T3 (codeDefault in field JSON). |
| 6 | **`BlueprintMutator::ClearField` + update_field null handling.** New `ClearField(root, topKey, compType, fieldName)`. `update_field` handler treats `value: null` as clear. | GoogleTest: `ClearField` removes key from fields object; absent key is no-op. JS: clear button sends null → field removed from file. | Not Started | haiku | Small C++ addition + handler tweak. |
| 7 | **`dia reflect` re-run + integration check.** After T1, run `dia reflect --target cluichetest`. Confirm schema has `default_values`. Open editor, add TransformComponent, confirm `scale` placeholder shows `1`. | `dia pipeline --target cluichetest` passes. Editor shows correct placeholders. | Not Started | sonnet | Manual verification step. Run googletest --filter="SchemaReader*:BlueprintPropertyController*". |

## Implementation Order

```
T1 (schema) → T2 (SchemaReader) → T3 (BuildPropertyJson)
T4 (picker UI) — independent of T1-T3, can run in parallel
T5 (field UX) — depends on T3 (codeDefault in JSON)
T6 (ClearField) — depends on T5 (clear button)
T7 (integration) — after all above
```
