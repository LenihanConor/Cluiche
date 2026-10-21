# Feature Spec: DiaValueSchema Format, Loader and Service

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md

**Gameplay Domains:** debug, tools

## Summary

The foundation of DiaValueSchema. It defines the `dia.value_schema` sidecar file format (including record paths, `unique`, `required`, `ref`, the `event` kind and enum colours), the in-memory `ValueInfo`, the loader that reads every `*.schema.json` reachable from the app's path aliases, and the `ValueSchemaModule` / `ValueSchemaService` that answer `Find(fullKey)` and `ForFormat(formatId)`. It also provides the clamp-and-warn helper that loaders use for out-of-range values. Everything else in DiaValueSchema (validation, tooltips, code-side descriptions, ramps, export) reads through this.

## Problem

Nothing describes values today: panel controls carry only `{key, label, min, max, step, value}`, metrics have only a name, and entity fields have ranges but no explanations. Every later feature, and DiaCellLayers, DiaCellHistory, T-1 and T-2, needs one registry to ask "what is this value?". This feature builds that registry and the file format that fills it, with no UI and no CLI.

## Goals

1. **One format.** `dia.value_schema` v1, with the entry keys in the system spec plus the additions below.
2. **One registry.** A `ValueSchemaRegistry` of `ValueInfo` keyed by full key `<for>/<key>` (StringCRC), also indexed by `for` format.
3. **One service.** `ValueSchemaService` (a `ServiceStream`, no singleton) answers `Find` and `ForFormat`.
4. **Resilient loading.** One bad file never blocks the app or hides the other schemas.
5. **Open for later features.** Feature 4 (metrics, command args) and Feature 7 (component fields) register entries through the same registry API.
6. **Clamp-and-warn helper.** A shared helper clamps a value to `min`/`max` and logs one Warning per key.

## Non-Goals

- `dia validate schema` and its fail/warn rules (Feature 2). The loader reports structural problems; semantic checks (coverage, defaults match, refs resolve) are Feature 2's.
- Tooltips and UI (Feature 3), `MetricInfo` / `ValueParam` (Feature 4), ramps (Feature 5), export/query/reference (Feature 6), component fields (Feature 7).
- Runtime reload of schema files in v1 (see Open Q1).
- Localisation, unit conversion.

## Architecture

New engine module `Dia/DiaValueSchema/` (AD-001, namespace `Dia::ValueSchema`, AD-003), created with `dia scaffold module`, with a `dia.*.architecture.module.md` file. It depends only on DiaCore (Json, StringCRC, containers, `PathStore`) and DiaApplicationFlow (`ServiceStream`).

```
Dia::ValueSchema
  ValueInfo              one described value (below)
  ValueSchemaLoader      file text → entries (+ per-file result)
  ValueSchemaRegistry    fullKey → ValueInfo ; format → list ; Register / Find / ForFormat
  ValueSchemaModule      discovers *.schema.json via path aliases at start; owns the registry
  ValueSchemaService     ServiceStream over the loaded registry (read-only snapshot)
  ClampAndWarn           value, ValueInfo → clamped value (+ one Warning per key, channel VSC)
```

No STL in public APIs (PD-004); `ValueInfo` lists use DiaCore containers.

### Format additions (over the system spec)

**Entry keys** keep the system spec's table. This feature adds:

| Addition | Form | Meaning |
|---|---|---|
| `kind: event` | event type, e.g. `cow.drought`, `cow.ecology.cause` | Describes a DiaCellHistory event type |
| `payload` | `[{ "name", "type", "description", "unit"? }]` | Fields an event carries (for example `group`, `layer`, `cause`, `delta`, `cells`) |
| `vocabulary` | `[{ "id", "label", "description", "unit"? }]` | Named values a payload field draws from (for example cause ids) |
| `ui.colour`, `ui.glyph` | colour string, short glyph name | How the event is drawn (T-1 markers) |
| Enum value `colour` | `values: [{id, label, description, colour?}]` | Optional palette colour per enum value |
| `type: "map"` | `keyRef: "<format>/<record path>"`, `valueType: float \| int \| bool \| string \| enum`, plus the usual `min`, `max`, `unit` for the values | An object whose keys must be ids from another format's records and whose values have one type and range (for example a species' biome `affinity`). Added 2026-10-20 for T-2 Feature 2 |

