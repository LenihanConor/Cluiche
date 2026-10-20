# System Spec: DiaValueSchema

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, tools

**Origin:** 2026-10-19 world-sim product review. The user asked for "more explanation (tool tips, etc.) on each value", in the testbeds, the inspector and the editor. **First consumer:** CoW (F-3 World Assets files, A1/A4 fields and metrics). **Mockup:** @docs/specs/applications/cow/systems/f3-worldassets/mockups/field-schema-tooltips.html

## Purpose

DiaValueSchema gives every value a game exposes one description, written once and shown everywhere. "Value" here means a tunable parameter, a per-cell state field, an entity component field, a metric, or a command argument. Each description has a label, a plain-language explanation, a unit, a range, a default, what the value affects, related values and UI hints, including a colour ramp.

From one definition you get:
- slider tooltips and default marks in debug panels
- tooltips in inspector rows
- forms and column headers in the editor
- chart axes in the inspector
- CellField colour ramps
- `dia validate` range and coverage checks
- a generated reference page

Nothing like this exists today:
- Debug-panel controls carry only `{key, label, min, max, step, value}`.
- `MetricRegistry` metrics have only a name.
- `CommandInfo` has command-level text but no argument descriptions.

## Responsibilities

- Own the `dia.value_schema` file format (sidecar files) and its loader.
- Own `ValueInfo` and the runtime lookup `ValueSchemaRegistry` (key → `ValueInfo`). It is published as a service; no singleton.
- Own the code-side description APIs: `MetricInfo` for metrics, `ValueParam` for DiaAPI command arguments, and `DIA_ATTR_DESCRIBE` for entity component fields. These feed the same registry.
- Own the debug-panel tooltip contract: controls and inspector rows carry a `schemaKey`, and the panel shows description, unit, range and default.
- Own the named colour-ramp library and its mapping to CellField ramps.
- Own the export: `dia schema export`, the DiaDebugServer query `valueschema.get` and `dia docs value-reference`.
- Own `dia validate schema`.

## Non-Responsibilities

- **The meaning of values.** Each system writes its own schema entries; CoW writes CoW's.
- **Loading the data files themselves.** F-3 and each system load the values; DiaValueSchema only describes them.
- **Localisation.** v1 is English text. A later `locale` key is possible, but nothing is designed for it now.
- **Editor UI.** The CoW World Data Editor and World Sim Inspector consume the schema; they are specced separately.

## Design

### Where descriptions are written

The rule is **describe values where they are defined**:

| Value kind | Defined in | Described in |
|---|---|---|
| Data-file parameter (e.g. `grazing.rate` in `ecology_parameters.json`) | JSON data file | **Sidecar** `<file>.schema.json` next to it |
| State field (e.g. `vegetation` per cell) | Code, shown by panels and layers | **Sidecar** fields schema (presentation lives with data) |
| Metric | `MetricRegistry::Register*` in code | **`MetricInfo`** passed at registration |
| Command argument | `CommandRegistry` (and editor actions) in code | **`ValueParam`** list on `CommandInfo` (the existing `EditorActionParam`, moved into DiaAPI) |
| Entity component field | `DIA_FIELD_*` in a component | **`DIA_ATTR_DESCRIBE`**, next to the existing `DIA_ATTR_RANGE` in `FieldAttributeRegistry`; exported into `registeredtypes.diaschema` field entries |

At runtime all five end up in one `ValueSchemaRegistry`, so consumers never care where an entry came from.

### Sidecar format

```json
// Cluiche/Assets/CoW/Global/Ecology/ecology_parameters.schema.json
{
  "format": "dia.value_schema", "version": 1,
  "for": "cow.ecology_params",
  "entries": [
    {
      "key": "grazing.rate",
      "kind": "parameter",
      "label": "Grazing rate",
      "description": "How much vegetation herbivores eat each season, relative to normal.",
      "unit": "× per season",
      "type": "float", "min": 0, "max": 2, "default": 1.0,
      "affects": ["vegetation", "herbivores"],
      "related": ["regrowth.forest"],
      "ui": { "group": "Herbivory", "step": 0.05, "decimals": 2 }
    }
  ]
}
```

