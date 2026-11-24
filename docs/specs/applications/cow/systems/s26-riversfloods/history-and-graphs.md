# Feature Spec: S-2.6 History & Graphs

**Parent:** @docs/specs/applications/cow/systems/s26-riversfloods/s26-riversfloods.md (Feature 5)
**Status:** Approved (2026-11-24)
**Pattern source:** @docs/specs/applications/cow/systems/s25-weatherpatterns/time-series-graphs.md (S-2.5 Feature 5)
**Depends on:** S-2.6 Feature 1 (stage, card, series, tracked-layer config) and Feature 3 (the runner and report); Feature 2's `S26WeekSummary`; A5 Feature 5 (the eight `hydro.*` layers, `kDefaultTrackedHydroLayers`, the five history event types, the weekly `hydro.totals` event); DiaCellHistory (aggregates, events, `cells.history.export`; built); S-3.5 Feature 1's `HistoryDriverModule` (the sampler; **soft**, see Open Design Question 1); S-1's region layer as a grouping layer.
**Consumed by:** T-1 Features 2 (time series) and the checks panel (S-2.6 reports, same shape as S-2.5), anyone reading `dia.cell_history` files.

## Summary

S-2.6's series, with **no in-game graph panel** (S-2.5 Feature 5, the GDD deviation). This feature makes the series *exist, be correct and be reachable*: DiaCellHistory aggregates for every `hydro.*` layer over the whole run; one **S-2.6 event** per week carrying the series the layers cannot (head and mouth flow, basin levels); the A5 `hydro.totals` event for stores and balance; flood and freeze events as markers; a one-click **Export history** on the S26 card; and a history export from each headless run so a failing curve can be opened in T-1 and two runs compared. A flood can be **scrubbed from peak back to onset**.

## Goals

1. For a live S-2.6 session, DiaCellHistory returns one sample per applied week for all eight `hydro.*` layers in the `world` group and each S-1 region group, for the whole run, and the A5 `hydro.totals` and event types alongside.
2. The series the GDD §5a lists are all reachable from the export: **water totals by store, balance error, mouth and head flow of the longest river, selected basin levels, flooded cell count, glacier mass**, with events as markers.
3. The aggregate and event values equal the committed state for that week.
4. The S26 card exports the current history in one action through the existing `cells.history.export`.
5. Each headless run that feeds a check writes a `dia.cell_history` file.
6. A flood can be scrubbed from its peak week back to its onset week with the existing step-back controls.
7. S-2.6 holds no aggregate store of its own beyond Feature 2's per-week series.

## Non-Goals

- Drawing graphs in-game, in any form.
- The Inspector's graph panel and run-vs-run view (T-1).
- New recording machinery: DiaCellHistory's recorder, budgets and file format are used as built (no scalar-series store; the Dia change A5 Feature 5 flagged is **not** proposed here).
- Changing which checks decide the verdict. Curves explain; Feature 2's checks decide.

## Design

### Live: what gets recorded

