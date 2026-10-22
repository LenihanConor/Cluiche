# Feature Spec: DiaValueSchema Export, Query and Reference

**Parent:** @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md
**Builds on:** @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md (Feature 1), @docs/specs/applications/dia/systems/diavalueschema/tooltips.md (Feature 3, defines `valueschema.get`), @docs/specs/applications/dia/systems/diavalueschema/ramps.md (Feature 5)

**Gameplay Domains:** tools, debug

## Summary

Gets the merged schema out of the running app and into files. It delivers:

- **`dia schema export --target <t>`**: writes `Cluiche/out/<App>/valueschema.json`, merging the sidecars with the code-side descriptions (metrics, command arguments, component fields) and listing what is undescribed.
- **Describe mode**: `--describe-schema <path>` makes the app write that file after its modules have started, then exit.
- **A schema subset helper** that DiaCellHistory exports use to embed the entries and ramps a file needs, so offline files explain themselves.
- **`dia docs value-reference --target <t>`**: generates a committed reference page, with a precommit freshness check.
- **Extensions to `valueschema.get`** (defined in Feature 3): an exact-key filter and an `origin` field.

## Problem

Three consumers need the schema outside the running game's debug server:

1. `dia validate schema` must check code-side coverage without building or running anything.
2. DiaCellHistory files and T-1's offline mode must carry their own labels, units and ramps.
3. People need a readable list of every value, unit, range and default to review, without opening JSON.

Code-side descriptions only exist in the process memory of the running app, so they have to be dumped from it.

## Goals

1. **One export file.** Sidecar entries and code-side entries in one JSON file, each marked with its origin.
2. **Honest coverage data.** The export lists the metrics, commands and component fields that have *no* description, so the validator can warn.
3. **Headless and scriptable.** Describe mode needs no window, no game loop and no connection.
4. **Not misleading when old.** The export records the commit it was made from; the validator warns when it differs from HEAD.
5. **Self-describing files.** A subset helper embeds exactly the entries and ramps a DiaCellHistory file references.
6. **Readable reference.** A generated, committed page lists every value, with a freshness check.

## Non-Goals

- Authoring or editing schema text (that stays in sidecars and code).
- A live-updating schema; the export is a snapshot.
- Localised or multi-version references.
- HTML or other reference formats; markdown only.
- Running the app on CI machines without a graphics stack beyond what `--automation` already needs.

## Architecture

### Export file (`dia.value_schema_export` v1)

`Cluiche/out/<App>/valueschema.json` (PD-009):

```json
{
  "format": "dia.value_schema_export", "version": 1,
  "header": { "app": "cow", "config": "Debug", "git_commit": "8f88d1aa", "dirty": false, "generated_at": "2026-10-20T14:03:00Z" },
  "entries": [ { "key": "cow.ecology_params/grazing.rate", "origin": "sidecar:Global/Ecology/ecology_parameters.schema.json", "...": "all sidecar fields" },
               { "key": "cow.ecology.herbivores_total", "kind": "metric", "origin": "code:metric", "label": "...", "description": "...", "unit": "animals" } ],
  "ramps": [ { "id": "sequential.green", "...": "..." } ],
  "undescribed": { "metrics": [{ "name": "cow.weather.ticks", "owner": "cow" }],
                   "commands": [{ "name": "cow.save", "owner": "CoW", "args": ["slot"] }],
                   "component_fields": [{ "name": "Transform2D/scale", "owner": "Transform2D" }] }
}
```

- `origin` is `sidecar:<path>` or `code:metric | code:command | code:arg | code:component_field`.
- `undescribed` lists metrics registered without `MetricInfo`, command arguments without `ValueParam` text, and component fields that have `DIA_ATTR_RANGE` but no `DIA_ATTR_DESCRIBE`. This is what lets the validator warn without running the app.
- `header.git_commit` and `dirty` come from the repository at export time.

**Undescribed item shape (amended 2026-10-22, closes the Feature 2 / Feature 4 disagreement).** Every `undescribed` list holds objects, never bare strings (the validator still reads strings for old files):

| List | Item | Meaning |
|---|---|---|
| `metrics` | `{ name, owner }` | a metric with no `MetricInfo` or an empty description; `owner` is the first dotted segment of the name |
| `commands` | `{ name, owner, args }` | a command with no `ValueParam`s and `noArgs` not set (`args` is empty), or one whose parameters lack text (`args` lists those parameter names); `owner` is the `CommandInfo` owner |
| `component_fields` | `{ name, owner }` | `name` is `<type id>/<field>`, `owner` the type id; a field with `DIA_ATTR_RANGE` and no `DIA_ATTR_DESCRIBE` |

