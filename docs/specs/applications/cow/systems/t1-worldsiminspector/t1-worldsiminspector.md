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
- (amended 2026-11-16) a typical-year and anomaly view, S-2.5 weather check results, space-over-time views, a world health dashboard and cross-system cause chains

It covers the **whole world sim, weather as much as ecology** (amended 2026-11-16). S-2.5 has no in-game graphs, so T-1 is the only place to see weather over time.

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
7. **Weather is first-class** (amended 2026-11-16). Seasonal cycles, moving storms and droughts, and the S-2.5 checks have views built for them, not only the generic charts.
8. **Judge the world, not just inspect it** (amended 2026-11-16). Health measures say whether the world is alive; cause chains link weather to ecology to succession.

## Non-Goals

- **Tuning or editing.** That happens in testbeds (F-3 Save-as) and T-2 World Data Editor.
- **Recording.** DiaCellHistory records; T-1 only reads.
- **In-game graphs.** The systems' own panel graphs stay in-game.
- **Running the simulation.** Headless runs are the S-2.5, S-3 and S-3.5 Regression runners; T-1 opens their `dia.cell_history` output (amended 2026-10-19, consistency pass).

## Architecture

### Placement

**CoW editor code** (AD-002, CoW tier): `Cluiche/CoW/Editor/WorldSimInspector/`, in the `CoWEditor` static library.
- `CluicheEditor.vcxproj` references `CoWEditor`. That is a shared build file; AD-001 sign-off was given 2026-11-05, and the reference and `RegisterCoWEditorPlugins()` call already exist in the repo (checked 2026-11-16).
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
| Causes for the why panel | Events of type `cow.ecology.cause {group, layer, cause, delta, cells}` (`cells` = top 8 contributing cells, A1 amendment 2026-10-20) | **A1 contract:** each band, for each region, A1 emits its top causes per tracked layer (from DiaCellHistory's resolution of A1 Open Q4) |
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
| 7 | Snapshots, cell compare and history status | Snapshot panel (list, take, delete, restore), delta compare on the mini-map, history status strip with Track/Untrack | Approved | [snapshots-and-compare.md](snapshots-and-compare.md) |
| 8 | Seasons: typical year and anomaly | Mean by week of year with the spread across years; anomaly mode on series and mini-map (value minus the typical value for that week); baseline is this run or a reference file; weather default view | Approved | [seasons.md](seasons.md) |
| 9 | S-2.5 checks panel | Opens `cow.s25_report` v1 files and `baseline.json`: verdict, per-check state, measured value against its ±10% band, trend across archived reports, check → run history link, performance/edge-case/live-smoke/baseline blocks, read-only live progress | Approved | [s25-checks-panel.md](s25-checks-panel.md) |
| 10 | Space over time | Strip chart (time × latitude band or region), one map per period, map playback, storm and drought tracks; region-level fallback outside the per-cell window | Approved | [space-over-time.md](space-over-time.md) |
| 11 | World health dashboard | Derived measures: biome diversity, biome turnover, near-extinction warnings, boom-bust period, settle time, drift; weather health; fixed coupling pairs; bands cite the GDD or are provisional; rolling sparklines, region table, A vs B | Approved | [world-health.md](world-health.md) |
| 12 | Cross-system causes | Cause chains weather → ecology → succession built from events, A1 causes and per-region lag scans; automatic lead/lag scan (preset pairs moved to Feature 11's coupling measures); recorded links (A1 causes, A3 source) vs candidate links; swimlanes + ranked chains | Approved | [cross-system-causes.md](cross-system-causes.md) |

- **Build order:** 1 → 2 → 6 → 3 → 8 → 9 → 4 → 5 → 7 → 10 → 11 → 12 (amended 2026-11-16: 8 and 9 move ahead because S-2.5 has no other viewer).
- Feature 1 can start against a hand-written `dia.cell_history` fixture before any simulation is built.
- An HTML mockup already exists. The visual gate for each view is checked against it.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Game-Scoped Editor Plugins (CluicheEditor) | Exists only with `cow.diagame` | Approved; sign-off given 2026-11-05, `CoWEditor` reference already wired |
| DiaCellHistory Features 1, 2, 5 | Aggregates, events, cell queries, export format | Done (2026-11-04) |
| DiaCellLayers Features 2, 5 | Selection commands, view-state topic | Built (`cells.select.cells`, `cells.view.state`; AD-001 sign-off for `cells.select.cells` not recorded, see navigation plan) |
| DiaValueSchema Features 1, 3, 6 | Schema query, `ValueLabel` / `ValueTooltip`, embedded schema in exports | Built (DiaValueSchema Done) |
| DiaDebugServer, `LiveConnectionPluginBase` | Transport, plugin base | Done |
| A1 cause events (`cow.ecology.cause`) | Why panel | Amend A1 (see below) |
| uPlot (npm) | Time series | New dependency |
| A4 Feature 5 (`weather.*` layers, `weather.storm` / `weather.drought` events) | Features 8, 10, 12 | Approved, not built |
| S-2.5 Feature 3 (`cow.s25_report` v1) and Feature 5 (history files per check) | Feature 9 | Approved, not built |
| A3 transition events | Features 11, 12 | Approved, not built |

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

## Amendment (2026-11-04; approved by the user 2026-11-16): Feature 7, Snapshots and cell compare

**Feature spec: [snapshots-and-compare.md](snapshots-and-compare.md) (Approved 2026-11-16).** On approval the scope also gained the history status strip (event legend, tier memory against budget, Track/Untrack), decided 2026-11-16.

**Origin:** DiaCellHistory was closed with its in-game panel work deferred (snapshot list and controls, delta banner/legend/group table; Dia features 3 and 4 tasks 3.10b and 4.10). The user decided these belong in the CluicheEditor Inspector, not in the in-game debug panel. The engine side is built and tested (commands, queries, topics, export); this amendment re-homes the UI here. **This section changes nothing until approved; T-1 stays `Approved` as written.**

**Scope (new Feature 7):**
1. **Snapshot panel.** List via `cells.snapshot.list` (name, sample, label, world, size, `sameRun`, status `pending|ready|failed` with reason, orphans shown as failed). Take (`cells.snapshot.take <name> [overwrite]`), Delete (`cells.snapshot.delete`), and Restore (`cells.snapshot.restore`) with a confirm that states the consequence (same run: history after the sample is dropped, N samples; earlier session: history is reset; other grid: refused, button disabled with the reason). The 16-snapshot cap, the one-operation-in-flight rule and the name rules (1-32 of `A-Za-z0-9_-`) are shown inline. Every action is a DiaAPI/wire command (the panel calls the same commands as the console). The visual reference is the snapshot states already drawn in `docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html` (unapproved; port into `mockups/world-sim-inspector.html` before building).
2. **Cell compare on the mini-map.** Snapshot vs now, snapshot vs snapshot, and sample vs sample for a chosen layer, computed **in the editor** from `cells.history.cells` (per-sample cell series) and `cells.history.snapshot {name, layer, offset, count}` (chunked), drawn with the layer's delta ramp, symmetric around zero, with a fixed/auto scale (grow at once, shrink after 2 s under 40%, the same rule as DiaCellHistory F4). A per-group table of mean A, mean B and change, sorted by absolute change. This is separate from Feature 5's run-vs-run compare (aggregates over two sources).
3. **Gating.** Take and Restore change the live game, so they need a live connection whose world id matches (the Feature 6 rule); offline file sources show snapshots read-only if the file lists them (`snapshots` field), without controls.

**Requirements this places on Dia (backlog items, not part of T-1 itself):**
- **Aggregates catch-up: BUILT 2026-11-05 (DiaCellHistory Feature 6, `aggregate-catch-up.md`).** `cells.history.aggregates` takes `groups`, `stride` and a layer cursor (`next = {from, layerOffset, structure}`). **How the live source should use it:** (1) fetch `cells.history.meta`; (2) first request `{groups: ["world"], stride: N}` with N chosen so the whole run is about 100 to 200 windows (target: at most 3 queries; **not measured** because the perf check was skipped), and draw the overview; the float min/mean/max are re-aggregated per window and category layers show the last sample's counts (label them end-of-window state); (3) load other groups and `stride = 1` detail lazily for the visible layers and range, re-sending `to`, `layers`, `groups`, `stride` with each follow-up and the `next` fields; (4) on 'structure changed: restart the catch-up', refetch meta and restart. Replies stay under 15,000 bytes; one layer of the requested groups that cannot fit gives 'request fewer groups'.
- **Spec correction for decoders:** a float cell code `q` dequantises as `min + q/65534 x (max - min)` with `q = 0xFFFF` meaning NaN (DiaCellHistory export spec says /65535; fixed there on 2026-11-04). The conformance fixtures in `Dia/DiaCellHistory/Testing/Conformance/` follow the code.
- Text fields (labels, event text) are now truncated on UTF-8 boundaries, so exported files are valid UTF-8.

## Amendment (2026-11-16, approved by the user): world-sim scope, Features 8 to 12

**Origin:** design discussion on the mockup. T-1 was ecology-shaped (the mockup said "Ecology Long-Run Analysis", the why panel reads only A1). The data side already handles weather: A4 Feature 5 registers seven `weather.*` layers and emits `weather.storm` / `weather.drought` events through DiaCellHistory, so they appear in Features 2 to 5 unchanged. What was missing are views built for weather and for judging a world. Each new feature needs its own `/spec-feature` interview and an approved mockup before building.

**Feature 8, Seasons.** Weather is a strong yearly cycle plus weekly noise, so a 40-year raw line is unreadable.
- Typical year: x is week of year (from the `Y<year> W<week>` sample label), the line is the mean across years and the band is the spread (min-max or p10-p90).
- Anomaly mode for any series: value minus a seasonal baseline.
- A weather default view (precipitation, temperature, snow pack, ground water) beside the ecology one.
- *Resolved 2026-11-16 (seasons.md):* the baseline is the run's own typical year or a reference history file. A4's table isn't in history files and `baseline.json` holds check magnitudes, so neither is used.

**Feature 9, S-2.5 checks panel.** A new file source kind for `out/CoW/s25/report_<world>_<seed>.json` and `report_summary.json` (envelope `cow.s25_report` v1, S-2.5 Feature 3) plus `baseline.json`.
- Shows the verdict with its precedence reason, prechecks, each check's state and measured value against its band, and the performance block (report-only).
- Opening several reports shows each check's trend across them.
- A check links to its `dia.cell_history` file (S-2.5 Feature 5 writes one per check run), so a failing check opens straight into its curve.
- *Resolved 2026-11-16 (s25-checks-panel.md):* live smoke is shown, together with performance, edge-case coverage and baseline state. Live status is read-only. Needs S-2.5 amendments: runs per check, a report archive, and a live-smoke envelope.

**Feature 10, Space over time.** The mini-map shows one frame only.
- **Strip chart:** time on x, space on y, with rows = latitude bands or regions and colour = a layer's mean. It shows drought fronts and seasonal belts moving. Built from per-cell history, or from aggregates when the rows are groups (no per-cell budget needed).
- **One map per period:** a mini-map per decade or season.
- **Playback** of the mini-map over the brush.
- **Storm and drought tracks:** event footprints over time.
- *Resolved 2026-11-16 (space-over-time.md):* A4 emits `weather.storm_track` per week and `weather.drought_end` (an A4 Feature 5 amendment). Latitude rows come from a new S-1 grouping layer (S-1 amendment 7).

**Feature 11, World health dashboard.** Designer measures instead of raw layers, derived in the editor:
- **Diversity:** Shannon diversity of biome counts (category-layer aggregates).
- **Turnover:** cells changing biome per decade (A3 transition events or a biome delta).
- **Near-extinction:** species or group layers below a floor.
- **Boom-bust:** period from autocorrelation of a series.
- **Settle time:** the time until rolling variance falls under a threshold.

Each measure is defined in a data file with DiaValueSchema descriptions, so tooltips work like every other number. Thresholds are Inspector config, not simulation tuning.
- *Resolved 2026-11-16 (world-health.md):* definitions live in a plugin JSON file whose bands cite the GDD or are marked provisional. No measure needs new A1 data (`ecology.state` already exists). Near-extinction covers groups, and covers species where they are recorded.

**Feature 12, Cross-system causes.**
- **Chains:** a weather event in region R at week w, an ecology change in R at w+k, then succession transitions in R. They are linked by region overlap and a lag window, built from existing events, A1's `cow.ecology.cause` (its causes already name weather inputs, since A1 reads rainfall, temperature and snow through `IEcologyWeatherInput`) and per-region aggregates.
- **Preset pairs:** rain → producers, snow pack → ground water.
- **Automatic lag scan** across metric pairs, per region, to find leading indicators.
- v1 is editor-side links, labelled as "candidate" links (correlation, not proof). Only if that proves insufficient does it ask A4 and A3 for recorded cause events, which would be a game-side contract change.
- Meaningful only once A4, A1 and A3 are built. Fixtures are enough to build it.
- *Resolved 2026-11-16 (cross-system-causes.md):* links are recorded (A1 cause events naming weather, and A3 transitions naming their `source` via a small A3 amendment) or candidate, each badged. A4 needs no cause events. The preset pairs became Feature 11's coupling measures.

**Derived metrics and wind (2026-11-16, user decision).** Wind cannot be a vector layer without a Dia engine change, so A4 records `weather.wind_east` and `weather.wind_north` (see A4 `weather-debug.md`, "Wind component layers") and T-1 derives wind itself. A shared pure module `model/derived.ts` (built in Feature 2, used by Features 3, 5, 8, 10, 11, 12) defines derived metrics:
- `weather.wind_speed = hypot(east, north)` and `weather.wind_direction = atan2(north, east)` in degrees. They appear in every metric and layer picker, marked "derived".
- **Per-cell** (mini-map, strip rows from per-cell data): exact, computed per cell then aggregated.
- **Group or world level from aggregates:** computed from the mean components, so the value is the **resultant** wind and is labelled "resultant speed" (it is lower than the mean speed when directions disagree). Direction aggregates are the resultant direction. Min and max of a derived metric are not available from aggregates (shown as "n/a").
- Direction is circular: charts show it on a 0 to 360 axis with wrap-aware lines, correlations use the components (not the angle), and the anomaly mode (Feature 8) applies to the components.
- Feature 10's mini-map draws **wind arrows** from per-cell components as an overlay when a wind layer is chosen.
- **Species and groups:** every `ecology.species.<id>` and `ecology.group.<tag>` the source records appears in the grouped layer picker. S-3 records them all as aggregates (S-3 amendment). Unrecorded species are listed greyed as "not recorded".

**Change-over-time additions (2026-11-16, user request: "all").** Seven views that show *change*, each written as a "Change-over-time additions" section with its own criteria in the feature that owns it: change hot-spot map, cell trail and slope chart (Feature 3, `breakdown-distribution-minimap.md`); event density strip on the brush (Feature 2); calendar heatmap (Feature 8); composition over time (Feature 10); change badges on the health cards (Feature 11). All are drawn in the v2 mockup.

**Plan impact:** `time-series-and-events.plan.md` (add `derived.ts` and the event density strip), `breakdown-distribution-minimap.plan.md` (derived layers, hot-spot map, cell trail, slope view), `breakdown-distribution-minimap.plan.md` (derived layers in the mini-map), and later the Feature 8, 10 and 11 plans.

**Not taken (2026-11-16):** seed ensembles (N-file bands and outlier seeds). Kept as an idea.

**Mockup:** retitled World Sim (2026-11-16). **New mockup drawn 2026-11-16, approved by the user 2026-11-16 (visual gate passed for Features 8 to 12 and the shared shell): [mockups/world-sim-inspector-v2.html](mockups/world-sim-inspector-v2.html)** (six workspaces: Overview, Trends, Space, Causes, Compare, Checks; lens toggle; per-workspace state bar; Feature 7's full state set stays in the v1 tab). Original request (user, 2026-11-16): the current one is an ecology-era single screen with the Feature 7 tab bolted on, so it cannot hold Features 8 to 12. Draw a new world-sim mockup (layout and navigation for weather and ecology, plus every new view) once Features 8 to 12 are specced, and get it approved before any of their UI is built. Until then each feature spec says what its views must show.

**Ripple:** S-2.5 `time-series-graphs.md` named "T-1 Feature 7" as the graph viewer. It now points at Features 2, 8 and 9 (pointer-only edit, 2026-11-16).

## Status

`Approved` (2026-10-19; amended 2026-11-16 for Features 7 to 12). All 12 feature specs `Approved` (8 to 12 on 2026-11-16); Feature 7 has a plan. The new world-sim mockup (v2) is approved, so the visual gate is passed for Features 8 to 12; none has a plan yet. Plans written 2026-11-16 for Features 1, 2, 3, 4, 6 and 7 (5 and 8 to 12 are planned when scheduled); nothing is built.
