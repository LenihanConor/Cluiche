# Feature Spec: S-2.5 Time-Series Graphs

**Parent:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md (Feature 5)
**Status:** Approved (2026-11-16)
**Depends on:** S-2.5 Feature 1 (stage, card, tracked-layer config); Feature 3 (the runner and report); A4 Feature 5 (the seven layers and their schema entries); DiaCellHistory (aggregates, `cells.history.aggregates`, `cells.history.export`; built); S-3.5 Feature 1's `HistoryDriverModule` (the sampler; **soft**, see Open Design Question 1); S-1's region layer flagged as a grouping layer.
**Consumed by:** T-1 Feature 7 (the Inspector, which draws the curves; proposed, not specced), anyone reading `dia.cell_history` files.

## Summary

S-2.5's weekly time series, with **no in-game graph panel** (decided 2026-11-16, matching DiaCellHistory's and A1's decision: the in-game history panel was never built and graphs live in the CluicheEditor Inspector, T-1). This feature makes the series *exist, be correct and be reachable*: DiaCellHistory aggregates for every weather layer, grouped by world and by S-1 region, over the whole run; a one-click history export from the S25 card; and a history export written by every headless run, so the curves behind Tests 4, 5 and 7 can be opened in T-1 and compared between runs.

It replaces the system spec's S-2.5-private 4 × 52-week aggregate ring, which is **not built**.

## Goals

1. For a live S-2.5 session, `cells.history.aggregates` returns one sample per applied week for all seven weather layers, in the `world` group and in each S-1 region group, for the whole run.
2. The aggregate values equal the committed fields' min, mean and max for that week.
3. The S25 card exports the current history to `out/CoW/s25/` in one action, through the existing `cells.history.export`.
4. Each headless run that feeds a check writes a `dia.cell_history` file, so a failing check's curve can be inspected and two runs (A vs B, edge on vs off) compared in T-1.
5. S-2.5 holds no aggregate store of its own.

## Non-Goals

- Drawing graphs in-game, in any form (decided 2026-11-16).
- The Inspector's graph panel and run-vs-run view (T-1 Feature 7, proposed).
- New recording machinery: DiaCellHistory's recorder, budgets and file format are used as built.
- Changing which checks decide the verdict. Curves explain; Feature 2's checks decide.
- Snow coverage percentage as a series: only the `snowPack` mean/min/max aggregate exists. (Coverage was an S-2.5 helper in Feature 2 for the checks, not a recorded layer.)

## Design

### Live: what gets recorded

- **Sampler.** `HistoryDriverModule` (S-3.5 Feature 1) samples once per applied world step (`OnWorldStepApplied`, labelled by `stepDay`). S-2.5 declares it in its manifest as Feature 1 already lists. Without it nothing is sampled and the card says so.
- **Aggregates.** DiaCellHistory records min, mean and max per layer per group for **every** layer registered with `CellLayerRegistry`, whole run, with no tracking needed (aggregate tier). For S-2.5 that is `weather.moisture`, `.pressure`, `.temperature`, `.precipitation`, `.snow_pack`, `.cloud_cover`, `.ground_water` (A4 Feature 5) plus S-1's layers.
- **Groups.** `world` always. Per-region groups need S-1's region category layer to carry `history.group: true` in its schema `ui`. A first-task check confirms S-1's registration does; if it does not, this feature's dependency is an S-1 schema one-liner (flagged, not assumed).
- **Per-cell tracking** stays Feature 1's three layers (`weather.precipitation`, `.temperature`, `.ground_water`) for the scrub window and step-back. Per-cell is not needed for curves.
- **Budget.** Aggregates are about 12 bytes per layer per group per sample: 7 weather layers × (1 + up to 64 regions) groups ≈ 5.5 KB per week, so four years (208 weeks) is about 1.1 MB against a 64 MB aggregate budget. A test asserts the arithmetic from the real group count.

### Export (card button and command)

