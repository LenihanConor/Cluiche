# Feature Spec: DiaValueSchema Entity Component Fields

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md (Feature 1), @docs/specs/applications/dia/systems/diavalueschema/tooltips.md (Feature 3), @docs/specs/applications/dia/systems/diavalueschema/code-side-descriptions.md (Feature 4), @docs/specs/applications/dia/systems/diacli/registered-types-schema.md

**Gameplay Domains:** debug, tools

## Summary

Gives entity component fields the same descriptions as every other value. It adds:

- **`DIA_ATTR_DESCRIBE(ClassName, member, label, unit, description)`**, a reflection attribute registered beside the existing `DIA_ATTR_RANGE` in `FieldAttributeRegistry`;
- a **shared builder**, `DescribeComponentFields`, that joins a component's `FieldDesc`s, its describe and range attributes and its true default values into one `ComponentFieldInfo` per field;
- the **collector** that turns those into `ValueSchemaService` entries (kind `component_field`);
- the extended **`registeredtypes.diaschema`** field entries (label, description, unit, range, default, hint) that offline editors read;
- **`ValueLabel` / `ValueTooltip`** replacing the raw field rows in the Blueprint Editor, Scene Editor and CluicheTest's PropertyGrid.

A range is never written twice: it comes from the existing `DIA_ATTR_RANGE`.

## Problem

Entity component fields have ranges but no explanations. Editors show raw reflected names (`mMass`), and three separate field-row implementations each lack a tooltip. Two metadata systems (reflection attributes and the value schema) would drift if component fields were described separately from their ranges. One description, written next to the range, should feed the Entity Inspector, Blueprint Editor and Scene Editor and the same tooltips as world values.

## Goals

1. **Describe a field once**, next to its range, with a label, unit, description and optional hint.
2. **No duplicated metadata.** Range from `DIA_ATTR_RANGE`; default from a properly constructed component; name from the field itself.
3. **One builder, two outputs.** The `.diaschema` writer and the schema collector both call `DescribeComponentFields`, so they cannot drift.
4. **True defaults.** The default shown is the value a new component actually has.
5. **Tooltips in the entity tools.** The three existing field rows adopt the shared `ValueLabel` and `ValueTooltip`.
6. **No silent loss.** The attribute registry no longer drops registrations past a hidden cap.
7. **Measurable coverage.** Ranged fields with no description are listed in the export (Feature 6) and warned by `dia validate schema` (Feature 2).

## Non-Goals

- Describing the sub-fields of nested structs (a nested field is described as one item).
- Changing how components serialise, validate or enforce ranges.
- New attribute types beyond describe (`DIA_ATTR_REQUIRED`, `DIA_ATTR_RANGE`, `DIA_ATTR_ASSET_REF` are unchanged).
- A UI for editing descriptions.
- Describing component types themselves (they already have `DIA_COMPONENT_DESCRIBE`).

## Architecture

### The describe attribute (DiaCore Reflect)

```cpp
// Dia/DiaCore/Reflect/FieldAttributes.h
struct DescribeAttribute : FieldAttribute {
    static constexpr const char* kKind = "Describe";
    const char* label = nullptr;
    const char* unit = nullptr;          // "" or null: no unit
    const char* description = nullptr;
    const char* hint = nullptr;          // optional
    const char* GetKind() const override { return kKind; }
};

DIA_ATTR_DESCRIBE(Rigidbody, mMass, "Mass", "kg", "How heavy the body is; heavier bodies push lighter ones.")
DIA_ATTR_DESCRIBE_HINT(Rigidbody, mMass, "Mass", "kg", "How heavy the body is.", "Above 1000 the solver loses precision.")
```

- Registered at static-init time exactly like `DIA_ATTR_RANGE` (same `(#TypeName, #fieldName)` key, same anonymous-namespace registrar pattern). All strings are literals.
- **Generic range read.** `FieldAttribute` gains a virtual `bool GetRange(double& lo, double& hi) const` (default false); `RangeAttribute<T>` overrides it for arithmetic `T`. The collector therefore reads any range without knowing `T`.
- **Capacity.** `FieldAttributeRegistry::kMaxEntries` rises from 256 to 2,048, and `Register` counts dropped registrations (`DroppedCount()`) instead of ignoring them silently. The count is logged as an Error at module start and degrades health when above zero.

### True defaults (DiaEntity)

`ComponentTypeDesc` gains `ConstructDefaultFn constructDefault = nullptr;` (`void(*)(void* mem)`), set by `DIA_COMPONENT_REGISTER` to a thunk that placement-news a default-constructed `ClassName`. The field is appended after `description`, and the macro supplies it. Components registered by other means keep `nullptr` and report no default. The builder serialises a constructed instance (through the existing `saveToJson`) and reads each field's value, so the default is the one set by `FIELD(type, name, default)`.

### The shared builder (DiaEntity)