```json
// Cluiche/Assets/CoW/Global/Ecology/ecology_fields.schema.json
{
  "format": "dia.value_schema", "version": 1,
  "for": "cow.ecology.cell",
  "entries": [
    {
      "key": "vegetation",
      "kind": "field",
      "label": "Vegetation",
      "description": "Plant cover in the cell, from bare (0) to full canopy (1).",
      "hint": "Below 0.2 herbivores start to starve; above 0.8 forest can establish.",
      "unit": "fraction", "type": "float", "min": 0, "max": 1,
      "source": "A1 step 6 (producers)",
      "ui": { "ramp": "sequential.green", "layer": "ecology.vegetation" }
    }
  ]
}
```

**Entry keys:**

| Key | Required | Notes |
|---|---|---|
| `key` | yes | A dotted path into the data file, or a field name. Unique within `for`. The full key is `<for>/<key>` (StringCRC). |
| `kind` | yes | `parameter`, `field`, `metric`, `arg`, or `component_field` (Feature 7) |
| `label` | yes | Short UI name |
| `description` | yes | 1–2 plain sentences |
| `type` | yes | `float`, `int`, `bool`, `enum` or `string`. An `enum` also needs `values: [{id, label, description}]` |
| `unit` | numeric types | Free text: "fraction", "mm/week", "× per season" |
| `min`, `max` | numeric types | Inclusive range |
| `default` | parameters | Must match the defaults file (checked by validate) |
| `hint` | no | "How to read it", shown in inspector tooltips |
| `affects`, `related` | no | Keys within the same game; validate checks they resolve |
| `source` | no | Provenance text, e.g. which system step writes the value |
| `ui` | no | `group`, `step`, `decimals`, `ramp`, `layer`, `hidden` |

**Tables and references** (amended 2026-10-19, T-2 World Data Editor):

| Addition | Form | Meaning |
|---|---|---|
| Record paths | `"key": "species[].growth_rate"` | One entry describes a column of an array of records |
| Unique | `"unique": true` | Values must be unique within the array (ids) |
| Required in record | `"required": true` | The column must be present in every record |
| References | `"type": "ref"` or `"ref[]"`, with `"ref": "cow.biomes/biomes[].id"` | The value must equal an id in another format's record array |

`dia validate schema` checks references, uniqueness and required columns. A failed reference gets a closest-match suggestion ("did you mean TemperateForest?").

`for` names the data file's `format` id (F-3 envelope), so a defaults file and every pack of that format share one schema.

### Code-side descriptions

```cpp
// DiaObservation: metric registration gains an optional MetricInfo
MetricInfo info{ "Herbivores (world)", "Total herbivore population across all cells.", "animals" };
mMetrics.RegisterGauge(StringCRC("cow.ecology.herbivores_total"), info);

// DiaAPI: CommandInfo gains args
Dia::API::ValueParam params[2];   // formerly EditorActionParam (DiaEditor)
params[0].name = "kind"; params[0].type = "enum";   params[0].required = true;
params[0].description = "Which pack to save: weather or ecology."; params[0].enumValues = "weather|ecology";
params[1].name = "id";   params[1].type = "string"; params[1].required = true;
params[1].description = "Asset id for the new pack.";
```

- Both are optional, so every existing call site keeps compiling.
- `dia validate schema` reports the missing ones as coverage gaps (see Validation).
- `HelpSystem` and the palette show the argument text.

### Reuse of existing pieces (amended 2026-10-19)

A survey found that no tool shows per-value descriptions today. Field labels are raw reflected names, and descriptions exist only per component, debug domain and command. It also found three pieces to reuse rather than duplicate:

