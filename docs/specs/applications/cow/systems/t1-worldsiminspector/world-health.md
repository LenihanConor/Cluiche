# Feature Spec: T-1 World Health Dashboard

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md (Feature 11, amendment 2026-11-16)
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/correlation-and-run-compare.md (Feature 5, Pearson with lag and Source B), @docs/specs/applications/cow/systems/t1-worldsiminspector/seasons.md (Feature 8, calendar), @docs/specs/applications/cow/systems/t1-worldsiminspector/space-over-time.md (Feature 10, latitude bands)
**Design intent:** GDD `10_PrototypeStrategy/S3_LivingEcology.md` (success criteria 2 and 5), `08_SystemsDeepDive/A1_Ecology/A1_EcologySystem.md` §4 (ecology states), `A1_PrototypeTestbed.md` (prey-predator lag, extinction behaviour), `A4_Weather/A4_WeatherSystem.md`

**Gameplay Domains:** debug

## Summary

A dashboard that says **whether the world is alive and behaving**, not just what its layers read. Each measure:
- is derived in the editor from data DiaCellHistory already records;
- is defined in one data file with a description, units and good / watch / bad bands;
- names the GDD section its target comes from, or is marked **provisional**.

Each measure shows its value over the brush with its band, plus a rolling-window sparkline that shows *when* it changed. Clicking the sparkline moves the brush there. There is a per-region table where the measure allows it, and an A vs B view with the difference when a second source is open. There is no overall score.

## Problem

The S-3 success criteria ask whether booms, crashes and recovery emerge, and whether the world runs for centuries with "no drift, corruption, unexpected equilibria". Today that is judged by eye from raw curves, one layer at a time, with no agreed numbers. Tuning a pack or a rule needs a quick, repeatable answer to "is this world healthier or sicker than before, and where?"

## Goals

1. **Four measure groups:** ecology core, dynamics, weather health and coupling (table below).
2. **Explained and owned:** every measure's label, unit, description and bands come from `health_measures.json`, shown through `ValueLabel` / `ValueTooltip`. Every band cites its GDD source or says "provisional".
3. **When, not just how much:** a rolling-window sparkline per measure across the brush. Clicking it moves the brush.
4. **Where:** a region table for measures that support it, sortable, with click-to-navigate (Feature 6 `group` intent).
5. **Compare:** with Source B set, A, B and Δ per measure, each coloured by its own band.
6. **Honest coverage:** a measure whose inputs are missing (A3 not built, layer untracked, too few years) shows "not available" with the reason, never a guessed value.

## Non-Goals

- An overall world score (rejected 2026-11-16: the weights would be arbitrary and the score would hide which part is unhealthy).
- Exploring cause chains or scanning all metric pairs. That is Feature 12; this feature scores a fixed set of pairs.
- Writing health results into a report for S-3.5 regression gating (kept for later, see Open Design Questions).
- Editing GDD targets. Number changes go through the GDD session (feedback rule).
- New game-side data. Everything is derived from existing layers and events.

## Measures (v1)