The S25 card gains **Export history**, which posts `cells.history.export path=out/CoW/s25/live_<world>_<seed>.history.json` (an existing DiaCellHistory command; this feature adds no command name). It is disabled with the reason "no history driver" or "no history yet". The file is the standard `dia.cell_history` (layers' schema embedded, groups, aggregates, events, tracked), so T-1 reads it with no S-2.5 knowledge.

### Headless history export

For runs R1, R2, R3, R4, R5a, R6 and R7 (Feature 3's table), when `export_history` is set (default **on** in the `SLOW_` test's output directory and for `cow.s25.run`), the runner writes `out/CoW/s25/history_<run>_<world>.json`.

- A `TraceHistoryWriter` feeds a **standalone** `CellHistoryRecorder` (no module, no bus) one sample per trace week from the `RecordedTrace`, with the same layer registrations, schema entries and groups as live, then calls its export. The first plan task confirms the recorder can be constructed and sampled outside a module and registry; **if it cannot, this part is dropped** and the headless side reduces to the report (the live export is unaffected), recorded as a deviation rather than building recorder changes (AD-001).
- The week label uses the same calendar as live (`Y<year> W<week>`), so a headless file and a live file line up in T-1.
- Writing is atomic (temp then rename) and its size is bounded by the aggregate-only tier (no per-cell rings are written).

### What is deleted from the system spec

The system spec's "`S25WeatherModule` records weekly aggregates ... into a bounded ring of 4 × 52 weeks" and its S-2.5-local graph panel are removed. The Feature 1 card shows no graph. The live checks' 65-week ring (Feature 4) is for check evaluation only and is not exposed as a series.

### Instrumentation

Info log on export (path, weeks, groups, bytes); Warning when the region layer is not a grouping layer (once per world) and when export is requested with no history; Error on a failed write. `DIA_TRACE_ZONE` around the export. No new metric.

## Public API

New: `TraceHistoryWriter.h/.cpp` in `Cluiche/CoW/Modules/` (S25 prefix), the card button wiring in `S25WeatherModule`. No new command names. Edited: Feature 3's runner (`export_history` option and file paths), the system spec (deleted ring).

## Acceptance Criteria

### Live
1. After 52 applied weeks with a history driver, `cells.history.aggregates` returns 52 samples, each with the seven weather layers in the `world` group; with a grouping region layer, each sample also carries the region groups.
2. For every week and layer, the aggregate min, mean and max equal the committed field's min, mean and max computed in double and rounded to float32 (the recorded value), for the `world` group and for at least one region.
3. A run of 208 applied weeks stays under the aggregate budget, with the measured bytes per sample within 10% of the stated estimate.
4. The scrub window and step-back (Feature 1) are unaffected: the three tracked layers still scrub 13 weeks within their 16 MB budget with aggregates on.
5. Without `HistoryDriverModule`, nothing is recorded, the card says "no history driver", and **Export history** is disabled with that reason; no error is logged per frame.

### Export
6. **Export history** writes a valid `dia.cell_history` file at the stated path containing the seven weather layers' schema entries (labels and ranges), the groups and the aggregates; re-reading it with DiaCellHistory's loader gives the same aggregates.
7. Exporting twice with the same name overwrites atomically; a failed write leaves any previous file intact and logs an Error.

### Headless
8. Each listed run with `export_history` writes `history_<run>_<world>.json` that loads in DiaCellHistory, with one sample per recorded week and the same layer ids and groups as live.
9. The headless file's aggregates equal the `RecordedTrace`'s min/mean/max per week (the same arithmetic as criterion 2), and the labels follow the live calendar.
10. R1 and R2 files are identical in aggregates (executor A vs B), as Test 9 requires of the fields; R3's differs from R1's only in the edge-sensitive layers.
11. If the standalone recorder cannot be constructed outside a module, the feature records the deviation, the runner skips the export with one Info log, and no run fails because of it.

### Cleanliness
12. No S-2.5 type owns an aggregate ring or series store; a search of the S-2.5 sources finds none. (Spec-level check, done in the gap audit.)

## Files Touched

New: `TraceHistoryWriter.*`; tests `Tests/GoogleTests/CoW/S25/{TestTraceHistoryWriter,TestS25HistoryAggregates}.cpp`. Edited: `S25WeatherModule` (button), the runner (`export_history`), vcxproj via `dia docs vcxproj-add`. **Spec edits on approval:** the system spec's Time-series graphs section (replaced by a pointer to this spec) and its Feature 5 row; the backlog row noting the in-game graph is not built and T-1 is the viewer. No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| CH-001 | Aggregates and events always recorded; per-cell opt-in | Curves come from the aggregate tier; no extra tracking |
| CH-004 | Live and file use one format | Live export and headless export are both `dia.cell_history` |
| AD-001 | Ask permission before Dia edits | None; the standalone-recorder use is checked first and dropped if it needs a change |
| AD-002 | Four-tier dependency model | Feature-local S25 code; nothing shared |
| Tool tiers | Editor/Inspector/Visual Debugger are separate | Graphs live in the Inspector (T-1), not in the in-game debugger |
| PD-004 | No STL in public APIs | Writer uses Dia containers; jsoncpp only in DiaCellHistory's exporter |

## Test Plan

Live aggregates over a real A4 module on hand-built terrain with a real `CellHistoryRecorder` and a scripted `OnWorldStepApplied` driver (no stage needed); the export and loader round trip on temp files; the headless writer against a `ScriptedTrace`. TDD order: aggregate equality, budget arithmetic, export, the writer. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify.

## Open Design Questions

1. **Nobody can see the curves on screen yet.** T-1 Feature 7 is proposed, not specced, so until it exists the series are only JSON (`cells.history.aggregates`, exported files). Tests 4, 5 and 7 are decided by numbers, so the verdict is not blocked, but the "understand the seasons by looking at them" goal in the system spec is deferred to T-1. If that is too long a wait, a minimal in-game plot is the obvious fallback, and would reopen this decision.
2. **`HistoryDriverModule` again.** As in Feature 1, everything live here depends on it (S-3.5 Feature 1). Without it only the headless exports work.
3. **Region groups depend on S-1.** If S-1's region layer is not a grouping layer, only the `world` curve exists. That is a one-line S-1 schema change, but it is an edit to a built system and would need your say.
4. **Standalone recorder.** The headless export assumes `CellHistoryRecorder` runs outside a module. If it does not, the headless exports are dropped (criterion 11) and a failing check is debugged from the report's numbers and a live re-run.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Writer fill path.** `TraceHistoryWriter` builds a private `CellLayerRegistry` holding real `CellField`s (one per weather layer, sized from the terrain, with the same schema keys as live) and a `CellGridDesc` from the terrain dimensions; per trace week it copies the trace values into the fields and calls the recorder's `Sample`. It uses no module and no bus.
2. **Group cap.** DiaCellHistory allows up to 127 non-world groups; S-1's `kMaxRegions` is 64. The budget estimate uses 64 and the test uses the real group count.
3. **Aggregate arithmetic (criterion 2).** The expected value is the mean accumulated in `double` in ascending cell order and rounded to `float32`. Plan Task 1 confirms the recorder's own arithmetic; if it accumulates in `float`, the criterion compares within one float32 ULP and records that.
4. **Criterion 10.** R1 and R2 aggregates are identical; R3's aggregates **differ from R1's** in at least `weather.moisture` (the layer the edge toggle acts on directly). Which other layers differ is not asserted (downstream effects).
5. **Mockup path** is Feature 1's amendment 1.

## Status

**Status:** `Approved` (2026-11-16). Plan: [time-series-graphs.plan.md](time-series-graphs.plan.md) (written 2026-11-16, not started).