1. **Command arguments reuse `EditorActionParam`.** `Dia/DiaEditor/EditorAPI/EditorActionDescriptor.h` already has `EditorActionParam {name, type, required, description}`, with up to 8 per action, used by 6 editor plugins.
   - It moves down into DiaAPI as `Dia::API::ValueParam` and gains optional `unit`, `min`, `max` and `enumValues`.
   - `EditorActionParam` becomes an alias of it, so editor plugins keep compiling.
   - DiaAPI `CommandInfo` gains the same list.
   - There is one type for both registries; no separate `ValueParam`.
2. **Component fields extend the existing `.diaschema` export.** `Cluiche/CluicheTest/Main.cpp:109-119` already writes each component's `description` into `registeredtypes.diaschema`, but its field entries are just `{name, kind}`.
   - Feature 7 adds `label`, `description`, `unit`, `min`, `max` and `default` to those field entries, taken from `FieldAttributeRegistry` and the existing defaults.
   - The Blueprint Editor's `SchemaReader` and the Entity Inspector read them from there. There is no second export path for component fields.
3. **One shared tooltip UI.** Field rows exist three times today: `DiaEntityTemplateEditor/UI/src/components/FieldRow.tsx`, the Scene Editor's `propRow()` and CluicheTest's `PropertyGrid.tsx`. None of them has a tooltip.
   - `@dia/editor-ui` (`Dia/DiaEditorUI`) gains `ValueLabel` (label, unit, (i) icon) and `ValueTooltip` (description, range, default, affects, related, source, hint), and the editor rows adopt them. T-2 also adds a generic `CellGridCanvas` (flat-top hex or square grid with ramp or palette, hover, selection and brush), shared with T-1 (amended 2026-10-19).
   - The in-game debug panel gets one matching `renderValueTooltip()` in its shared panel script, so it isn't copied per panel.

### Runtime

- `ValueSchemaModule` (any PU that needs it) loads every `*.schema.json` reachable from the app's path aliases at start.
  - Its `ValueSchemaService` (a `ServiceStream`, following the existing service pattern; no singleton) answers `Find(fullKey)` → `const ValueInfo*`.
  - It also answers `ForFormat(formatId)` → a list of entries.
- `MetricRegistry` and `CommandRegistry` push their code-side infos into the service when registering.
- **Out-of-range values at runtime.** Loaders that use the schema clamp the value and log one Warning naming the key and range (channel `VSC`). The same rule applies to F-3 pack overrides.

### Debug-panel tooltips

- A control in `IDebugDomain::GetJSONState` may add `"schemaKey": "cow.ecology_params/grazing.rate"`. Inspector rows may do the same.
- When a `schemaKey` resolves, the panel takes `label`, `min`, `max` and `step` from the schema unless the control overrides them.
- On hover or via an (i) icon, the panel shows `description`, `unit`, range, `default`, `affects`, `related` and `source`.
- The slider track shows a mark at the default. If a pack overrides the value, the row shows a "pack: <id>" marker (F-3).
- The panel receives the schema once, with a query, not in every state frame.
- The panel contract golden test (`TestPanelContract`) is extended.

### Ramps

- A small named library ships in Dia: `sequential.green`, `sequential.blue`, `sequential.heat`, `diverging.blue_red`, `categorical.12`.
- `ui.ramp` names one of them. `ValueSchema::ToCellFieldRamp(info)` returns the CellField ramp with the entry's `min` and `max`.
- The Layer Viewer uses this, so every field layer gets the same colours and legend everywhere.

### Export, query and reference

- **`dia schema export --target <t>`** writes the merged schema to `Cluiche/out/<App>/valueschema.json` (PD-009). It covers sidecars plus code-side infos. Code-side infos are collected by running the app headless in `--automation` describe mode; see Open Q1.
- **DiaDebugServer query `valueschema.get {prefix?}`** gives the editor and inspector the live merged schema.
- **`dia docs value-reference --target <t>`** generates `docs/reference/<app>/values.md`: one table per `for` format, plus metrics and commands.

