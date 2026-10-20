# Feature Spec: DiaValueSchema Tooltips (Panel and Editor UI)

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md (Feature 1)
**Mockup:** @docs/specs/applications/cow/systems/f3-worldassets/mockups/field-schema-tooltips.html

**Gameplay Domains:** debug, tools

## Summary

Makes the schema visible. A control or row that names a `schemaKey` gets its label, range, step and default from DiaValueSchema, and shows a tooltip with the description, unit, range, default, what it affects, related values, source and, when a pack overrides it, a "pack: id" badge. It delivers four things:

1. the `schemaKey` contract for `IDebugDomain::GetJSONState` controls and inspector rows, with a small C++ helper that checks keys;
2. the `valueschema.get` DiaDebugServer query and a schema version, which the panel uses to fetch descriptions once;
3. `renderValueTooltip()` in the debug panel's shared script (plain JS, Ultralight);
4. `ValueLabel` and `ValueTooltip` in `@dia/editor-ui` (React), for T-1, T-2 and the editors.

A shared golden tooltip fixture keeps the two renderers consistent. Replacing existing editor field rows (three of them) is not part of this feature; that is Feature 7.

## Problem

Debug-panel controls carry only `{key, label, min, max, step, value}`; editors show raw reflected names; nothing shows what a value means, what its unit is, what it affects or whether a pack changed it. Feature 1 gives the data a home; this feature puts it in front of the user in every tool that shows a value.

## Goals

1. **One contract for panels.** A control or inspector row may carry `"schemaKey": "<for>/<key>"`; the panel resolves the rest from the schema.
2. **Schema fills the gaps.** Label, `min`, `max`, `step` and `decimals` come from the schema unless the control overrides them.
3. **Rich tooltip.** Hover or an (i) icon shows description, unit, range, default, affects, related, source and, in inspector rows, the hint.
4. **Default mark and pack marker.** A slider shows a tick at the schema default; a control that a pack overrides shows "pack: <id>".
5. **Fetch once.** The panel asks for schema data once per open, caches it, and refreshes only when the schema version changes.
6. **Consistent everywhere.** The panel and the React components render the same sections for the same entry, tested against one golden fixture.
7. **Fail quietly, log once.** A key that doesn't resolve gives the raw label with no tooltip and one Warning (`VSC`).

## Non-Goals

- Replacing the three existing field rows (DiaEntityTemplateEditor `FieldRow`, Scene Editor `propRow`, CluicheTest `PropertyGrid`); that is Feature 7, where component-field descriptions exist.
- Editing schema text from the UI.
- Localisation.
- Code-side descriptions (Feature 4), ramps (Feature 5), file export and the reference page (Feature 6).
- A tooltip for every possible widget: only value controls and inspector rows in this feature.

## Architecture

### Control and row contract

A control in `GetJSONState` (and an inspector row) may add:

```json
{ "name": "grazing.rate", "type": "slider", "value": 0.8,
  "schemaKey": "cow.ecology_params/grazing.rate",
  "packId": "lush" }
```

- `schemaKey` is the full key from Feature 1. Without it nothing changes: existing controls and their golden files are untouched.
- `packId` is optional. A provider sets it when F-3 reports that a pack overrides the value; the panel shows the badge.
- Resolution on the page, once the schema is cached:
  - `label` = control `label` if given, else the schema `label`;
  - `min`, `max` = control values if given, else the schema's;
  - `step`, `decimals` = control values, else `ui.step`, `ui.decimals`;
  - default tick from the schema `default`; the tick is hidden when the entry has no default.
- The page treats a missing field as "use the schema", so providers send only what is special.

### C++ helper (DiaVisualDebugger)

`SchemaControl::Add(Json::Value& controls, const char* name, StringCRC schemaKey, float value, const char* packId = nullptr)` builds the control JSON. When a `ValueSchemaService` is available it checks the key once per key and logs one Warning (`VSC`) if it does not resolve. When no service is present, it still emits the key and skips the check, so domains work with or without DiaValueSchema running.

### Schema delivery

- **Query `valueschema.get {prefix?}`** (DiaDebugServer, new here): returns the entries whose full key starts with `prefix` (all if omitted), plus `version`. The query reads the service snapshot; it adds no new state. Entry JSON uses the field names of the sidecar format.
- **Schema version:** a number that changes whenever a new snapshot is published (Feature 1). The domain state frame carries the current `schemaVersion` as one number.
- **Panel flow:** on open, collect the distinct `schemaKey` prefixes in the domains' state, call `valueschema.get` once per prefix, cache the entries by key, and re-query only when a frame reports a different `schemaVersion`.

Feature 6 adds the export file, the reference page and schema embedding in exports on top of this query; it does not redefine it.

### Tooltip content

For one `ValueInfo`, sections in order (from the mockup):

| Section | Source | Shown when |
|---|---|---|
| Title | `label` | always |
| Description | `description` | always |
| Hint | `hint` | inspector rows only |
| Meta row | `unit`, `min`–`max`, `default` | each item only if present |
| Affects | `affects` as chips | non-empty |
| Related | `related` as links | non-empty; a link focuses the matching control if it is on the panel, else plain text |
| Source | `source`, and "Defined in `<for>` · `<key>`" | present |
| Pack badge | "overridden by pack `<id>`" | `packId` set |

Enum entries list their values (label and description) after the description. Hover shows the tooltip, keyboard focus on the (i) icon shows it too, and Esc closes it.

### Renderers