- **Sampler.** `HistoryDriverModule` samples once per applied world step (`OnWorldStepApplied`). Without it nothing is sampled and the card says so.
- **Layer aggregates (free).** min, mean and max per layer per group for every registered layer (`hydro.soil`, `.ground`, `.surface`, `.river_flow`, `.flooded`, `.ice`, `.glacier`, `.passability`), world and per region. **Flooded cell count** = `hydro.flooded` mean × cell count; **glacier mass** is `hydro.glacier` mean over the grid times cell count (the sum over eligible cells, since the layer is 0 elsewhere). The export header carries the cell count so a reader does the multiplication; no new layer is added.
- **A5 events (as built).** `hydro.totals` once per applied week (stores, in, seaOut, evaporation, balance error), plus `flood_began`/`flood_ended`, `river_ran_dry`, `lake_level_record` and `freeze_up` markers.
- **S-2.6 series event.** `s26.series`, one per applied week emitted from `S26RiversFloodsModule` after `SummariseWeek` (so it carries exactly Feature 2's numbers): `{week, head, mouth, meanFlow, basinLevels:[...], flooded, glacier}` as compact JSON under 1 KB. The event type is described in a DiaValueSchema entry (`kind: event`) so an exported file explains itself. Basin levels are limited to the **selected** basins (the dry basin, the lake basin and up to 4 more chosen in the card), default the dry and lake basins from the precheck; the full table stays in the series for checks.
- **Per-cell tracking** stays A5's `kDefaultTrackedHydroLayers` (`river_flow`, `soil`, `flooded`, `glacier`), posted by the stage at start with `cells.history.track`, for the step-back window. Curves do not need per-cell data.
- **Budget.** 8 layers × (1 + up to 64 regions) groups at ~12 bytes ≈ 6 KB per week, plus ≤ 2 KB of events, so a 100-year run (5,200 weeks) is about 40 MB against DiaCellHistory's aggregate budget; a test asserts the arithmetic from the real group count and the 5,200-week case. Where it exceeds the budget DiaCellHistory's own band rules apply (yearly when zoomed out), and the Warning below is raised.
- **Graph set (GDD §5a).** The table below lists where each graph's series comes from, so the T-1 Inspector (which draws them) needs no S-2.6 knowledge.

| Graph series | Source in `dia.cell_history` |
|---|---|
| Water totals by store (soil, ground, channels, basins, ice, glacier) and balance error | `hydro.totals` events |
| Mouth and head flow of the longest river | `s26.series` events |
| Selected basin levels | `s26.series` events |
| Flooded cell count | `hydro.flooded` aggregate mean × cell count (or `s26.series.flooded`) |
| Glacier mass | `hydro.glacier` aggregate × cell count (or `s26.series.glacier`) |
| Events as markers | flood/river/lake/freeze events |

### Scrub from peak to onset

The step-back controls (Feature 1: `cells.history.view`, `cells.snapshot.restore`) already scrub tracked per-cell layers. This feature adds the **proof** and the **jump targets**: the card lists the latest flood episode's onset week and peak week (from the `flood_began` event and the series' `floodedCells`), with "Jump to peak" and "Jump to onset" buttons that post `cells.history.view` for those weeks. The history window must retain a whole `melt_pulse` flood: **at least 26 weeks** of the four tracked layers at Small within A5's budget (a test asserts retention, as S-2.5 did for 13).

### Export (card button and command)

The S26 card's **Export history** posts `cells.history.export path=out/CoW/s26/live_<world>_<seed>.history.json` (an existing command; this feature adds **no** command name). Disabled with "no history driver" or "no history yet". The file is the standard `dia.cell_history`, so T-1 reads it with no S-2.6 knowledge.

### Headless history export

For runs H1, H2, H3, H4, H5, H7 (Feature 3's table), when `export_history` is set (default **on** for `cow.s26.run` and in the `SLOW_` test's output directory), the runner writes `out/CoW/s26/history_<run>_<world>.json`.

- A `SeriesHistoryWriter` feeds a **standalone** `CellHistoryRecorder` (no module, no bus) one sample per run week from the run's `S26WeekSummary` list and the cell layers' weekly values the runner already holds, with the same schema entries and groups as live, and emits `hydro.totals`/`s26.series` events from the summaries. The first plan task confirms the recorder can be constructed and sampled outside a module and registry; **if it cannot, this part is dropped** and the headless side reduces to the report, recorded as a deviation (AD-001).
- The week label follows the live calendar (`Y<year> W<week>`), so files line up in T-1.
- H8 (5,200 weeks) and H9 write **summary-only** files (events and the world aggregates, no per-cell tracking) to bound size.
- Writing is atomic (temp then rename).

### Instrumentation

Info log on export (path, weeks, groups, bytes); Warning when the region layer is not a grouping layer (once per world), when export is requested with no history, and when the aggregate budget forces banding; Error on a failed write. `DIA_TRACE_ZONE` around the export and the per-week `s26.series` emission; `DIA_PROFILE_SCOPE` on the emission. No new metric.

## Public API

New: `SeriesHistoryWriter.h/.cpp` and `S26SeriesEvent.h/.cpp` in `Cluiche/CoW/Modules/S26/`; the `s26.series` DiaValueSchema entry; the card's button and jump wiring in `S26RiversFloodsModule`. No new command names. Edited: Feature 3's runner (`export_history`), the system spec's graphs section.

## Acceptance Criteria

### Live
1. After 52 applied weeks with a history driver, `cells.history.aggregates` returns 52 samples with the eight `hydro.*` layers in the `world` group (and each region group when the region layer groups), and `cells.history.events` returns one `hydro.totals` and one `s26.series` event per week.
2. For every week and layer, the aggregate min, mean and max equal the committed field's, computed in double and rounded to float32 (as recorded), for the `world` group and at least one region.
3. Each `s26.series` event equals Feature 2's `S26WeekSummary` for that week field for field (head, mouth, mean flow, selected basin levels, flooded, glacier).
4. A 5,200-week run stays within the aggregate budget (or bands as DiaCellHistory specifies), with measured bytes per sample within 10% of the estimate, and the event payload is under 1 KB.
5. The tracked scrub window and step-back (Feature 1) are unaffected: the four tracked layers retain at least 26 weeks at Small within budget with aggregates and events on.
6. Without `HistoryDriverModule`, nothing is recorded, the card says "no history driver", **Export history** is disabled with that reason and no error is logged per frame.

