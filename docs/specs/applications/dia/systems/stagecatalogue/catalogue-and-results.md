# Feature Spec: Catalogue, Tags and Results

**Parent:** @docs/specs/applications/dia/systems/stagecatalogue/stagecatalogue.md (Feature 1)
**Status:** Approved (2026-11-18)

## Summary

Stages get optional `tags` and a `description` in their `.diastage`, validated against a fixed kind vocabulary (`showcase`, `tool`, `test`). DiaGame gains a pure `StageCatalogue` (C++ and Python, same fixtures) built from the `.diagame`. It also gains a `StageCatalogueModule` on MainPU, which:

- keeps the **last result per stage** in `Cluiche/out/<App>/stage_results.json`,
- takes results over a stream,
- publishes a snapshot stream for the menu,
- answers `stages.list`, `stages.get` and `stages.results`.

`dia stages` reads the same files from the CLI. CluicheTest and CoW report results through the new event, and every existing stage is tagged.

This feature does **not** change the menu. That is Feature 4, which is gated on a mockup. The current menu and `StageStatusModule` keep working until Feature 4 replaces them.

## Goals

- Every consumer reads one stage model: game, CLI, DebugServer clients and docs.
- A stage's result survives the process, so `dia run e2e` and a menu run show the same result.
- CoW reports stage results for the first time.
- Nothing breaks for stages with no new fields.

## Acceptance Criteria

### Format and kind vocabulary (DiaGame, DiaCLI)

1. `DiaStageManifest` gains:
   - `tags`: `DynamicArrayC<String64, 16>`
   - `description`: `String128`

   `JsonDiaStageSerializer` loads and saves both. Absent fields load as empty, and a save writes them only when non-empty, so an existing file round-trips byte-identical in content. A field of the wrong type fails with a message that names the field.
2. `Dia/DiaGame/StageKinds.h` defines `StageKind { kShowcase, kTool, kTest }`, `kStageKindCount`, `KindFromTag(StringCRC) -> optional kind` and a kind bitmask type. `Dia/DiaGame/stage_kinds.json` lists the same three strings. A GoogleTest asserts that the JSON and the C++ table match.
3. `dia validate manifest` checks each `.diastage`:
   - `tags` is an array of unique strings, each 1-63 characters and lowercase kebab-case.
   - A tag that is not a kind but matches one case-insensitively, by plural or within edit distance 1 (`Tool`, `tools`, `tset`) is an **error** that names the intended kind.
   - At most 16 tags.
   - `description` is a string of at most 120 characters.
   - The validator reads the vocabulary from `stage_kinds.json`.

### Catalogue model (CluicheGameBaseline, pure)

4. `Cluiche::AppFlow::StageCatalogueEntry` holds the name (`StringCRC`), tags (`StringCRC`, ≤ 16), the kind bitmask, the description and the `.diastage` path. `StageCatalogue` holds ≤ `kMaxStages = 64` entries.
5. `StageCatalogueLoader::Load(const char* diagamePath, StageCatalogue& out)` returns `Ok | MissingDiagame | Malformed | NoRootManifest`.
   - **Order** follows Boot's `transitions` in the root `.diaapp`.
   - A transition with no stage import is still listed, untagged. That matches today's menu.
   - A stage import missing from Boot's transitions is omitted and counted (`omittedCount`).
   - Going over 64 drops the extra entries and counts them (`droppedCount`).
6. Queries:
   - `Find(StringCRC)`
   - `Kinds(entry)`
   - `Filter(const StringCRC* tags, n)`: all tags must match.
   - `GroupByKind(StageCatalogueRows&)`: rows `{group, entryIndex}` in group order showcase, tool, test, other. One row per kind the entry has. Entries with no kind (untagged, or free-form tags only) get one `other` row.
7. Python `dia_cli/utils/stage_catalogue.py` does the same as AC 5-6. Both implementations run against the same fixture set (`Cluiche/Tests/Fixtures/StageCatalogue/`), and their outputs must be equal: order, rows, omitted and dropped counts.

### Results (CluicheGameBaseline)

8. `StageResult { Severity {kNone, kOk, kWarn, kFail}; String16 label; uint64 utcMs; String64 sessionId; String256 logPath; }`. `StageResultEvent { StringCRC stage; StageResult result; }`.
9. `StageResultsFile::Read(path, StageResultsTable&)` returns `Ok | Missing | Malformed | WrongFormat | UnsupportedVersion`. Any result except `Ok` leaves the table empty.
10. `StageResultsFile::Write(path, const StageResultsTable&)`:
    1. Reads the file currently on disk.
    2. Keeps, per stage, the entry with the newer `utcMs`.
    3. Writes `<path>.tmp` and renames it over `path`.

    Two processes writing at once therefore lose only a same-stage race, where last write wins (user decision 2026-11-18). File format: `{format: "dia.stage_results", version: 1, results: {<stage name>: {severity, label, utc, session_id, log_path}}}`.