### Validation (`dia validate schema`)

**Fails on:**
- a data-file or pack key with no schema entry
- an entry with no `description`
- a value outside `min`/`max`, or not one of the enum ids
- a `default` that differs from the defaults file
- an `affects` or `related` key that doesn't resolve
- an unknown `ui.ramp`
- a `ref` that matches no id in the referenced format (with a closest-match suggestion), a duplicate `unique` value, or a missing `required` column

**Warns on:**
- metrics or commands with no `MetricInfo` or `ValueParam` (coverage gap), once code-side infos are available to the validator (Open Q1)
- component fields with `DIA_ATTR_RANGE` but no `DIA_ATTR_DESCRIBE`, on the same condition

```
$ dia validate schema
  FAIL Global/Ecology/ecology_parameters.json
       key 'grazing.juvenile_rate' has no schema entry
  FAIL Global/Ecology/Packs/lush.ecology.json
       'grazing.rate' = 2.5 outside [0, 2]
  OK   Global/Weather/weather_parameters.json (24 keys)
```

`dia docs precommit` runs it on changed data and schema files.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Format, loader and service | `ValueInfo`, the sidecar loader (including record paths, `unique`, `required`, `ref`), `ValueSchemaModule` / `ValueSchemaService`, lookup by full key and by format, clamp-and-warn helper | Draft | TBD |
| 2 | `dia validate schema` | DiaCLI validator (Python, like `cli_validate.py`), precommit hook | Draft | TBD |
| 3 | Tooltips (panel and editor UI) | Controls and rows take `schemaKey`; shared `ValueLabel`/`ValueTooltip` in `@dia/editor-ui` plus `renderValueTooltip()` in the debug panel; default mark, pack marker; schema query; contract golden | Draft | TBD |
| 4 | Code-side descriptions | `MetricInfo` on `MetricRegistry`; `EditorActionParam` moved to DiaAPI as `ValueParam` (alias kept) and added to `CommandInfo`; `HelpSystem` and palette show the args | Draft | TBD |
| 5 | Ramps | Named ramp library, `ToCellFieldRamp`, legend data | Draft | TBD |
| 6 | Export, query and reference | `dia schema export`, `valueschema.get`, `dia docs value-reference` | Draft | TBD |
| 7 | Entity component fields | `DIA_ATTR_DESCRIBE(Type, field, label, unit, description)` alongside the existing `FieldAttributeRegistry` ranges. It feeds `ValueSchemaService` (kind `component_field`), so the Entity Inspector, Blueprint Editor and Scene Editor show the same tooltips as world values. Without it, entity fields have ranges but no explanations, and two metadata systems drift apart | Draft (v1, user decision 2026-10-19) | TBD |

