# Feature Spec: DiaValueSchema Code-Side Descriptions

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md (Feature 1), @docs/specs/applications/dia/systems/diavalueschema/export-query-and-reference.md (Feature 6)

**Gameplay Domains:** debug, tools

## Summary

Lets code describe the values it defines: metrics and command arguments. It delivers:

- **`MetricInfo`** (label, description, unit) passed when a metric is registered with `MetricRegistry`;
- **`ValueParam`**, the existing `EditorActionParam` moved down into DiaAPI and extended (unit, range, enum values), carried by DiaAPI `CommandInfo` and `CommandInfoJson`;
- a **collector** in DiaValueSchema that pulls both from their registries into the `ValueSchemaService` once at the end of Init (and in describe mode);
- **argument text** in `HelpSystem` and the debug-panel command palette.

Entity component fields are Feature 7. Everything here is optional: every existing call site keeps compiling.

## Problem

Metrics have only a name and command arguments have no description at all, so charts, the palette and `--help` cannot say what a value is, what its unit is or what an argument means. Editor actions already describe their parameters, in a type that only the editor can see. The same descriptions should be written once, next to the definition, and appear everywhere.

## Goals

1. **Describe where defined.** A metric carries its description at registration; a command carries its argument descriptions beside its callback (VS-003).
2. **One parameter type.** `ValueParam` replaces `EditorActionParam` as the single type for editor actions and DiaAPI commands.
3. **No new dependencies below DiaValueSchema.** DiaObservation and DiaAPI store the infos; DiaValueSchema collects them.
4. **Visible in the tools that exist.** `--help <command>` and the debug-panel palette show argument text.
5. **Measurable coverage.** Undescribed metrics, commands and arguments are listed for the export (Feature 6) and `dia validate schema` (Feature 2).
6. **No breakage.** Existing registrations and the 84 current uses of `EditorActionParam` compile unchanged.

## Non-Goals

- Entity component fields (Feature 7).
- Describing metrics or commands in sidecar files (rejected by VS-003).
- Argument parsing or validation by `ValueParam` (the existing `ArgumentParser` is unchanged).
- A change to how commands execute, or to Python bindings.
- Runtime range clamping of command arguments.

## Architecture

### Types

```cpp
// Dia/DiaAPI/CommandRegistry/ValueParam.h        namespace Dia::API
struct ValueParam
{
    const char* name        = nullptr;   // "kind"
    const char* type        = nullptr;   // "string" | "int" | "float" | "bool" | "enum"
    bool        required    = false;
    const char* description = nullptr;
    // new, all optional
    const char* unit        = nullptr;   // "weeks"
    bool        hasRange    = false;
    float       min         = 0.0f;
    float       max         = 0.0f;
    const char* enumValues  = nullptr;   // "weather|ecology" for type "enum"
};

// Dia/DiaObservation/Metric/MetricInfo.h        namespace Dia::Observation::Metric
struct MetricInfo
{
    const char* label       = nullptr;   // "Herbivores (world)"
    const char* description = nullptr;   // "Total herbivore population across all cells."
    const char* unit        = nullptr;   // "animals"
};
```

- `EditorActionParam` becomes `using EditorActionParam = Dia::API::ValueParam;` in `Dia/DiaEditor/EditorAPI/EditorActionDescriptor.h`. The first four members keep their order, so aggregate initialisation in the 84 existing uses (8 files) still compiles. `EditorActionParamSchema` keeps its 8-slot `DynamicArrayC` of the same type. The editor-action manifest export gains the new fields only when set.
- The metric name and command name text come from `StringCRC::AsChar()`, so neither info repeats its name.
- All strings follow the existing `CommandInfo` rule: string literals or otherwise outliving the registration. The registries store pointers, not copies.

### Registration APIs

```cpp
// MetricRegistry: overloads, the originals stay
Counter*   RegisterCounter  (const StringCRC& name, const MetricInfo& info);
Gauge*     RegisterGauge    (const StringCRC& name, const MetricInfo& info);
Histogram* RegisterHistogram(const StringCRC& name, const float* bounds, unsigned int count, const MetricInfo& info);

// CommandInfo and CommandInfoJson gain
const ValueParam* params     = nullptr;   // caller-owned array, e.g. a static array beside the callback
unsigned int      paramCount = 0;
bool              noArgs     = false;     // true: the command takes no arguments (not a coverage gap)
```

