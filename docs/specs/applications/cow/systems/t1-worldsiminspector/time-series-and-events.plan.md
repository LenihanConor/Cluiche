# Plan: T-1 Time Series and Events (Feature 2)

**Spec:** @docs/specs/applications/cow/systems/t1-worldsiminspector/time-series-and-events.md
**Status:** Not Started
**Mockup (approved 2026-11-16):** `mockups/world-sim-inspector-v2.html`, "Trends" workspace (chart, legend, event timeline, saved views). Older `mockups/world-sim-inspector.html` is the secondary reference (the spec's own visual gate names it). Both are visual gates for the view tasks. Where they differ, v2 wins.

## Prerequisites (gate, checked in Task 0)

As of 2026-11-16 `Cluiche/CoW/Editor/` holds only the `CoWEditor` stub (`CoWEditor.h/.cpp`, vcxproj). There is no `WorldSimInspector/` folder, no UI project, and uPlot is not installed anywhere in the repo (grep over `Dia/` and `Cluiche/` finds no uPlot import or package entry).

| Needs | From | Used by tasks |
|---|---|---|
| UI project scaffold (Vite, React 18, zustand, `@dia/editor-ui`, Vitest, RTL), `model/` + `store/` layout | T-1 Feature 1 | 1-6, 8-12 |
| `Source` model (`aggregates()`, `events()`, `samples`, `schema`), shared brush, `ValueLabel` / `ValueTooltip`, checked-in fixture + stub publisher | T-1 Feature 1 | 1, 3, 6, 8-12 |
| `CoWWorldSimInspectorPlugin` with a UI bridge request mechanism (`openHistoryFile` is the only existing pattern) | T-1 Feature 1 | 7 |
| Feature 6 navigation intents (event click to commands) | T-1 Feature 6 | hook only, see Cross-feature hooks |

**Do not start Task 1 until Feature 1 is Done.** Tasks 1 to 5 are pure TS and may start as soon as Feature 1's scaffold and `Source` types exist.

Existing repo patterns to copy (real paths):
- React + zustand + Vite + Vitest UI projects: `Dia/DiaEconomyInspector/UI/`, `Dia/DiaBlackboardInspector/UI/`, `Dia/DiaEntityInspector/UI/` (package.json, vite.config.ts, tsconfig.json, `src/`).
- Shared UI package `@dia/editor-ui`: `Dia/DiaEditorUI/src/` (includes `CellGridCanvas.tsx`; no chart component exists there).
- C++ plugin base and test pattern: `Dia/DiaEconomyInspector/DiaEconomyInspectorPlugin.cpp`, `Dia/DiaAIInspector/DiaAIInspectorPlugin.cpp` (`LiveConnectionPluginBase`); GoogleTest pattern `Cluiche/Tests/GoogleTests/Integration/TestLiveConnectionPluginBase.cpp`.

## Sign-offs (AD-001)

- **uPlot npm dependency: already signed off 2026-11-05** for the parent system. Evidence: `docs/BACKLOG.md` line 20 (AD-001 list names uPlot, "Nothing is outstanding") and `seasons.md` (AED-005 row: "uPlot (signed off 2026-11-05)"). The feature spec's own text ("needs AD-001 sign-off at build time") is stale. Task 0 re-confirms this in the backlog; if it cannot be confirmed, Task 8 stops and asks.
- C++ saved-view and export file I/O under `out/CoW/inspector_views/` and `out/CoW/inspector_exports/` is CoW-side code in `Cluiche/CoW/Editor/WorldSimInspector/`. No sign-off needed (AD-002 tier).
- No Dia, DiaEditor or shared build file change is planned. If Task 7 finds it needs one, stop and flag.

## Cross-feature hooks this feature must leave

| Hook | For | How it is left |
|---|---|---|
| `transform` on series | Feature 8 (anomaly mode: value minus seasonal baseline) | `series(source, metric, brush, opts?)` takes an optional `transform?: (y: Float32Array, samples) => Float32Array` applied after extraction, before axis and unit logic. The metric may carry an optional `unitOverride`. Default is identity. Tested with a trivial transform |
| Event click navigation intent | Feature 6 | Marker click and row select set `selectedEvent` in the store and nothing else. The store exposes `selectEvent(event, origin: 'marker' \| 'row')` and a subscribe point; Feature 6 subscribes and builds the command plan. This feature sends no command (AC 9) |
| Source list | Feature 5 (Source B overlay) | `series()` is called per `(source, metric)`; the chart takes `SeriesSpec[]` with a `sourceId`. Only A is populated here |
| Built-in saved views | Feature 8 (weather default view, ecology default view) | Shipped as JSON under `Cluiche/CoW/Editor/WorldSimInspector/UI/views/builtin/` (`weather.json`, `ecology.json`), read through the same `savedView.ts` restore path, listed read-only in the saved views menu |

## Implementation Patterns

### Models (Tasks 1-5), pure TS, no React, no bridge
- Files in `UI/model/`: `series.ts`, `axes.ts`, `events.ts`, `savedView.ts`, `csv.ts`. Views import these and the store only.
- `series.ts`: `series(source, metric, brush, opts?) -> { x: Float64Array, y: Float32Array, unit, labels: string[] }`. `x` is the sample index; `labels` come from `source.samples`. NaN stays NaN (uPlot draws a gap). Brush is inclusive on both ends and clamped to the populated range. Unit comes from the schema entry of `metric.layer`, never from the view.
- `axes.ts`: `assignAxes(metrics) -> { axes: Axis[<=2], assignment: Map<metricKey, axisIndex>, error? }`. Group by unit in first-seen order. Third unit gives `error: 'Only two units can share a chart'`. `addMetric(list, metric)` also enforces the 8-series cap with its own message; both refusals are returned values, never exceptions. Duplicate metrics are rejected.
- `events.ts`: `filterEvents(events, {types, groups, range})` where empty `types`/`groups` means all; `markerStyle(event, schema) -> {color, glyph, label}` with a neutral fallback plus a logged warning when the schema has no entry; `collapseMarkers(events, xToPx, thresholdPx) -> Marker[]` merges events whose pixel distance is under the threshold into one `{x, count, events[]}` (threshold constant `MARKER_COLLAPSE_PX`, default 6, tunable, Open Question 1).
- `savedView.ts`: `serialise(state) -> SavedViewV1` (shape exactly as in the spec; brush stored by sample label, sources stored as `{kind, path?}`, live has no path); `restore(json, ctx) -> { state, skipped: SkippedItem[] }` where `ctx` exposes open sources and the schema. Skips missing file, metric not in the source, untracked or unknown layer, unknown event type; wrong `format`/`version` rejects the whole file with one message. A brush label missing from the new sample list snaps to the nearest label inside range. Pure; no file access.
- `csv.ts`: `toCsv(seriesList, brush) -> string`. Columns `sample,label,<layer> / <group> / <stat> (<unit>)...`; one row per sample inside the brush; NaN as empty; fields containing comma, quote or newline are RFC 4180 quoted; `\n` line ends. Values are formatted from the same Float32Array the chart plots (AC 12).

### Store (Task 6)
- zustand slices in `UI/store/`: `metrics[]`, `eventFilter {types, groups, followBrush}`, `selectedEvent`, `savedViewName`, `hiddenEventTypes`, `skippedReport`. Actions call model functions and return their refusal messages for the view to show. No bridge calls in the store; file I/O goes through a thin `inspectorFiles.ts` adapter in `UI/bridge/` (the one place that talks to the host), injected so tests use a fake.

### Bridge (Task 7), C++
- Feature 1 only defines `openHistoryFile`. The spec names `writeInspectorFile`, but **loading a saved view also needs to list and read `out/CoW/inspector_views/`**, which the spec does not mention. Task 7 adds three requests to `CoWWorldSimInspectorPlugin`: `writeInspectorFile {folder: "views"|"exports", name, text}`, `listInspectorViews`, `readInspectorView {name}`. Built-in views are bundled with the UI and need no bridge call.
- Path rules: the folder is an enum, never a path; `name` must match `^[A-Za-z0-9_ -]{1,64}$` plus the extension added by C++; reject separators, `..`, drive letters, reserved Windows names. Resolve to the canonical path and check it stays under the allowed root. Create the folder if missing. Atomic write (temp file then rename). Max size cap (for example 8 MB for CSV, 256 KB for views). Export file name `<timestamp>.csv` is generated by C++, not the UI.
- GoogleTests: `*WorldSimInspector*` filter, accept, reject every escape (`../x`, absolute path, `a/b`, trailing dot or space, reserved name, over-length, over-size), overwrite of an existing view, list ordering, read of a missing name.

### Chart (Task 8), uPlot
- `views/TimeSeries.tsx` wraps uPlot imperatively: create once, `setData` on data change, `setSize` on resize (ResizeObserver), destroy on unmount. Data is passed as typed arrays with no per-point object allocation. Axes: left and right from `assignAxes`; y-scales per unit; x is sample index with the label formatter from `samples`.
- Event markers: a uPlot `hooks.draw` plugin draws vertical lines and glyphs from `collapseMarkers` onto the canvas; marker hit-testing is done in model code from the same marker list (so marker click to select is testable without pixels). Selected marker is drawn emphasised.
- Hover crosshair tooltip: `hooks.setCursor`; tooltip text built by a model helper (`tooltipRows(seriesList, idx)`) so it is unit-testable. Over a collapsed marker, the tooltip lists the events inside.
- Drag select: `hooks.setSelect` converts to sample indices and calls the store's `setBrush` (Feature 1). A brush change from the toolbar calls `setScale('x', ...)` and redraws; guard against feedback loops (ignore the redraw-originated select).
- Perf marks: `performance.mark('t1.series.build')`, `'t1.chart.draw'` in dev builds only.
- jsdom has no canvas, so RTL tests mock the uPlot constructor and assert on the calls and on model output; real drawing is covered by the perf task and the visual gate.

### Views (Tasks 9-12)
- `MetricPicker`: layer, group, stat dropdowns from `source.schema` and `source.groups` via `ValueLabel`; Add button disabled-with-tooltip-reason when the 8 cap or unit rule would be violated (reason from the model result, not computed in the view); legend rows with remove, per-series colour swatch, per-event-type toggles.
- `EventTimeline`: virtualised (windowed) list; row = sample label, type glyph, group, layer, summary text from the schema. Filters: type, group, "limit to brush" (default on). Row select and marker select stay in sync through `selectedEvent`; selecting scrolls the row into view.
- `SavedViewsMenu`: Save (name box, overwrite confirm), Open, built-in section, Export CSV. On partial restore, shows a message listing the skipped items. Opening a view with a `live` source while offline restores the rest and reports the live source as skipped (Open Question 3: confirm this is the right behaviour, record the answer in the spec).
- Wire all four into the Trends workspace of the v2 shell as the Feature 1 shell defines it. Log lines on channel `[EDITOR] T1`: saved view saved / loaded / partially restored, CSV exported, rejected third unit, rejected ninth series.

### Fixtures
- Small: Feature 1's checked-in `dia.cell_history` fixture (about 100 samples).
- Large perf fixture (Task 13): generated by a seeded script `UI/tools/genPerfFixture.ts` (2,080 samples, 20 layers, 30 groups, a few hundred events, deterministic PRNG, not checked in; generated at test time into memory). Series have seasonal structure plus noise so uPlot does real work.

### Verification cadence
- Per task: scoped Vitest (`npx vitest run <file>` from `Cluiche/CoW/Editor/WorldSimInspector/UI/`) or scoped GoogleTests (`dia run googletest --filter="*WorldSimInspector*" --shards 2`).
- ONE full Vitest + GoogleTests run, the E2E scenario and the perf check run once, in Task 14.
- No mutation checks (decided 2026-11-12). Task 15 is a gap audit plus edge tests only.
- All build and run steps use the `dia` CLI. Never call executables directly.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Gate: confirm T-1 Feature 1 is Done (UI scaffold, `Source` model, brush, store, `ValueLabel`, fixture, `openHistoryFile` pattern); re-confirm the uPlot sign-off line in `docs/BACKLOG.md`; record baseline Vitest and `*WorldSimInspector*` GoogleTest pass counts | — | Not Started | haiku | No code. If Feature 1 is not Done, stop and report |
| 1 | RED → GREEN: `model/series.ts` extraction over a brush, unit from schema, NaN gaps, optional `transform` hook | AC 1: hand-built fixture; brush start/end inclusive and clamped; empty brush; NaN preserved; transform applied before unit logic; labels match samples | Not Started | sonnet | Leaves Feature 8 hook |
| 2 | RED → GREEN: `model/axes.ts` `assignAxes`, `addMetric` with 8-series cap and duplicate check | AC 2, 3: 1, 2, 3 units; first-seen axis order; ninth series refused; third unit refused with exact message; refusal leaves the list unchanged | Not Started | sonnet | |
| 3 | RED → GREEN: `model/events.ts` `filterEvents`, `markerStyle`, `collapseMarkers`, `tooltipRows` | AC 4, 5, 6: type/group/range filters and their combinations; schema colour and glyph, fallback with warning; collapse at threshold-1, threshold, threshold+1 px; count and member list; tooltip row per series with unit | Not Started | sonnet | `MARKER_COLLAPSE_PX` is a constant (Open Q1) |
| 4 | RED → GREEN: `model/savedView.ts` serialise and restore | AC 10, 11: round trip equals; missing file, unknown metric, untracked layer, unknown event type each skipped and listed; wrong format/version rejects; brush by label survives a different sample count; live source offline reported | Not Started | sonnet | Built-in JSON shapes validated by the same code |
| 5 | RED → GREEN: `model/csv.ts` `toCsv` | AC 12: known fixture exact text; unit in header; NaN empty; quoting; one row per sample in brush; values equal `series()` output | Not Started | sonnet | |
| 6 | Store slices (`metrics`, `eventFilter`, `selectedEvent`, `savedViewName`, `hiddenEventTypes`, `skippedReport`) and `inspectorFiles.ts` adapter interface with a fake; `selectEvent(event, origin)` | AC 9 store half: add/remove metrics return refusal messages; selecting sets `selectedEvent` and calls no adapter method; filter defaults (brush filter on); restore applies state and records skips | Not Started | sonnet | Navigation hook for Feature 6 |
| 7 | C++: `writeInspectorFile`, `listInspectorViews`, `readInspectorView` handlers in `CoWWorldSimInspectorPlugin`, path-restricted to `out/CoW/inspector_views/` and `out/CoW/inspector_exports/`; UI adapter implementation | GoogleTests: allowed writes land only in the two folders; every escape form rejected; size and name caps; atomic overwrite; list and read | Not Started | sonnet | Spec only names the write request; list/read are added here. No sign-off needed (CoW-side). `dia run googletest --filter="*WorldSimInspector*" --shards 2` |
| 8 | Add `uplot` to the WorldSimInspector UI `package.json` (signed off 2026-11-05) and build `views/TimeSeries.tsx`: uPlot wrapper, unit axes, draw-hook markers, crosshair tooltip, drag-to-brush, brush-to-scale sync | RTL with mocked uPlot: data and axes passed correctly; brush change calls `setScale`; drag select calls `setBrush`; marker hit-test selects; no feedback loop; destroy on unmount | Not Started | sonnet | uPlot is not yet in the repo: also confirm it bundles under the Vite build used by the CluicheEditor plugin load. Visual gate: v2 Trends chart |
| 9 | `MetricPicker` and legend: layer/group/stat pickers via `ValueLabel`, add/remove, cap and unit refusals shown inline, per-event-type toggles | RTL AC 2-4: ninth and third-unit messages, disabled reasons, legend toggle hides and shows markers; tooltips from schema | Not Started | sonnet | Visual gate: v2 Trends legend; older mockup for picker layout |
| 10 | `EventTimeline`: virtualised list, type/group filters, brush filter toggle (default on), row and marker selection sync | RTL AC 8, 9: filters; brush filter on by default and off on toggle; 5,000-row fixture renders a window only; marker click selects row and row click highlights marker; no command emitted | Not Started | sonnet | Visual gate: v2 Trends event list |
| 11 | `SavedViewsMenu` (save, open, overwrite confirm, built-in section) and Export CSV button; ship `views/builtin/weather.json` and `ecology.json` | RTL AC 10-12: save then restore through the fake adapter; partial restore message lists skips; CSV button writes via adapter with generated text; built-ins listed read-only | Not Started | sonnet | Built-in JSONs are the hook for Feature 8; metric names must exist in the schema (`weather.*` layers come from A4 F5, not built: restore will report them skipped until then, state this in the notes) |
| 12 | Wire chart, picker, timeline and menu into the Trends workspace of the plugin shell; `[EDITOR] T1` logs; dev `performance.mark` around series build and draw | RTL: workspace renders from the fixture source; brush in toolbar redraws chart (AC 7); log lines for save, load, partial restore, CSV export, rejected third unit and ninth series | Not Started | sonnet | Depends on Feature 1's shell having a Trends slot; if it has none, record in Notes and add it as a minimal slot |
| 13 | Perf: seeded generator for the 2,080 × 20 × 30 fixture; scripted test with 8 series plotted asserting series build plus first draw under 300 ms, and a brush-drag loop holding 60 fps (frame time budget 16.7 ms p95) | AC 13: budget assertions in a Vitest browser-mode or Playwright-driven run (jsdom cannot measure canvas draws); numbers quoted in notes | Not Started | sonnet | If the 300 ms budget is missed, first look at per-point allocation and uPlot `setData` resets before changing the design. Report pass/fail with the measured numbers |
| 14 | One full run: full Vitest, full GoogleTests (`dia run googletest --shards 2`), E2E scenario (stub publisher: chart appears, save/restore round trip across app reload, CSV file exists in `out/CoW/inspector_exports/`), visual gate against v2 Trends and the older mockup | AC 10, 14 plus everything above passing; screenshots compared by the user | Not Started | sonnet | `dia test e2e` does not build: run `dia pipeline --target cluichetest --stage compile-code` first. Kill leftover CluicheTest.exe. Report pass/fail and failing names only |
| 15 | Gap audit and edge tests (no mutation checks): re-read all 14 ACs against the tests; add edges (single-sample brush, all-NaN series, zero events, 8 series then remove one then add, duplicate event samples, view saved with a since-deleted layer, view name collisions, CSV with commas in labels, live source going stale mid-chart, very long group names) | Added tests fail first where they find a bug; fixes kept minimal | Not Started | sonnet | Explicit task per the exhaustive-test rule. Also record answers to Open Questions 1 and 3 in the spec |
| 16 | Close-out: Observation Opportunity Scan (suggestions only), `dia docs spec-done time-series-and-events.md`, update parent features table, backlog row to history, note built-in view files for Feature 8 | — | Not Started | haiku | Scan across logs, traces, profiling, metrics, health for the touched files. Use `dia docs` commands rather than manual edits. Do not commit unless asked |