| Id | Group | Measure | Inputs | Region table | Target source |
|---|---|---|---|---|---|
| `eco.state_mix` | Ecology core | Share of cells Thriving / Stable / Stressed / Collapsed; banded on the Collapsed share | `ecology.state` category counts | Yes | A1 §4 states; S-3 criterion 2. Band numbers provisional |
| `eco.biome_diversity` | Ecology core | Shannon diversity of biome cell counts, also shown as a share of its maximum | Biome category layer counts | Yes | Provisional |
| `eco.biome_turnover` | Ecology core | Cells that change biome per decade | `cow.succession.transition` events (A3), else biome count deltas (labelled "net only") | Yes | Provisional |
| `eco.near_extinction` | Ecology core | Groups (and recorded species) whose world or region mean stays below a floor for N samples; listed by name | `ecology.group.<tag>`, `ecology.species.<id>` | Yes | A1 testbed extinction behaviour; floor provisional |
| `dyn.boom_bust_period` | Dynamics | Dominant cycle length (the autocorrelation peak between 1 and 20 years) and its strength, for a chosen series (default `ecology.group.herbivore`) | Any series | Yes | S-3 criterion 2 (booms and crashes emerge). Provisional |
| `dyn.settle_time` | Dynamics | First sample after which the rolling variance (2-year window) stays under the threshold; "never settles" is a valid result | Any series | Yes | Provisional |
| `dyn.drift` | Dynamics | Slope of the rolling mean across the brush, per decade, relative to the mean | Any series | Yes | S-3 criterion 5 (no drift) |
| `wx.dry_weeks` | Weather health | Share of weeks where a region's mean precipitation is below the dry threshold (default 0.02, A4 `dry_threshold`) | `weather.precipitation` region aggregates | Yes | A4 drought threshold; band provisional |
| `wx.storm_rate` | Weather health | Storms per year | `weather.storm` events | No | Provisional |
| `wx.snow_line` | Weather health | Southernmost latitude band with mean snow pack above a level at that winter's peak week, per winter | `weather.snow_pack` latitude-band aggregates (S-1 amendment 7) | No (it is spatial already) | Provisional |
| `wx.rain_variability` | Weather health | Coefficient of variation of yearly precipitation totals | `weather.precipitation` aggregates | Yes | Provisional |
| `cpl.rain_producers` | Coupling | Lag (weeks) and r of the strongest correlation from precipitation to producers | `weather.precipitation`, `ecology.producers_total` | Yes | Provisional |
| `cpl.prey_predator` | Coupling | Lag and r from herbivore group to predator group | `ecology.group.herbivore`, `ecology.group.predator` | Yes | A1 testbed: "predator starvation follows prey collapse with ~1 year lag" |
| `cpl.snow_groundwater` | Coupling | Lag and r from snow pack to ground water | `weather.snow_pack`, `weather.ground_water` | Yes | A4 melt-flood behaviour; provisional |

The coupling measures use Feature 5's lagged Pearson (lags 0 to 104 weeks). They score fixed pairs only; Feature 12 explores.

## Architecture

```
UI/health/health_measures.json    envelope cow.t1_health_measures v1
  measures[{ id, group, label, unit, description, params{...}, bands{ good, watch, bad, direction },
             window, regionTable: bool, source: "<GDD path §>" | "provisional" }]
model/health/                     (pure TS)
  registry.ts     id -> compute(inputs, params) ; validates the file against the registry
  inputs.ts       resolve(source, measure) -> inputs | { unavailable: reason }
  measures/*.ts   one pure function per measure id
  rolling.ts      rolling(measure, series, window, stride) -> points
  band.ts         classify(value, bands) -> 'good' | 'watch' | 'bad'
  compare.ts      compareSources(a, b, measures) -> rows { a, b, delta, bandA, bandB }
store/  health { enabled[], seriesChoice, regionLayer }
views/  HealthDashboard, MeasureCard (value, band, sparkline), RegionHealthTable, HealthCompare
```

- Every measure is a pure function, and the file supplies its parameters and bands. A new threshold is a data change. A new kind of measure is a code change plus a file entry, and the loader rejects file entries the registry doesn't know.
- Views call only `model/`, the store and Feature 6's navigation model (UI/business separation rule).

### Bands and sources

- Each band has a direction (`higher_is_better`, `lower_is_better`, or `in_range`) and two cut points.
- `source` names the GDD file and section. "provisional" bands are drawn with a dashed outline and a "provisional target" tooltip.
- When a provisional number is replaced, the change goes through the GDD session first (fold-back text with the proposed number), then into this file.

### Rolling sparkline

- Each measure has a `window` (for example 2 years for the state mix, 10 years for the boom-bust period) and is recomputed every `stride` samples across the brush.
- Points are coloured by band. Clicking a point sets the brush to that window.
- A window longer than the brush shows one point and says so.

### Coverage rules

- A missing input layer, event type or grouping layer marks the measure unavailable with the reason (for example "needs A3 transition events", "latitude bands need S-1 amendment 7").
- Too little data gives "needs at least N years" (the boom-bust period needs 3 cycles of the longest lag tried).
- **Species beyond the layer cap:** near-extinction always covers functional groups, and covers species only where their layers are recorded. The card lists the recorded species and, when the schema carries the roster size, says "N of M species covered".

### Logging and observation

Channel `[EDITOR] T1`. Logged: measures file load (count, rejected entries), unavailable measures (id, reason), compute time per measure on a full recompute, and compare runs. Dev builds add `performance.mark` around the full dashboard recompute.

## Acceptance Criteria