A command registered with `noArgs: true` is described by definition: it is never listed in `undescribed.commands`. Its `entries` item (`kind: command`, `origin: code:command`) carries `"ui": { "no_args": true }` so readers can tell "takes no arguments" from "arguments not described".

### Describe mode (C++)

- The app accepts `--describe-schema <path>`. `ValueSchemaModule` waits until every processing unit has finished its Init phase (all registrations are in), writes the export, and requests a clean exit with code 0. On failure it exits with a non-zero code and a message.
- It writes the sidecar entries, the code-side entries, the ramps and the `undescribed` lists from the same registry the live service holds, so the file is exactly what the running app would serve.
- No window, no rendering and no debug server are needed; the flag is meant to be combined with `--automation`.

### `dia schema export` (Python)

```
dia schema export --target <t> [--stage <name>] [--out PATH] [--sidecars-only]
```

- Default: launch the target through `dia launch` (never the executable directly) with `--describe-schema`, wait for it to finish, and check the file exists and parses.
- **`--stage <name>`** (added 2026-10-20, T-2 Feature 5): commands registered by a stage's modules exist only while that stage runs, so describe mode waits until the named stage has finished Init (the app enters it through the same automation path the E2E tests use) before it dumps, and the file is written as `valueschema.<name>.json` beside the base export. The scenario editor reads the export for the stage folder a scenario lives in and falls back to the base export.
- **`--sidecars-only`:** build the export in Python from the sidecars and ramps alone (reusing Feature 2's discovery). No launch, no build; `undescribed` is omitted and the header says `"code_side": false`. The CLI prints a one-line notice ("code-side coverage omitted: --sidecars-only") with the summary, and `dia validate schema` treats an export with `code_side: false` as having no code-side data: it prints its existing "code-side checks skipped" notice instead of passing silently (amended 2026-10-22).
- **Locating an export without `--target`** (amended 2026-10-22; this is what `dia validate schema` already does): the only `valueschema.json` under `Cluiche/out/<App>/`. With none, the validator prints its skipped notice; with several, it prints the notice naming the candidates and asks for `--target`. `dia schema export` itself always requires `--target`.
- Fills `header.git_commit` and `dirty` from git; writes to `Cluiche/out/<App>/valueschema.json` unless `--out` is given.

### Staleness

`dia validate schema` (Feature 2) reads `header.git_commit` and, if it differs from the current HEAD or `dirty` is true, prints one WARN ("export is from `<commit>`, HEAD is `<commit>`") and still uses the data. No timestamps are compared.

### Schema subset helper (C++)

```cpp
bool ValueSchemaService::ExportSubset(const StringCRC* keys, uint32 keyCount, Json::Value& out) const;
```

- Writes `{ entries: [...], ramps: [...] }` for the given keys, plus every ramp their `ui.ramp` / `ui.delta_ramp` names, plus the entries those entries reference through `affects` / `related`.
- Used by DiaCellHistory's `cells.history.export` to embed the schema for every recorded layer and event type. An unknown key is skipped with one Warning (`VSC`).
- The embedded block uses the same field names as the export file, so T-1's file source and the live query parse one format.

### `valueschema.get` extensions

Feature 3 defined `valueschema.get {prefix?}`. This feature adds:
- `keys: [...]`: return exactly these full keys (used by clients that know what they need, such as T-1 loading a layer list).
- an `origin` field on each entry.

### Reference page

```
dia docs value-reference --target <t> [--check]
```

- Output: `docs/reference/<app>/values.md`, committed.
- **Content:** a "Generated by `dia docs value-reference`, do not edit" header with the export's commit; one table per `for` format (key, label, type, unit, range, default, description, affects); a metrics table; a commands table with their arguments; a component fields table. Code-side tables appear only when an export exists; the page says so otherwise.
- **`--check`:** regenerates in memory and exits 1 if the committed file differs.
- **Precommit:** `dia docs precommit` runs the check when any `*.schema.json`, `*.ramps.json` or the reference page itself changed, and reports "reference out of date" as a WARN (not a block).

### Logging and observation

- **Logs:** `VSC` Info on export written (entries, undescribed counts); Error on a failed write.
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` around the dump.
- **Metrics:** entries exported, undescribed items.
- **Health:** none beyond Feature 1's.
- CLI prints a one-line summary (entries, undescribed counts, path).

## Acceptance Criteria

1. Launched with `--describe-schema <path>`, the app writes a valid `dia.value_schema_export` after all PUs finish Init and exits with code 0, with no window and no debug server.
2. The export contains sidecar entries and code-side entries, each with an `origin`, plus the ramps and the `undescribed` lists.
3. `undescribed` lists a metric registered without `MetricInfo`, a command argument without a description and a ranged component field without `DIA_ATTR_DESCRIBE`, and omits described ones.
4. `dia schema export --target <t>` launches through `dia launch` with the flag and fails with a clear message if the file is missing or invalid.
5. `--sidecars-only` writes an export from sidecars and ramps with no launch, omits `undescribed`, and marks `"code_side": false`.
6. The header records the app, config, `git_commit`, `dirty` and `generated_at`.
7. `dia validate schema` warns when the export's commit differs from HEAD or is dirty, and still uses it.
8. `ExportSubset` returns the requested entries, the ramps they use and the entries they reference; an unknown key is skipped with one Warning.
9. `cells.history.export` embeds the subset, and a file loaded with only its embedded schema shows the same labels, units and ramps as the live app.
10. `valueschema.get` accepts an exact-key filter and returns `origin` on each entry.
11. `dia docs value-reference` writes `docs/reference/<app>/values.md` with the tables described; code-side tables appear only when an export exists.
12. `--check` exits 1 when the committed page differs from a fresh generation; precommit reports that as a WARN.
13. The export file is written under `Cluiche/out/<App>/`, and nothing is written outside it except the reference page.

## Test Strategy

- **GoogleTests:** export content from a fixture registry (sidecar, metric, arg and component entries; each `origin`); `undescribed` for described and undescribed items; `ExportSubset` (keys, ramps, references, unknown key); `valueschema.get` filters and `origin`; describe-mode write on a minimal test app.
- **pytest:** `dia schema export` with a stub launcher; `--sidecars-only`; header git fields (mocked git); staleness WARN in `dia validate schema`; reference page generation against a fixture export, the no-export case, `--check` and the precommit warning.
- **Integration:** run describe mode on CluicheTest headless and confirm the export parses and lists known metrics.
- **Round trip:** a DiaCellHistory export fixture with an embedded subset loads in T-1's file source model and renders labels without the app.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-003 | Metrics and command args are described in code | Export reads code-side descriptions from the running registry; no sidecar fallback |
| VS-004 | Strict coverage | `undescribed` feeds Feature 2's coverage warnings |
| VS-005 | No singleton | Describe mode and `ExportSubset` use the service |
| PD-009 | Generated output under `Cluiche/out/<App>/` | Export goes there; only the reference page is committed |
| DiaCLI rule | Never run executables directly | Export launches through `dia launch` |
| CH-004 (CoW) | Live and file data share one format | Embedded subset uses the live query's entry format |
| Resolved Decision 1 (system spec) | Headless describe mode; validate never needs a build | Implemented as written; `--sidecars-only` needs no launch at all |
| AD-001 (Dia) | Dia startup and CLI edits need sign-off | The `--describe-schema` flag touches Dia startup and the DiaCellHistory export; both flagged for sign-off before build |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Registry and service | Approved |
| Feature 3 | `valueschema.get` | Approved |
| Feature 5 | Ramps in the export | Approved |
| Features 4 and 7 | Code-side entries and the `undescribed` source data | Draft (TBD); the export works with whatever is registered |
| DiaCellHistory Feature 5 (export) | Calls `ExportSubset` | Approved, not built |
| `dia launch`, `dia docs precommit` | Hosting | Done (extended) |
| Dia app startup (`--automation` handling) | `--describe-schema` flag | Done (extended; sign-off needed) |

## Open Design Questions

1. **When the dump runs across PUs.** Registrations happen as each PU starts its modules. Decide in the plan whether "after Init on every PU" is a barrier the application already has, or whether `ValueSchemaModule` needs a short wait on the main PU.
2. **Startup flag placement.** Where the `--describe-schema` argument is parsed (the Dia application base, or each app's `Main`) is a Dia startup change. Check against how `--automation` is handled and follow it.
3. **Reference page size.** One page for a large app may get long. If it does, split by format into a folder with an index; the generator can do this without changing the export.

## Status

`Approved` (2026-10-20); amended 2026-10-22 (undescribed item shape, `noArgs`, `--sidecars-only` notice, export lookup). Plan: [export-query-and-reference.plan.md](export-query-and-reference.plan.md). AD-001 sign-off for the `--describe-schema` startup flag with `--stage` was given 2026-10-21. AC 9 (`cells.history.export` embeds the subset) needs DiaCellHistory Feature 5, which is not built: this feature delivers `ExportSubset` and its tests, and AC 9 closes with DiaCellHistory.