- **Enum colours.** A value without a `colour` is assigned from the `categorical.12` ramp by value order, so a category layer always has a palette. (The ramp library itself is Feature 5; until then the loader uses a built-in 12-colour list.)
- **`ui` and `history` blocks are open.** The loader keeps any keys they contain (stored as JSON values), so other specs can add keys (`hotkey`, `group`, `delta_ramp`, `track`, `history.group`) without a loader change. Unknown keys are logged once as a Warning on channel `VSC`, naming the file and key.
- **Core keys are strict.** A missing required key (`key`, `kind`, `label`, `description`, `type`), a wrong type, an unknown `kind` or `type`, or a bad `format`/`version` fails that file.

### `ValueInfo`

Holds, per entry: full key, `for`, `key`, `kind`, `label`, `description`, `hint`, `unit`, `type`, `min`, `max`, `default`, enum `values`, `affects`, `related`, `source`, `unique`, `required`, `ref`, record-path segments, `ui` (open), `history` (open), event `payload` and `vocabulary`. Lists are DiaCore containers.

- **Full key:** `<for>/<key>`, hashed with StringCRC (PD-001). Record paths such as `species[].growth_rate` are stored as written and also split into path segments (`species`, `[]`, `growth_rate`) for lookup by record column.
- **Hash collisions:** two different full keys with the same hash are detected at registration and logged as an Error; the second is rejected.

### Loading

`ValueSchemaModule` at start:
1. Enumerates `*.schema.json` under the app's path aliases (the same alias roots the app uses for assets).
2. Passes each file to `ValueSchemaLoader`, which returns the file's entries and a result (OK, or the failure with file, entry and reason).
3. Registers the entries of every OK file into the registry.
4. Publishes the finished registry to `ValueSchemaService` as a read-only snapshot.

Failure handling:
- **A bad file** is skipped and an Error is logged (file, entry, reason); the rest load.
- **A duplicate full key**: the first loaded wins, and an Error names both files. Load order is the sorted file path, so it is deterministic.
- Both are fatal only in `dia validate schema` (Feature 2) and CI.

### Service

```cpp
class ValueSchemaService {              // ServiceStream-published, read-only
public:
    const ValueInfo*  Find(StringCRC fullKey) const;
    const ValueInfo*  Find(StringCRC forFormat, StringCRC key) const;
    ValueInfoSpan     ForFormat(StringCRC formatId) const;   // entries in file order
    const ValueInfo*  FindEvent(StringCRC eventKey) const;   // `kind: event` entry by its key, in any `for`
    uint32            Count() const;
};
```

- Consumers on other PUs read through the stream; the registry is immutable once published (v1), so reads need no lock.
- Feature 4 and Feature 7 register extra entries (metrics, command args, component fields) during module start through `ValueSchemaRegistry::Register`, before the snapshot is published.

### Clamp-and-warn

`ClampAndWarn(value, info)` returns the value clamped to `min`/`max` (numeric types only) and logs one Warning per key on channel `VSC` the first time it clamps ("`grazing.rate` = 2.5 outside [0, 2], clamped"). Non-numeric and range-less entries pass through. Counts of clamps are exposed as a metric.

### Logging and observation (all five pillars)

- **Logs:** channel `VSC`: Info on load summary (files, entries), Warning for unknown `ui`/`history` keys and clamps, Error for bad files, duplicate keys and hash collisions.
- **Traces:** `DIA_TRACE_ZONE` around module start load.
- **Profiling:** `DIA_PROFILE_SCOPE` on file load.
- **Metrics:** entries loaded, files failed, clamp count.
- **Health:** `IHealthReporter` degraded when any file failed to load.

## Acceptance Criteria