1. `health_measures.json` loads and validates. An entry with an unknown id, a missing band, a missing source, or an id the registry lacks is rejected with the reason, and the rest still load.
2. All 14 measures in the table compute on a fixture source and match hand-computed values. Specifically: Shannon over known counts, the autocorrelation peak of a known sine, the settle time of a step-then-flat series, the drift slope of a known ramp, the lag and r of a known shifted pair.
3. Each card shows the value over the brush, its band colour, its unit and description, and its source. Provisional bands are visibly marked.
4. The rolling sparkline recomputes on the measure's window and stride. Clicking a point sets the brush to that window.
5. Measures flagged for the region table appear in a sortable region table. Clicking a row raises a Feature 6 `group` intent.
6. With Source B set, each measure shows A, B and Δ, each value coloured by its own band. Measures available in only one source say so.
7. A measure with missing inputs or too little data shows "not available" and the reason, never a value.
8. `eco.biome_turnover` uses A3 transition events when present, otherwise biome count deltas labelled "net only".
9. `eco.near_extinction` always covers groups, lists the recorded species it covers, and shows "N of M species" when the roster size is known.
10. Coupling measures use the same lagged Pearson as Feature 5, with lags from 0 to 104 weeks, and report lag and r.
11. There is no overall score anywhere.
12. Perf: a full recompute of all 14 measures over a 40-year weekly run with 30 groups finishes in under 500 ms off the UI thread (a worker), and changing the brush updates the cards within 1 second.

## Test Strategy

- **Vitest (model/):**
  - file validation and rejection reasons
  - each measure against a crafted fixture (AC 2)
  - band classification for all three directions
  - rolling windows and edges
  - coverage reasons
  - compare rows with a missing side
  - turnover fallback
- **React Testing Library:** cards with provisional marking, sparkline click moving the brush, region table sort and click intent, compare view, unavailable states.
- **E2E:** a stub game with a fixture history covering ecology and weather layers. The dashboard shows all measures, a region row click sends the `group` navigation commands.
- **Perf:** AC 12 on a generated 40-year fixture.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code and the measures file in `Cluiche/CoW/Editor/WorldSimInspector/` |
| CoW AD-003 | GDD owns design intent | Bands cite GDD sections or are marked provisional. Number changes go through the GDD session first |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| Tool tiers (Inspector) | Inspector reads, never tunes | Reads only. Thresholds are Inspector config, not simulation parameters |
| UI/business separation | Typed model first | `model/health/`. Views render results only |
| T-1 Goal 6 (every number explained) | Labels, units and tooltips from schema | Measure descriptions use the same `ValueLabel` / `ValueTooltip` shape, read from the measures file |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| T-1 Features 1, 5, 6, 8 | Sources, lagged Pearson and Source B, navigation, calendar | Approved, not built |
| T-1 Feature 10 + S-1 amendment 7 | Latitude bands for `wx.snow_line` | Approved, not built |
| A1 (`ecology.state`, groups, species, producers) | Ecology measures | Approved, not built |
| A3 (`cow.succession.transition`) | Turnover (falls back without it) | Approved, not built |
| A4 Feature 5 (`weather.*`, `weather.storm`) | Weather measures | Approved, not built |

Built against fixtures first. Each measure lights up as its system lands.

## Open Design Questions

1. **Most targets are provisional.** Only state names, "no drift", "booms and crashes emerge" and the ~1-year prey-predator lag come from the GDD. The rest need numbers. Proposed route: after the first real S-3 long runs, draft numbers from what a healthy-looking world measures, then send fold-back text to the GDD session. Until then every band is dashed.
2. **`wx.dry_weeks` from aggregates is approximate.** A region whose *mean* precipitation is under the threshold isn't the same as its cells being dry. The exact version needs per-cell data and only exists inside the per-cell window. Accept the approximation (labelled), or offer an exact mode inside the window.
3. **Feeding S-3.5 regression.** These measures look like natural gates (no drift, no collapse) for S-3.5 R-tests and S-4's Live-mode gate. Pulling them into the runners would need the measures in C++ or a shared definition. Not in v1. Decide when S-3.5 Regression mode is planned.

## Status

`Approved` (2026-11-16). No plan. The views are drawn in the new world-sim mockup (owed, see the parent amendment) before UI work.