```cpp
// Dia/DiaEntity/ComponentDescribe.h
struct ComponentFieldInfo {
    const char* typeId;        // "foo-component"
    const char* name;          // "speed" (FieldDesc.name)
    const char* label;         // describe label, else the member name
    const char* description;   // or null
    const char* unit;          // or null
    const char* hint;          // or null
    FieldKind   kind;
    bool        hasRange;  double min, max;
    bool        hasDefault;  Json::Value defaultValue;
    bool        described;     // a DescribeAttribute exists
};
unsigned DescribeComponentFields(const ComponentTypeDesc&, ComponentFieldInfo* out, unsigned maxOut);
```

- It joins each `FieldDesc` to the attribute registry by `(CRC(desc.debugName), field.nameCrc)`: `debugName` is the C++ class name and `FieldDesc.name` is the member name, the same identifiers the `DIA_ATTR_*` macros use.
- `Nested` fields are described as one item (label and description only); their `type` is reported as `object` in the `.diaschema` and as `string` with `ui.nested: true` in the service entry.
- DiaEntity gains no dependency on DiaValueSchema.

### Collector (DiaValueSchema)

`ComponentFieldCollector` runs with Feature 4's collector (once after Init, and in describe mode). For every registered component and field it creates a `ValueInfo` and registers it through `ValueSchemaRegistry::Register`:

| Item | Value |
|---|---|
| `for` | the component type-id string, e.g. `foo-component` |
| `key` | the field name, e.g. `speed` (full key `foo-component/speed`) |
| `kind` | `component_field` |
| `label`, `description`, `unit`, `hint` | from the describe attribute; the label falls back to the member name |
| `min`, `max` | from the range attribute |
| `default` | from the constructed default |
| `type` | from the default's JSON type (`bool`, `int`, `float`, `string`); if there is no default, `float` for `Primitive` and `string` for `StringId` |

- DiaValueSchema depends on DiaEntity for this collector only. If `dia check deps` finds a cycle, the collector moves behind an `ICodeSideSource` interface in DiaValueSchema that DiaEntity's module registers; nothing else changes.
- **Undescribed** items for the export: a field with a `DIA_ATTR_RANGE` and no `DescribeAttribute`. Fields with neither are not reported (they are plain data).
- The `DIA_ATTR_RANGE` and `DIA_ATTR_DESCRIBE` of one field must agree in type only through the field; a describe without a range simply has no range.

### Extended `registeredtypes.diaschema`

`dia reflect` runs the game with `--dump-schema` and the writer in the application's `Main` builds the component list. Its field entries today are `{name, kind}`. They gain, when present, `label`, `description`, `unit`, `hint`, `min`, `max` and `default`. The writer is changed to call `DescribeComponentFields`, so the keys come from the shared builder; the call site stays in the application (CluicheTest now, CoW's `Main` when it has components). Existing keys are unchanged, so the Blueprint Editor's `SchemaReader` and the Entity Inspector keep working before they adopt the new keys. The component `default_values` in the same file now come from the constructed default instead of a zero-filled buffer.

### Editor adoption

| Editor | Source of descriptions | Change |
|---|---|---|
| DiaEntityTemplateEditor (`FieldRow.tsx`) | `registeredtypes.diaschema` | Label via `ValueLabel`, tooltip via `ValueTooltip` |
| Scene Editor (`propRow()`) | `registeredtypes.diaschema` | Same |
| CluicheTest `PropertyGrid.tsx` (live Entity Inspector) | `valueschema.get` with prefix = the component type id | Same |

A field with no description renders exactly as today (raw name, no icon). The three rows read a shared small adapter that turns a `.diaschema` field entry or a service entry into the `ValueInfo`-shaped prop `ValueLabel` and `ValueTooltip` take (Feature 3).

### Logging and observation

- **Channel:** `VSC`.
- **Info:** component fields collected (described, undescribed).
- **Error:** attribute registrations dropped (`DroppedCount() > 0`).
- **Warning:** a describe with an empty description; a describe for a field name that is not a registered field of the class (once per pair).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on the component collection pass.
- **Metrics:** component fields described, undescribed, registry entries used and dropped.
- **Health:** Degraded when any attribute registration was dropped.

## Acceptance Criteria

