# System Spec: T-1 World Sim Inspector

**Parent:** @docs/specs/applications/cow/cow.md  
**Origin:** 2026-10-19 world-sim product review. The user asked for "an inspector to capture more graphs, charts and data insights into behaviours".  
**Mockup:** [mockups/world-sim-inspector.html](mockups/world-sim-inspector.html)  
**Built on:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md, @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md, @docs/specs/applications/dia/systems/diavalueschema/diavalueschema.md, @docs/specs/applications/cluicheeditor/systems/pluginbrowser/game-scoped-plugins.md

**Gameplay Domains:** debug

## Summary

T-1 is a CluicheEditor plugin for understanding how CoW's world simulation behaves over time. It reads DiaCellHistory, either live from a running CoW game over DiaDebugServer or offline from `dia.cell_history` report files. It shows:
- time series with event markers
- breakdowns by region and biome
- distributions across cells
- a "why" panel that sums the A1 ledger's causes
- correlations between two metrics
- run-vs-run compare, e.g. pack lush vs arid

It mostly reads. Clicking an event or "show cells" sends navigation commands back to the game so the in-game viewer jumps to the same time, selection and layer.

It is **CoW's first game-scoped editor plugin**: it exists in CluicheEditor only while `cow.diagame` is loaded. Generic Dia editors stay available in every game.

### Tool tier

This is the **Inspector** tier (feedback rule: Editor = offline files, Inspector = live game through DiaDebugServer, Visual Debugger = in-game overlay). It also opens report files. Those are generated output, not authored assets, so it stays an inspector and never edits data.

## Goals

1. **See behaviour over time.** Any aggregate (layer × group × stat) can be plotted over the whole run, with events marked: droughts, storms, biome transitions, ecology crashes.
2. **Explain changes.** For a region and period, the panel shows the top causes of a change, summed from the A1 ledger cause events.
3. **Find patterns.** It shows distributions across cells, breakdowns per region and biome, and correlations between two metrics.
4. **Compare runs.** Two sources are compared on aligned time: live vs file, or file vs file.
5. **Jump to the game.** Clicking an event, a cell or "show cells" moves the live game's viewer there.
6. **Every number is explained.** Labels, units and tooltips come from DiaValueSchema through the shared `ValueLabel` and `ValueTooltip`.

## Non-Goals

- **Tuning or editing.** That happens in testbeds (F-3 Save-as) and T-2 World Data Editor.
- **Recording.** DiaCellHistory records; T-1 only reads.
- **In-game graphs.** The systems' own panel graphs stay in-game.
- **Running the simulation.** Headless runs are the S-2.5, S-3 and S-3.5 Regression runners; T-1 opens their `dia.cell_history` output (amended 2026-10-19, consistency pass).

## Architecture

### Placement

**CoW editor code** (AD-002, CoW tier): `Cluiche/CoW/Editor/WorldSimInspector/`, in the `CoWEditor` static library.
- `CluicheEditor.vcxproj` references `CoWEditor`. That is a shared build file, so it needs AD-001 sign-off when built.
- The plugin is listed in `Cluiche/Assets/CoW/cow_editor.diaapp`, named from `cow.diagame` `editor.manifests` (Game-Scoped Plugins).

```
CoWWorldSimInspectorPlugin : Dia::Editor::LiveConnectionPluginBase   (C++)
  ├─ subscribes   cells.history.aggregates, cells.history.events, cells.view.state
  ├─ queries      cells.history.cells, cells.select.stats, valueschema.get
  ├─ file read    "openHistoryFile" bridge request → reads dia.cell_history JSON (C++), returns chunks
  └─ navigate     sends cells.history.view / cells.select.* / cells.layer.base to the game

UI (React 18 + zustand + @dia/editor-ui + uPlot, Vite)    Cluiche/CoW/Editor/WorldSimInspector/UI/
  ├─ model/      pure TS, no React: sources, alignment, analyses (unit-tested)
  │    Source { kind: live|file, schema, groups, aggregates, events, cellQuery() }
  │    analyses: series(), breakdown(), histogram(), causes(), pearson(), alignRuns()
  ├─ store/      zustand: sources A/B, time brush, chosen metrics, selection mirror
  └─ views/      TimeSeries (uPlot), EventTimeline, Breakdown, Distribution, HexMiniMap,
                 WhyPanel, Correlation, RunCompare
```