- **Debug panel:** `renderValueTooltip(entry, options)` in the panel's shared script returns the tooltip element. One implementation serves every panel; no per-panel copy. Styling follows the panel's existing theme (Alpine.js / DaisyUI) and the mockup.
- **Editor UI:** `ValueLabel` (label, unit, (i) icon) and `ValueTooltip` in `Dia/DiaEditorUI/src/`, exported from the package index. They take a `ValueInfo`-shaped prop (not a service), so editors fetch the schema their own way (`valueschema.get` live or an embedded schema in a file). Styling uses the `theme` object only (DEUI-002).
- **Golden fixture:** `Cluiche/Tests/Contracts/valuetooltip/` holds schema entries (a parameter with all fields, an enum, a bare entry) and an `expected.json` listing for each the sections that must appear and their text. Vitest in both the panel's page tests and `@dia/editor-ui` loads the same files.

### Logging and observation

- **Logs:** `VSC` Warning once per unresolved `schemaKey` (C++ helper); the query logs at Debug.
- **Traces:** `DIA_TRACE_ZONE` around `valueschema.get`.
- **Profiling:** `DIA_PROFILE_SCOPE` on the query.
- **Metrics:** queries served, unresolved keys.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. A control with `schemaKey` takes its label, `min`, `max`, `step` and `decimals` from the schema; any of them present on the control wins.
2. A control without `schemaKey` renders and serialises exactly as before; the existing golden files are unchanged.
3. Hovering a resolved control, or focusing its (i) icon, shows the tooltip sections in the order above; Esc closes it; sections with no data are omitted.
4. A slider shows a default tick at the schema `default`; no tick when the entry has none.
5. A control with `packId` shows "overridden by pack `<id>`" in the tooltip and a "pack: `<id>`" marker on the row.
6. Enum entries list their values with labels and descriptions.
7. A Related link focuses the matching control when it is on the panel, and is plain text otherwise.
8. `valueschema.get` returns entries for a prefix (or all) plus the version, using the sidecar field names, and the panel calls it once per prefix per open.
9. The panel re-queries only when a frame's `schemaVersion` differs from its cache.
10. An unresolved `schemaKey` renders the raw label with no tooltip, and the C++ helper logs one Warning per key.
11. `SchemaControl::Add` emits the key and skips the check when no `ValueSchemaService` is present.
12. `renderValueTooltip` and `ValueTooltip`/`ValueLabel` both pass the shared golden fixture.
13. `TestPanelContract` is extended with a golden state containing a control with `schemaKey` and `packId`, and the panel page tests feed the same file.
14. `ValueLabel` and `ValueTooltip` are exported from `@dia/editor-ui` and use only the `theme` object for styling.
15. Visual match against the mockup's tooltip, default tick and pack badge.

## Test Strategy

- **GoogleTests:** `SchemaControl::Add` output (with and without service, with and without `packId`); one Warning per unresolved key; `valueschema.get` prefix filtering and version; `TestPanelContract` golden for the new state.
- **Vitest, panel page (`debugpanel` tests):** resolution rules (control overrides, schema fills), default tick, pack badge, cache and re-query on `schemaVersion`, Related focus, keyboard open and Esc, missing key.
- **Vitest, `@dia/editor-ui`:** `ValueLabel` and `ValueTooltip` against the golden fixture, theme-only styles, missing optional sections.
- **Visual gate:** compare against `field-schema-tooltips.html` before Done.
- **CoW proof (when CoW content exists):** the S-3 slider tooltip shows the schema description; this is the Feature 3 gate in the system spec.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-003 | Descriptions live where values are defined | The UI only reads; nothing is duplicated into panel code |
| VS-005 | No singleton | Lookup through the service; the page caches only the query result |
| DEUI-001 / DEUI-002 | Local package; no bundled CSS, theme object | `ValueLabel`/`ValueTooltip` use inline styles and `theme` |
| DEUI-007 | `file:` path reference | Editors pick up the new exports without changing how they reference the package |
| SD-001 (DiaDebugDomain) | Debug panel is Ultralight HTML (Alpine.js / DaisyUI) | `renderValueTooltip` is plain JS in the panel's shared script |
| Panel contract golden | The real C++ builders and the page share golden files | New golden state added; existing files unchanged |
| AD-001 (Dia) | Edits to DiaVisualDebugger, DiaEditorUI and the DiaDebugServer query registry | Reviewed and signed off before build |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | `ValueSchemaService`, snapshot version | Approved |
| DiaDebugServer query registry | `valueschema.get` | Done (extended) |
| DiaVisualDebugger panel, `IDebugDomain` | Contract and shared script | Done (extended) |
| `@dia/editor-ui` (DiaEditorUI) | `ValueLabel`, `ValueTooltip` | Done (extended) |
| F-3 | Sets `packId` on overridden controls | Specced (CoW); this feature only defines the field |

## Open Design Questions

1. **Control type names.** The panel today documents sliders as part of the domain card; the exact JSON control types for value controls should be confirmed against the current panel script when this is built, and `schemaKey` added to each value control type (not just `slider`).
2. **Query ownership.** The system spec lists `valueschema.get` under Feature 6; this feature defines it because the panel needs it. Feature 6 should be amended to say it extends rather than defines the query.
3. **Tooltip size on small panels.** The debug panel sits on the left side and is narrow. Check the tooltip placement (side, flip) and maximum width against real panels at build time.

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the DiaVisualDebugger, DiaEditorUI and DiaDebugServer edits.