- **Build order:** 1 → 2 → 3, then 4 and 7 (code-side descriptions), 5 (with or after CellField Step 1) and 6.
- **Feature 7 consumers:** the Entity Inspector, Blueprint Editor and Scene Editor show the same tooltips as the debug panels, read from the extended `registeredtypes.diaschema` and rendered with the shared `ValueTooltip`. Each component field resolves to the full key `<ComponentType>/<field>`, and its range comes from the existing `RangeAttribute`, so it is never stated twice.
- **CoW content** is CoW's own work: schema files for every F-3 defaults file, the A1/A4 fields schemas, plus `MetricInfo` and `ValueParam` on CoW registrations. It goes into F-3 Feature 1's scope and each system's Feature 1.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaCore Json, StringCRC, `PathStore` | Loader, keys | Done |
| DiaApplicationFlow `ServiceStream` | `ValueSchemaService` | Done |
| DiaObservation `MetricRegistry` | `MetricInfo` | Done (extended) |
| DiaAPI `CommandRegistry`, `HelpSystem`; DiaEditor `EditorActionDescriptor` | `ValueParam` (moved from DiaEditor) | Done (extended) |
| `@dia/editor-ui` (DiaEditorUI) | Shared `ValueLabel` / `ValueTooltip` | Done (extended) |
| `dia reflect` / `registeredtypes.diaschema` export | Component field descriptions (Feature 7) | Done (extended) |
| DiaVisualDebugger panel and `IDebugDomain` | Tooltips | Done (extended) |
| DiaBgfx CellField | Ramps | Approved, not built |
| DiaCore `FieldAttributeRegistry` / DiaEntity | Entity component fields (Feature 7) | Done (extended) |
| DiaEntityInspector, DiaBlueprintEditor, DiaSceneEditor | Tooltips for component fields (Feature 7) | Done (extended) |
| DiaDebugServer query registry | `valueschema.get` | Done |
| DiaCLI | validate, export, docs | Done (extended) |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|---|---|---|---|---|
| VS-001 | A Dia system; games ship only content (user decision 2026-10-19) | Every consumer is Dia (panel, CellField, CLI, editor); any game has tunables | Accepted | Yes |
| VS-002 | Data parameters and fields are described in sidecar `.schema.json` files keyed by the data file's `format` | Defaults and packs stay plain values with clean diffs; one schema serves every pack of a format | Accepted | Yes |
| VS-003 | Metrics and command arguments are described in code at registration (`MetricInfo`, `ValueParam`) | They are defined in code, so describing them there stops the description drifting from the definition | Accepted | Yes |
| VS-004 | Strict coverage: `dia validate schema` fails if any data key lacks an entry or description, or is out of range. At runtime, out-of-range values are clamped with a Warning | Tooltips can't silently go missing, and bad packs are caught before runtime (user decision 2026-10-19) | Accepted | Yes |
| VS-005 | No singleton: lookup goes through `ValueSchemaService` | User rule: no singletons without sign-off | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Implication |
|---|---|---|
| PD-001 | Platform | Full keys and format ids are StringCRC |
| PD-004 / AD-002 | Platform / Dia | `ValueInfo` lists use DiaCore containers, not STL |
| PD-009 | Platform | `dia schema export` writes under `Cluiche/out/<App>/` |
| AD-001 | Dia | New module `Dia/DiaValueSchema/` with a module doc |
| AD-003 | Dia | Namespace `Dia::ValueSchema` |
| AD-004 | Dia | `ValueSchemaModule` is a normal module; the service is a `ServiceStream` |

## Test Strategy

- **GoogleTests:**
  - The loader rejects missing required keys.
  - Lookup works by full key and by format.
  - The enum `values` parse.
  - The clamp helper clamps and warns once per key.
  - `ToCellFieldRamp` maps min and max.
  - `MetricInfo` and `ValueParam` reach the service.
- **DiaCLI pytest:** one fixture per fail rule in Validation, and the OK path.
- **Panel contract golden:** a control with `schemaKey` serialises with tooltip data.
- **CoW proof** (when CoW content exists): `dia validate schema` passes on every F-3 defaults file and pack. The S-3 slider tooltip shows the schema description; this is the visual gate against the mockup.

## Resolved Decisions

1. **Code-side descriptions reach the CLI through a headless export** (2026-10-19). `dia schema export` runs the app in describe mode and dumps `MetricInfo`, `ValueParam` and `DIA_ATTR_DESCRIBE` entries. `dia validate schema` always checks the sidecar files, and checks code-side coverage only when an export exists, so validation never needs a build.
   - Rejected: sidecars for metrics and commands, because it breaks VS-003.
2. **Units are free text in v1** (2026-10-19), plus an optional `ui.format` (`percent`, `fixed:2`). A unit set can come later if charts need to convert units.
3. **Entity component fields are in v1** (Feature 7, user decision 2026-10-19).

## Status

`Approved` (2026-10-19). Feature specs TBD; no plan yet.