The UI is kept separate from the analysis code (feedback rule): views call only the `model/` functions and the store, never the bridge.

### Data contracts

| Need | Source | Notes |
|---|---|---|
| Aggregates (min/mean/max per layer × group × sample) | `cells.history.aggregates` topic; file `aggregates` | Whole run |
| Events | `cells.history.events` topic; file `events` | Typed; described in DiaValueSchema (`kind: event`) |
| Per-cell values at a sample | `cells.history.cells` query (chunked); file with `--cells` | Only for tracked layers. Otherwise the view says "track this layer" and offers `cells.history.track` |
| Labels, units, ramps | `valueschema.get` (live); a schema block in the file | File exports must embed the schema so offline files explain themselves |
| Causes for the why panel | Events of type `cow.ecology.cause {group, layer, cause, delta}` | **A1 contract:** each band, for each region, A1 emits its top causes per tracked layer (from DiaCellHistory's resolution of A1 Open Q4) |
| Live view state | `cells.view.state` | Shows the game's current base layer and selection; drives "in sync" status |

### Views (v1, all in the mockup)

- **Toolbar:**
  - connection pill, connected only while CoW is live
  - Source A (Live, or a file) and an optional Source B (compare)
  - a time-range brush shared by every view
  - Saved views (Open Q1)
- **Time series** (uPlot):
  - pick any number of metrics; axes are grouped by unit, two at most
  - event markers by type, with hover crosshair tooltips
  - the brush sets the time range
- **Event timeline:** a list filterable by type, group and time. Clicking an event navigates.
- **Breakdown:** a table of groups (regions or biomes) × chosen layers (mean over the brush), with a sparkline per row. Click a group to select it in-game.
- **Distribution:** a histogram of a tracked layer's per-cell values at the brush end, split by a grouping layer (small multiples).
- **Hex mini-map:** a tracked layer at the brush end, with the schema ramp, drawn with the shared `CellGridCanvas` from `@dia/editor-ui` (amended 2026-10-19, T-2). Clicking a cell navigates.
- **Why panel:** choose a group, a layer and the brush period to see the summed cause deltas as a bar list. "Show cells" selects the cells that contributed most.
- **Correlation:** a scatter of metric X against metric Y over group × sample within the brush, with Pearson r and a least-squares line. Choose the lag in samples (0 by default).
- **Run compare:** with Source B set, the time series shows A and B overlaid, the breakdown shows A, B and Δ, and events from both are listed with source badges.
  - Alignment is by sample label, falling back to sample index.
  - If the two sources have different groupings (different worlds), only world-level aggregates are compared, with a warning.

### Navigation (read + navigate)

| Action | Commands sent to the live game |
|---|---|
| Click an event | `cells.history.view <sample>`, `cells.select.category <group layer> <value>` (or `cells.select.cell`), `cells.layer.base <event's layer>` |
| Click a breakdown row | `cells.select.category <group layer> <value>` |
| Why panel "show cells" | `cells.select.cells <q,r…>` with the top contributing cells (new DiaCellLayers command, requested 2026-10-20; the query language cannot name cells) |
| Click a mini-map cell | `cells.select.cell <q> <r>`, `cells.history.view <sample>` |
| "Back to live" | `cells.history.live` |

- Navigation is enabled only when Source A is Live, or when the file's `world` id matches the live game's loaded world.
- Nothing else is sent: no tuning and no simulation control. The one addition is recording config: `cells.history.track` / `untrack` for the "Track this layer" button (Feature 3; amended 2026-10-20). It changes what DiaCellHistory records, not the simulation.

### Logging and observation

- **Channel:** `[EDITOR] T1`.
- **Logged:** source opened or closed, file load errors, alignment warnings, query chunk failures.
- **Profiling:** UI timings in dev builds (`performance.mark`).

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Plugin shell and sources | `CoWEditor` library, plugin, game-scoped registration, live subscribe, file open (C++ reads), source model, shared brush, schema tooltips | Approved | [plugin-shell-and-sources.md](plugin-shell-and-sources.md) |
| 2 | Time series and events | uPlot chart, metric picker, unit axes, event markers, event timeline, saved views, CSV export | Approved | [time-series-and-events.md](time-series-and-events.md) |
| 3 | Breakdown, distribution and mini-map | Group table with sparklines, histogram by group, hex mini-map | Approved | [breakdown-distribution-minimap.md](breakdown-distribution-minimap.md) |
| 4 | Why panel | Cause aggregation from `cow.ecology.cause` events, "show cells" | Approved | [why-panel.md](why-panel.md) |
| 5 | Correlation and run compare | Scatter with r and lag, Source B, alignment, overlay and Δ | Approved | [correlation-and-run-compare.md](correlation-and-run-compare.md) |
| 6 | Navigation | Commands back to the game, live/world-match gating, "back to live" | Approved | [navigation.md](navigation.md) |

- **Build order:** 1 → 2 → 6 → 3 → 4 → 5.
- Feature 1 can start against a hand-written `dia.cell_history` fixture before any simulation is built.
- An HTML mockup already exists. The visual gate for each view is checked against it.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Game-Scoped Editor Plugins (CluicheEditor) | Exists only with `cow.diagame` | Approved, not built (AD-001 sign-off needed) |
| DiaCellHistory Features 1, 2, 5 | Aggregates, events, cell queries, export format | Approved, not built |
| DiaCellLayers Features 2, 5 | Selection commands, view-state topic | Approved, not built |
| DiaValueSchema Features 1, 3, 6 | Schema query, `ValueLabel` / `ValueTooltip`, embedded schema in exports | Approved, not built |
| DiaDebugServer, `LiveConnectionPluginBase` | Transport, plugin base | Done |
| A1 cause events (`cow.ecology.cause`) | Why panel | Amend A1 (see below) |
| uPlot (npm) | Time series | New dependency |

**Amendments when Approved:**
- **A1:** emit `cow.ecology.cause` events each band per region (top causes per tracked layer).
- **DiaCellHistory:** export files embed the value schema.
- **cow.md:** add the Tools track (T-1, T-2).

## Binding Decisions

| Source | Decision | Constraint on T-1 |
|---|---|---|
| CoW AD-001 | Ask before Dia / CluicheEditor / shared build edits | The `CluicheEditor.vcxproj` reference and any DiaEditor change need sign-off |
| CoW AD-002 | Four-tier placement | CoW editor code lives in `Cluiche/CoW/Editor/`; nothing CoW-specific goes into Dia |
| AED-002 | Plugins are listed in a `.diaapp` `"editor"` section | `cow_editor.diaapp` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Game-Scoped Plugins | Game-scoped plugins are fully unavailable out of scope | T-1 is never created unless CoW is loaded |
| CH-004 | Live and file data share one format | One `Source` model serves both |

## Test Strategy

- **Vitest (model/):**
  - series extraction
  - alignment by label and by index
  - histogram bins
  - breakdown means over a brush
  - Pearson r on known data, including lag
  - cause summation and top-N
  - different-world compare falls back to world level
- **React Testing Library:** each view renders from a fixture source; tooltips come from the schema.
- **GoogleTests (C++):**
  - The `openHistoryFile` handler chunks a large file.
  - It rejects files whose format or version is wrong.
  - Navigation is gated when the world doesn't match.
- **E2E:**
  1. Load `cow.diagame` with a stub game publishing fixture history.
  2. T-1 shows the time series.
  3. Clicking an event sends the expected commands.
  4. Loading `cluichetest.diagame` closes T-1.
- **Perf:** a 40-year weekly run (2,080 samples) with 20 layers × 30 groups plots in under 300 ms and brushes at 60 fps.

## Resolved Decisions

1. **Saved views are in v1** (2026-10-19). A named view saves the chosen metrics, brush, sources and layout to `out/CoW/inspector_views/<name>.json`. Built in Feature 2.
2. **CSV export in v1, PNG later** (2026-10-19). "Export CSV" writes the current chart's series.
3. **Cause events only for tracked layers** (2026-10-19). A1 emits `cow.ecology.cause` per region per band, top 3 causes, only for layers tracked in DiaCellHistory.

## Proposed Amendment (2026-11-04, pending user approval): Feature 7, Snapshots and cell compare

**Origin:** DiaCellHistory was closed with its in-game panel work deferred (snapshot list and controls, delta banner/legend/group table; Dia features 3 and 4 tasks 3.10b and 4.10). The user decided these belong in the CluicheEditor Inspector, not in the in-game debug panel. The engine side is built and tested (commands, queries, topics, export); this amendment re-homes the UI here. **This section changes nothing until approved; T-1 stays `Approved` as written.**

**Scope (new Feature 7):**
1. **Snapshot panel.** List via `cells.snapshot.list` (name, sample, label, world, size, `sameRun`, status `pending|ready|failed` with reason, orphans shown as failed). Take (`cells.snapshot.take <name> [overwrite]`), Delete (`cells.snapshot.delete`), and Restore (`cells.snapshot.restore`) with a confirm that states the consequence (same run: history after the sample is dropped, N samples; earlier session: history is reset; other grid: refused, button disabled with the reason). The 16-snapshot cap, the one-operation-in-flight rule and the name rules (1-32 of `A-Za-z0-9_-`) are shown inline. Every action is a DiaAPI/wire command (the panel calls the same commands as the console). The visual reference is the snapshot states already drawn in `docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html` (unapproved; port into `mockups/world-sim-inspector.html` before building).
2. **Cell compare on the mini-map.** Snapshot vs now, snapshot vs snapshot, and sample vs sample for a chosen layer, computed **in the editor** from `cells.history.cells` (per-sample cell series) and `cells.history.snapshot {name, layer, offset, count}` (chunked), drawn with the layer's delta ramp, symmetric around zero, with a fixed/auto scale (grow at once, shrink after 2 s under 40%, the same rule as DiaCellHistory F4). A per-group table of mean A, mean B and change, sorted by absolute change. This is separate from Feature 5's run-vs-run compare (aggregates over two sources).
3. **Gating.** Take and Restore change the live game, so they need a live connection whose world id matches (the Feature 6 rule); offline file sources show snapshots read-only if the file lists them (`snapshots` field), without controls.

**Requirements this places on Dia (backlog items, not part of T-1 itself):**
- **Layer cursor for the aggregates catch-up.** `cells.history.aggregates` replies are capped at 15,000 bytes (the sim-hosted limit is 16,384 per reply), so a request without a `layers` filter errors for a 20-layer x 27-group run, and a 2,000-sample catch-up costs about 100 queries per layer. Feature 1's live source needs a cursor that walks the layers (or a groups filter, or a coarser first-screen summary).
- **Spec correction for decoders:** a float cell code `q` dequantises as `min + q/65534 x (max - min)` with `q = 0xFFFF` meaning NaN (DiaCellHistory export spec says /65535; fixed there on 2026-11-04). The conformance fixtures in `Dia/DiaCellHistory/Testing/Conformance/` follow the code.
- Text fields (labels, event text) are now truncated on UTF-8 boundaries, so exported files are valid UTF-8.

## Status

`Approved` (2026-10-19). All 6 feature specs `Approved` (2026-10-20); no plan yet. Build needs AD-001 sign-off for the `CluicheEditor.vcxproj` reference.