1. A valid sidecar loads into `ValueInfo` entries with every system-spec key present, including record paths, `unique`, `required` and `ref`.
2. `Find` returns an entry by full key and by `(for, key)`; `ForFormat` returns that format's entries in file order; an unknown key returns null.
3. A file with a missing required key, a wrong type, an unknown `kind` or `type`, or a bad `format`/`version` is rejected as a whole with an Error naming file, entry and reason; other files still load.
4. A duplicate full key across files: the first (sorted by path) wins and an Error names both; a hash collision between different keys is rejected with an Error.
5. An `event` entry loads with `payload`, `vocabulary` and `ui.colour`/`glyph`. `FindEvent` returns an event entry by its `key` in any `for`; two `kind: event` entries with the same `key` in different `for` formats are rejected as a duplicate (first wins, Error names both). Amended 2026-10-20 for DiaCellHistory Export, which resolves event types by key.
6. Enum values keep `colour` when given and get a colour from the built-in categorical list by order when not.
7. Unknown keys inside `ui` and `history` are kept and readable by consumers, and logged once as a Warning; unknown top-level entry keys fail the file.
8. `ClampAndWarn` clamps numeric values to `min`/`max` and logs exactly one Warning per key regardless of how often it clamps; it passes through values for entries without a range.
9. The registry accepts extra entries from code before publishing (the Feature 4 and 7 path), and rejects registration after publishing with an Error.
10. The published service is readable from another PU without locking.
11. A file that fails to load degrades the module's health reporter; a clean load reports healthy.
12. The module has a module doc, appears in `dia check deps` without violations, and the public headers use no STL (PD-004).

## Test Strategy

- **GoogleTests (TDD):**
  - Loader: each key, record paths, `unique`/`required`/`ref`, enum values, `event` payload and vocabulary.
  - Loader rejections: one test per strict failure in AC 3.
  - Open blocks: unknown `ui`/`history` key kept and warned; unknown core key rejected.
  - Registry: lookup by full key and by format, collision, duplicate, post-publish rejection.
  - Module: discovers fixtures under a test alias; bad file skipped, good file loaded; deterministic order.
  - `ClampAndWarn`: in/out of range, one warning per key, non-numeric pass-through.
  - Service: read from a second PU thread under a stress test (no locks, no data race under ASan/TSan where available).
- **Test utilities** live with the module under `DiaValueSchema/Testing/` (fixture builder for schema JSON, an in-memory registry), per the testing rule.
- **Shared conformance fixtures (amended 2026-10-20, Feature 2):** `DiaValueSchema/Testing/Conformance/` holds good and bad schema files with an `expected.json` verdict each. These GoogleTests load every file there and must match `expected.json`; `dia validate schema` (Feature 2) runs the same set in pytest, so the C++ loader and the Python validator cannot drift. A new format rule must add a fixture.
- **Fixtures:** a sidecar for a parameter file, a fields file with a category layer, an event type with vocabulary, and a deliberately bad file for each rejection.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| PD-001 | Full keys and format ids are StringCRC | Full key `<for>/<key>` hashed with StringCRC; collisions detected |
| PD-004 / AD-002 | No STL in public APIs; DiaCore containers | `ValueInfo` and the service API use DiaCore containers |
| AD-001 (Dia) | New module has a module doc | `dia.*.architecture.module.md` created at scaffold |
| AD-003 (Dia) | Namespace | `Dia::ValueSchema` |
| AD-004 (Dia) | `ValueSchemaModule` is a normal module; the service is a `ServiceStream` | As designed |
| VS-002 | Parameters and fields in sidecar files keyed by the data file's `format` | `for` names the format id |
| VS-004 | Runtime out-of-range values clamp with a Warning | `ClampAndWarn` |
| VS-005 | No singleton | Lookup only through the service |

New Dia module: placement is the engine, and the system spec already approves the module (AD-001), so no further placement sign-off is needed here. Edits to shared build files (`.sln`, module registry) follow the normal scaffold flow.

No conflicts.

## Open Design Questions

1. **Runtime reload.** A1 has `cow.ecology.reload_data` and T-2 has "Reload in game", which re-read data files. Schemas are published as an immutable snapshot, so a reload would swap the whole snapshot. Decide in Feature 3 or when T-2 needs it whether v1 needs a `Reload()` (re-run the load, publish a new snapshot, consumers re-read), or whether restarting the app after a schema edit is acceptable.
2. **Memory and start-up cost.** Hundreds of entries with descriptions are small, but F-3's many formats could reach thousands. Measure load time and memory when CoW's schemas exist, and add an on-demand per-format load if the start-up cost is visible.
3. **Hash-collision policy.** Rejecting the second key is safe but surprising. If a real collision ever happens, a longer key hash may be needed; unlikely for CRC32 at this scale, but the Error tells us immediately.

## Status

`Approved` (2026-10-20). Plan: [format-loader-and-service.plan.md](format-loader-and-service.plan.md).