### Module (CluicheGameBaseline `StageCatalogueModule`, MainPU)

11. Declared per app in `.diaapp` (user decision 2026-11-18). Config:
    - `diagame_path` (required; same convention as `DebugServerHostModule`)
    - `results_path` (optional)

    By default `results_path` is the parent of the observation session's `sessions/` folder plus `stage_results.json`, which gives `Cluiche/out/<App>/stage_results.json`. The session folder comes from `ObservationSessionService`. With neither the config value nor the session service, the module runs without persistence and logs one Warning.
12. On `DoStart` the module loads the catalogue and reads the results file. A load error is logged, and the module runs with an empty catalogue. It never fails the app.
13. It reads the `StageResult` EventStream (SimPU → MainPU). It applies each event to the table, adds the session id and log path from the observation session when the reporter left them empty, bumps the revision, and writes the file **at most once per frame**.
14. It publishes `StageCatalogueSnapshot` on a single-writer FrameStream (MainPU → RenderPU):
    - every entry,
    - its kinds, tags and description,
    - its last result **without `logPath`** (resolved 2026-11-18, Open Design Question 1: the menu does not need it; `stages.results` and the file still carry it),
    - the revision.

    It publishes only when the revision changes, plus once at start. A `static_assert` keeps `sizeof(StageCatalogueSnapshot)` ≤ 48 KB.
15. Consuming `DebugServerService` (precedent: `ValueSchemaModule`), it registers three queries with help text:
    - `stages.list {tag?: [..], kind?}`
    - `stages.get {stage}`
    - `stages.results {stage?}`

    Replies are JSON. An unknown stage replies with `{error}`. When `DebugServerService` is absent (Release), the module runs without the queries.
16. **Observation:**
    - `STAGES` log channel: catalogue loaded (count, omitted, dropped), results read status, result received (stage, severity), write failure.
    - Metrics: `stages.catalogue.count` gauge, `stages.results.write_fail` counter.
    - An `IHealthReporter` that is Degraded while the last write failed.

### Reporters and manifests

17. `cow_main.diaapp` and `cluiche_main.diaapp` declare:
    - `StageCatalogueModule` on MainPU,
    - the `StageResult` EventStream (SimPU → MainPU, drop-oldest, capacity 16),
    - the `StageCatalogueSnapshot` FrameStream.

    `dia validate manifest` passes on both.
18. **CluicheTest:** a new `TestResultReporterModule` (SimPU, `all` stages) polls `TestResultsRegistry` on the thread that writes it. When a stage's state changes, it sends a `StageResultEvent`:
    - Passed → ok/PASS
    - Failed → fail/FAIL
    - Timeout → fail/TIMEOUT
    - Running → none/RUNNING

    `TestStatusBridgeModule` and `StageStatusModule` stay unchanged until Feature 4.
19. **CoW:** `F2TestRunnerModule` sends one `StageResultEvent` when its run completes: ok/PASS when every check passed, otherwise fail/FAIL.
20. **Tags:**
    - Every CoW stage is tagged: `FrontEnd` → `showcase`; `F2CoreRuntime` → `test`; `S1WorldGenerationTestbedStage` → `tool, worldgen`.
    - Every CluicheTest stage is tagged `test` plus one domain tag.
    - Each stage gets a description.

### CLI and docs

21. `dia stages list [--game <name>] [--tag T]... [--kind K] [--json]`, `dia stages get <stage> [--game]` and `dia stages results [--game]` read the files only (the catalogue and `stage_results.json`); live data is `dia query stages.list`. The game resolves like `dia run` targets. The default output is a table; `--json` matches the `stages.*` reply shapes.
22. `dia docs stage-registry` writes `docs/reference/registry/stage-registry.md`, one table per game: stage, kinds, tags, description.

### Compatibility

23. A `.diastage` with no new fields loads, validates and appears as today. A game without `StageCatalogueModule` runs unchanged.
24. `ManifestComposerV2` ignores the new fields; runtime construction does not change (PD-010).

## Files Touched