### Scrub
7. After `melt_pulse`, the card shows the flood's onset and peak weeks equal to the series' first non-zero `floodedCells` week and its maximum week; "Jump to onset" and "Jump to peak" move the view to those weeks and the view's `hydro.flooded` equals the committed values then.
8. Scrubbing from peak back to onset steps each retained week without a gap and ends at the onset week (the system spec's requirement).

### Export
9. **Export history** writes a valid `dia.cell_history` file containing the eight layers' schema entries, the groups, the aggregates and the events (including `hydro.totals`, `s26.series`, and the flood/freeze markers); re-reading it with DiaCellHistory's loader gives the same aggregates and events.
10. Exporting twice with the same name overwrites atomically; a failed write leaves any previous file intact and logs an Error.

### Headless
11. Each listed run with `export_history` writes `history_<run>_<world>.json` that loads in DiaCellHistory, with one sample per recorded week, the same layer ids, groups and event types as live.
12. The headless file's aggregates and events equal the run's summaries (the same arithmetic as criterion 2 and 3), and its labels follow the live calendar.
13. H1 reference and the range-1/serial determinism runs give identical history aggregates (as Test 10 requires of the fields).
14. If the standalone recorder cannot be constructed outside a module, the feature records the deviation, the runner skips the export with one Info log and no run fails because of it.
15. H8 and H9 files are summary-only and under a stated size bound (asserted in the test).

### Cleanliness
16. No S-2.6 type owns an aggregate store or series store beyond Feature 2's per-week summary list; a search of the S26 sources finds none (checked in the gap audit).

## Files Touched

New: `SeriesHistoryWriter.*`, `S26SeriesEvent.*`; the `s26.series` schema entry; tests `Tests/GoogleTests/CoW/S26/{TestS26SeriesEvent,TestSeriesHistoryWriter,TestS26HistoryAggregates}.cpp`. Edited: `S26RiversFloodsModule` (button, jumps, event emission), the runner (`export_history`), vcxproj. **Spec edits on approval:** the system spec's Panel graphs bullet (pointer here, the series source table). No Dia or Draoi files.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| CH-001 | Aggregates and events always recorded; per-cell opt-in | Curves come from the aggregate and event tiers; per-cell stays A5's four tracked layers |
| CH-004 | Live and file use one format | Live and headless exports are `dia.cell_history` |
| AD-001 | Ask permission before Dia edits | None; the standalone recorder is checked first and the export dropped if it needs a change; no scalar-series store is proposed |
| AD-002 | Four-tier dependency model | Feature-local S26 code |
| Tool tiers | Editor/Inspector/Visual Debugger are separate | Graphs live in the Inspector (T-1), not the in-game debugger |
| PD-004 | No STL in public APIs | Writer uses Dia containers; jsoncpp only in DiaCellHistory's exporter |

## Test Plan

Live aggregates and events over a real A5 module on hand-built terrain with a real `CellHistoryRecorder` and a scripted `OnWorldStepApplied` driver (no stage); the export and loader round trip on temp files; the headless writer against `ScriptedSeries`. TDD order: event equality, aggregate equality, budget arithmetic, jumps, export, writer. Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify.

## Open Design Questions

1. **Nobody can see the curves on screen yet.** Until T-1's graph features exist the series are only JSON. The verdict is numeric so it is not blocked, but "understand water by looking at it" is deferred to T-1. A minimal in-game plot is the obvious fallback and would reopen the S-2.5 decision.
2. **`HistoryDriverModule` again** (S-3.5 Feature 1). Everything live depends on it, as in S-2.5. Without it only the headless export works, and the scrub-from-peak criteria (7, 8) are gated on it.
3. **`s26.series` duplicates some numbers** already derivable from layer aggregates (flooded count, glacier mass) so a reader does not need the cell count. If you would rather not record the same value twice, drop the two fields and keep the multiplication in T-1.
4. **26 weeks of per-cell retention** may not fit A5's 16 MB budget at the four tracked layers; criterion 5 measures it. If it does not, either raise the budget or scrub with the aggregate and event view only (no per-cell flood map), which weakens the "scrub a flood" requirement.

## Status

**Status:** `Approved` (2026-11-24). No plan. Blocked on Feature 1 and A5 Feature 5.