1. `DIA_ATTR_DESCRIBE` and `DIA_ATTR_DESCRIBE_HINT` register a `DescribeAttribute` under `(class, member)`, retrievable with `FindAttribute<DescribeAttribute>` alongside any Range or Required attribute on the same field.
2. `FieldAttribute::GetRange` returns the range of a `RangeAttribute<T>` for arithmetic `T` and false for other attributes; existing attribute tests pass unchanged.
3. `FieldAttributeRegistry` holds 2,048 entries; a registration past the cap is counted in `DroppedCount()`, never silently lost, and is logged as an Error at module start with the health reporter Degraded.
4. `DIA_COMPONENT_REGISTER` sets `constructDefault`; a component registered without it reports no default.
5. `DescribeComponentFields` returns, per field, the label (falling back to the member name), description, unit, hint, range, default and kind, joined by class name and member name; a nested field is one item.
6. The default of a field equals the value set by `FIELD(type, name, default)`, not zero.
7. The collector produces a `component_field` entry per field with full key `<type id>/<field>`, with the range from the range attribute and the type from the default; entries are queryable through `ValueSchemaService`.
8. The export's `undescribed.component_fields` lists ranged fields without a describe and omits the rest.
9. `registeredtypes.diaschema` field entries carry label, description, unit, hint, min, max and default when present and are otherwise unchanged; `default_values` come from the constructed default; the Blueprint Editor and Entity Inspector still load files made before the change.
10. The attribute registry and `DescribeComponentFields` introduce no dependency from DiaEntity or DiaCore on DiaValueSchema (`dia check deps` clean).
11. The Blueprint Editor's `FieldRow`, the Scene Editor's `propRow()` and CluicheTest's `PropertyGrid` show a label, a unit and a tooltip for a described field and render an undescribed field exactly as before.
12. The Entity Inspector shows the same tooltip content for a field as the debug panel shows for a world value with the same entry (shared golden fixture, Feature 3).
13. A describe for a name that is not a field of the class logs one Warning per pair and has no other effect.

## Test Strategy

- **GoogleTests (TDD):**
  - Attribute registration and lookup for describe, range and both on one field; `GetRange` for several `T`.
  - Registry capacity: fill past 2,048, `DroppedCount()`, health.
  - `constructDefault` and true defaults on a fixture component; a component without it.
  - `DescribeComponentFields`: label fallback, unit, hint, range, default, nested fields, join by class and member name.
  - Collector output (keys, `for`, `kind`, `type`) and `undescribed` list.
  - The `.diaschema` writer output for a fixture component: new keys present, old keys unchanged, `default_values` true.
- **Existing suites:** `TestFieldAttributes` and the component registry tests run unchanged.
- **Vitest:** `FieldRow`, `propRow`, `PropertyGrid` with described and undescribed fields; the adapter; the shared tooltip golden.
- **Test utilities** in `DiaEntity/Testing/`: a fixture component with described, ranged and nested fields.
- **Visual gate:** the Entity Inspector against the tooltip mockup (`field-schema-tooltips.html`).

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-003 | Described where defined | `DIA_ATTR_DESCRIBE` sits beside `DIA_ATTR_RANGE`; the range is never restated |
| VS-005 | No singleton | The existing `FieldAttributeRegistry` singleton is only read; the service is reached through the stream |
| PD-004 | No STL in public APIs | Plain structs and fixed arrays |
| DEUI-002 | Theme object, no bundled CSS | The adopted rows use `ValueLabel`/`ValueTooltip` only |
| Engine vs Application boundary | Generic builder in Dia; wiring in the application | `DescribeComponentFields` in DiaEntity; the `--dump-schema` call site stays in each application |
| AD-001 (Dia) | Edits to DiaCore Reflect, DiaEntity and three editors need sign-off | Flagged below |
| Panel contract golden | Panel and page share goldens | The shared tooltip fixture (Feature 3) is reused |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1, 3, 4 and 6 | Service, tooltip components, collector, export | Approved |
| DiaCore Reflect (`FieldAttributes.h`) | `DescribeAttribute`, `GetRange`, capacity | Done (extended; sign-off needed) |
| DiaEntity (`ComponentTypeDesc`, `DIA_COMPONENT_REGISTER`) | `constructDefault`, builder | Done (extended; sign-off needed) |
| `dia reflect` / `registeredtypes.diaschema` | Extended field entries | Done (extended) |
| DiaEntityTemplateEditor, DiaSceneEditor, CluicheTest UI | Row adoption | Done (extended; sign-off needed) |

## Open Design Questions

1. **Dependency direction.** The collector makes DiaValueSchema depend on DiaEntity. `dia check deps` decides when the module is scaffolded; the fallback (an `ICodeSideSource` interface that DiaEntity's module registers) is specified above and changes nothing else.
2. **Adoption order.** The three editors can adopt independently. Do the Entity Inspector (`PropertyGrid`) first, because it reads the live service and proves the whole chain; the Blueprint and Scene editors follow from the `.diaschema` once it is regenerated with `dia reflect`.
3. **Descriptions for existing components.** Writing `DIA_ATTR_DESCRIBE` for the existing components is content work, not part of this feature. `dia validate schema` warns about ranged fields without a describe, so the list builds itself.

## Status

`Approved` (2026-10-20). Plan: [entity-component-fields.plan.md](entity-component-fields.plan.md). AD-001 sign-off given 2026-10-21 for the DiaCore Reflect and DiaEntity edits; the three editor edits still need it.