- **DiaGame (format only):** `DiaGameManifest.h`, `JsonDiaStageSerializer.*`, new `StageKinds.h`, `stage_kinds.json`, `dia.game.architecture.module.md`, vcxproj and filters.
- **CluicheGameBaseline (Modules/ and Types/):** `StageCatalogue.*`, `StageCatalogueLoader.*`, `StageResult.h`, `StageResultsFile.*`, `StageCatalogueModule.*`, `StageCatalogueSnapshot.h`, vcxproj and filters.
- **DiaCLI:** `cli/cli_validate.py`, new `cli/stages.py`, `utils/stage_catalogue.py`, `commands/docs/stage_registry_cmd.py`, `cli/docs.py`, tests.
- **CluicheTest:** new `Modules/TestResultReporterModule.*`, `cluiche_main.diaapp`, every `.diastage`.
- **CoW:** `Modules/F2TestRunnerModule.cpp`, `cow_main.diaapp`, three `.diastage`.
- **Tests:** `Cluiche/Tests/GoogleTests/DiaGame/` (format, kinds) and `Cluiche/Tests/GoogleTests/CluicheGameBaseline/` (catalogue, loader, results, module) (both new), `Cluiche/Tests/Fixtures/StageCatalogue/`.

## Binding Decisions

| Decision | How this feature complies |
|---|---|
| SC-002: tags plus named fields; no key-value store | Only `tags` and `description` are added; no free-form object |
| SC-003: fixed kind vocabulary, one list for C++ and Python | `stage_kinds.json` plus a GoogleTest that it matches `StageKinds.h` (AC 2-3) |
| SC-004: multi-group rows, results keyed on the stage | `GroupByKind` returns rows over entry indices. Results and the snapshot are per stage (AC 6, 14) |
| SC-006: last result persisted, written only by the game | Only `StageCatalogueModule` writes the file. The CLI only reads it (AC 10, 21) |
| SC-010: menu actions are `stages.*` commands | The three read commands land here. `stages.launch` comes in Feature 2 |
| PD-002 / AD-004: PU boundaries are streams | Results come in on an EventStream and go out on a FrameStream. The CluicheTest reporter runs on SimPU, where the registry is written, so it adds no new cross-PU read (AC 13, 14, 18) |
| PD-004 / AD-002: no STL in public APIs | Fixed-size Dia strings and containers throughout (AC 1, 4, 8) |
| PD-009: output under `out/<App>/` | `stage_results.json` sits in `out/<App>/` (AC 11) |
| PD-010: `.diastage` is metadata | The new fields are tooling metadata, and the runtime composer ignores them (AC 24) |
| AD-001: sign-off for engine API changes | Engine edits are limited to the DiaGame format fields, serializer and `StageKinds` (sign-off given 2026-11-18). The runtime module is in the Baseline (SC-001 amended), so it needs no engine sign-off |

## Test Plan

- **GoogleTests** (`DiaGame` for format and kinds; `CluicheGameBaseline` for the rest):
  - Serializer round trip with and without the new fields; wrong-type errors.
  - `StageKinds` matches the JSON.
  - Loader on fixtures: order, a transition with no import, an omitted import, 65 stages giving `droppedCount` 1, a missing `.diagame` and a malformed file.
  - `GroupByKind`: two kinds give two rows; free-form-only tags go to `other`.
  - `Filter` with all-match.
  - Results file: missing, malformed or wrong format starts empty; the merge keeps the newer result per stage; the temp file is renamed.
  - Module with fake streams: event to table to one write per frame (two events in one frame give one write); snapshot published on revision change only; health Degraded on write failure.
  - Query replies and the unknown-stage error.
- **pytest:**
  - Validator: kebab-case, duplicates, near-kind error message, length limits.
  - Python loader equals C++ output on the shared fixtures (a golden JSON produced by both).
  - `dia stages list --tag --kind --json`.
  - `dia docs stage-registry` output on fixtures.
- **E2E:**
  1. `dia run e2e` on one CluicheTest stage creates `out/CluicheTest/stage_results.json` with that stage's PASS.
  2. A second launch answers `stages.results` with it.
  3. `dia stages results --game cluichetest` prints it.
- **Manual:** `dia stages list --game cow` shows the three tagged stages.

## Resolved Design Questions (2026-11-18)

1. **Snapshot size.** `logPath` is dropped from the snapshot (AC 14). The menu does not need it; `stages.results` and the file still carry it. This keeps the snapshot well under the 48 KB assert.
2. **Running state persisted.** Kept as specced: RUNNING is written like any result, so a crash mid-test leaves RUNNING in the file, which shows the run never finished.

## Open Design Questions

1. **The CluicheTest reporter polls a singleton.** `TestResultsRegistry` is existing debt (no-singletons rule). The reporter reads it on its own PU, so the debt doesn't grow.
   - Replacing the registry with direct `StageResultEvent` sends from each test stage is the clean fix. It touches about 29 stage modules and is out of scope here.