- `CommandInfo` stays small: it is copied by value into 256-entry arrays (`ListCommandInfos`), so the arguments are a pointer and count, not an embedded array.
- Registering a metric again with the same name and a different info logs a Warning and keeps the first; the same name with no info after one with an info keeps the info.
- The registries expose read-only enumeration for the collector: `MetricRegistry` gains `ForEachMetric(callback)` (name, kind, info or none), and the command registry's existing `ListCommandInfos` already returns the infos; a matching list for JSON commands is added if missing.
- Each registry also exposes a `Generation()` counter that increases on every registration. The collector records it, and `ValueSchemaModule` compares it later to detect late registrations (see Resolved Decisions).

### Collector (DiaValueSchema)

`CodeSideCollector` runs in `ValueSchemaModule`:
1. **When:** once after every PU finishes Init (just before the service snapshot is published, Feature 1), and again for the dump in describe mode (Feature 6).
2. **What:** it reads `MetricRegistry` and the command lists and builds `ValueInfo` entries, then registers them with `ValueSchemaRegistry::Register` (the path Feature 1 reserves for code-side entries).
3. **Late registrations:** a metric or command registered after the snapshot is published is not in the service; the next describe-mode dump (which runs after Init on every PU) does include it, and the module logs a Warning naming how many were late.

| Value | `for` | `key` | `kind` | Fields filled |
|---|---|---|---|---|
| Metric | `dia.metric` | the metric name, e.g. `cow.ecology.herbivores_total` | `metric` | label, description, unit; `type` float |
| Command argument | `dia.command` | `<command>.<arg>`, e.g. `cow.save.slot` | `arg` | label (the name), description, type, unit, range, enum values, `required` |
| Command itself | `dia.command` | `<command>`, e.g. `cow.save` | `command` | label (the name), description, owner, category, example, and the ordered list of its argument keys. One entry for **every** registered command, including those with no arguments (`noArgs`), so an editor can list all commands (added 2026-10-20, T-2 Feature 5) |

- A `type` string outside the five known ones maps to `string` with a Warning once per command.
- An enum argument's `enumValues` ("a|b") becomes enum values whose id and label are the text and whose description is empty; no sidecar strictness applies to code-side entries.
- **Undescribed** items, for the export's `undescribed` section: a metric registered without a `MetricInfo` or with an empty `description`; a command with `params == nullptr` and `noArgs == false`; a `ValueParam` with no `description`. Commands that declare `noArgs = true` are never listed.

### Help system and palette

- **`ShowCommandHelp`** prints an "Arguments" section when a command has params:

  ```
  Arguments:
    <kind>   enum    weather|ecology   Which pack to save: weather or ecology.
    [id]     string                    Asset id for the new pack.
    [weeks]  int     weeks, 1..52      How long to run.
  ```
  Required arguments are in angle brackets, optional in square brackets; unit, range and enum values follow the type. A command with no params prints nothing new.
- **Debug-panel palette:** the command results gain an optional `args: [ {name, type, required, description, unit?, min?, max?, values?} ]` array only for commands that have params, and the palette shows the argument text under the command. Results for commands without params are byte-identical to today, so the existing `palette_results_commands.json` golden is unchanged, and a new golden case covers a command with params.
- `ShowGlobalHelp` is unchanged.

### Logging and observation

- **Channel:** `VSC`.
- **Info:** collector summary (metrics, commands and arguments collected, undescribed counts).
- **Warning:** late registrations, unknown `type` string, a same-name metric with a conflicting info.
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on the collection pass.
- **Metrics:** metrics described, metrics undescribed, commands described, arguments described.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. `EditorActionParam` is an alias of `Dia::API::ValueParam`; all existing editor-plugin code and tests compile with no source change beyond the include, and the editor-action manifest output is unchanged for existing actions.
2. `RegisterCounter`, `RegisterGauge` and `RegisterHistogram` accept an optional `MetricInfo`; the original overloads still work; the info is retrievable from the registry.
3. `CommandInfo` and `CommandInfoJson` accept `params`, `paramCount` and `noArgs`; existing registrations compile unchanged, and `sizeof(CommandInfo)` grows by no more than a pointer, an integer and a bool.
4. The collector turns each described metric and each command argument into a `ValueInfo` with the keys, `for` and `kind` above, and registers them before the service snapshot is published.
5. An enum argument's `enumValues` becomes enum values; an unknown type string becomes `string` with one Warning per command.
6. A metric or command registered after the snapshot is not in the service but is in the next describe-mode dump. Each registry's `Generation()` increases on every registration, and when `valueschema.get` is answered or a dump is written after a generation has moved on, the module logs one Warning counting the late registrations.
7. The `undescribed` lists contain metrics without a description, commands without `params` that do not set `noArgs`, and parameters without a description, and omit everything else.
8. `--help <command>` prints the Arguments section as above for commands with params and is unchanged for commands without.
9. The palette command results carry `args` only for commands with params; existing results and the existing golden are unchanged; a new golden case with params passes in the C++ test and the page test.
10. DiaObservation and DiaAPI do not depend on DiaValueSchema (`dia check deps` clean); only DiaValueSchema depends on them.
11. A metric registered twice with conflicting infos keeps the first and logs one Warning.
12. Registration and lookup cost of metrics and commands without infos is unchanged.

## Test Strategy

- **GoogleTests (TDD):**
  - `MetricInfo` round trip through each register overload; duplicate and conflicting registration.
  - `CommandInfo` and `CommandInfoJson` with params, `noArgs`; copy by value; `sizeof` guard.
  - `ValueParam` alias: a compile test that the old `EditorActionParam` aggregate-initialisation forms still build.
  - Collector: keys, `for`, `kind`, types, enum values, unknown type, late registrations, undescribed lists.
  - `HelpSystem` output text for required, optional, unit, range, enum.
  - Palette JSON with and without params; the golden test in `TestPanelContract`.
- **Existing suites:** the editor plugin tests and `DiaAPI` command tests run unchanged.
- **Vitest (panel page tests):** the palette renders `args` and still renders results without them.
- **Test utilities** in `DiaValueSchema/Testing/`: a fake metric and command source for the collector.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-003 | Metrics and command arguments are described in code at registration | `MetricInfo` and `ValueParam` |
| VS-005 | No singleton | The existing `MetricRegistry` singleton is only read; the service is reached through the service stream |
| PD-004 / AD-002 | No STL in public APIs; DiaCore containers | `MetricInfo` and `ValueParam` are plain structs; `CommandInfo` uses a pointer and count |
| AD-001 (Dia) | Edits to DiaObservation, DiaAPI and DiaEditor need sign-off | All three flagged below |
| Panel contract golden | Panel JSON and page share golden files | New palette case added; existing golden unchanged |
| Layering | A lower module must not depend on a higher one | DiaValueSchema pulls; the registries never call it |

No conflicts. This replaces the system spec's "registries push their infos into the service" with the pull collector (amended in the system spec).

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | `ValueSchemaRegistry::Register`, module start sequence | Approved |
| Feature 6 | Describe mode and the `undescribed` section | Approved |
| DiaObservation (`MetricRegistry`) | `MetricInfo`, enumeration | Done (extended; sign-off needed) |
| DiaAPI (`CommandRegistry`, `HelpSystem`) | `ValueParam`, `CommandInfo` fields, help text | Done (extended; sign-off needed) |
| DiaEditor (`EditorActionDescriptor`) | Alias to `ValueParam` | Done (extended; sign-off needed) |
| DiaVisualDebugger palette | `args` in results | Done (extended) |

## Resolved Decisions

1. **Warning volume is handled in `dia validate schema`** (2026-10-20). Existing commands will almost all lack params and `noArgs`, so a flat warning list would be unreadable. Feature 2 now groups code-side coverage warnings by owner with counts ("DiaAssets: 41 commands without argument descriptions") and prints the full list only with `--verbose` (Feature 2 amended). Nothing needs to change in this feature: the `undescribed` section already carries the owner, taken from `CommandInfo.owner`; metrics use the first dotted segment of the name as the owner.
2. **Late registrations: warn, don't republish** (2026-10-20). The service snapshot stays immutable (Feature 1). To avoid silent gaps without polling, `MetricRegistry` and the command registry each expose a `Generation()` counter that increases on every registration. `ValueSchemaModule` records the generations it collected at, and compares them when it answers `valueschema.get` and when it writes a describe-mode dump; if either has moved on it logs one Warning ("N metrics/commands were registered after the schema was published and are not in the live schema"). Describe mode still includes them, so exports and the reference page are complete. Republishing is added only if a real late-registering system needs live tooltips.
3. **Units get a light lint, not a check** (2026-10-20). Units stay free text. Feature 2 warns (never fails) when a metric name ends in a known unit suffix (`_ms`, `_us`, `_bytes`, `_kb`, `_mb`, `_hz`, `_pct`) and its `MetricInfo.unit` is empty. It does not compare unit text with the suffix, because "milliseconds", "ms" and "msec" are all legitimate (Feature 2 amended).

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the DiaObservation, DiaAPI and DiaEditor edits.
