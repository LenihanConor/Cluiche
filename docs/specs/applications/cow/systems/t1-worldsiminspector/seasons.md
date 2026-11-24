# Feature Spec: T-1 Seasons (Typical Year and Anomaly)

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md (Feature 8, amendment 2026-11-16)
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/time-series-and-events.md (Feature 2), @docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md (Feature 3)
**Consumed by:** @docs/specs/applications/cow/systems/s25-weatherpatterns/time-series-graphs.md (S-2.5 has no in-game graphs; this is where its seasons are seen)

**Gameplay Domains:** debug

## Summary

Weather is a strong yearly cycle with weekly noise, so a 40-year raw line shows mostly noise. This feature adds:
- a **typical-year chart**: x is week of year, the line is the mean across years, the band is the spread (p10-p90 by default, min-max on a toggle);
- an **anomaly mode**: any series, breakdown, correlation or mini-map can show *value minus the typical value for that week of year*;
- a **reference baseline**: by default the run's own typical year, or the typical year of another history file;
- a built-in **weather view** (precipitation, temperature, snow pack, water to ground (amended 2026-11-17, A5 Hydrology): A4 has no ground water now; A5's `hydro.ground`, `hydro.soil` and `hydro.river_flow` are candidates for a water view when A5 is built, TBD).

Everything is computed in the editor from data DiaCellHistory already records. No game or engine change.

## Problem

You can't see "is this drought unusual for the season?" or "does this pack make winters wetter?" from raw weekly curves. The seasonal cycle hides the signal. The usual answer is a typical-year chart and anomalies against a baseline, and T-1 has neither. S-2.5 decided to have no in-game graphs, so T-1 is the only place weather can be seen over time.

## Goals

1. **Typical year** for any metric (layer × group × stat), weather or ecology: mean per week of year with a p10-p90 band, a min-max toggle, and the current year (or the year at the brush end) overlaid.
2. **Anomaly mode** as a toggle on Feature 2 time series, Feature 3 breakdown and mini-map, and Feature 5 correlation. Values are shown as the difference from the baseline for the same week of year.
3. **Baseline choice:** this run (default) or a reference history file. The file's typical year is computed the same way, and its world must match for per-cell anomalies.
4. **Fair years by default:** year 1 (spin-up) and partial years are left out; the brush narrows further; a toggle includes everything. The years used are always listed.
5. **Weather view** shipped as a built-in saved view (Feature 2 saved-view format), next to an ecology view.
6. **Explained numbers:** anomaly values carry the layer's unit with a "Δ vs typical" label from DiaValueSchema, and every chart states its baseline and years.

## Non-Goals

- A4's in-game seasonal baseline (`IBiomeSeasonalBaseline`). It isn't in history files and the editor can't call game code. Using it would need a new export, and it was not taken (2026-11-16).
- S-2.5 `baseline.json`. It holds per-check magnitudes, not weekly curves; that belongs to Feature 9.
- Trend removal (detrending a run that drifts over decades). Anomalies are against a seasonal mean only.
- Day-band or Season-band typical years. Only Week-band samples are used.
- Changing what DiaCellHistory records.

## Architecture

```
model/calendar.ts   (pure TS)
  parseLabel("Y12 W7") -> { year: 12, week: 7 } | null        CoW label form, one function
  weeksPerYear(samples) -> n                                  max week seen in full years
  fullYears(samples, opts{ skipFirst: 1, brush }) -> year[]   partial years dropped
model/seasons.ts    (pure TS)
  typicalYear(series, years, weeksPerYear) -> per week { mean, p10, p90, min, max, n }
  anomaly(series, baseline) -> series of value - baseline.mean[week]
  Baseline = { source: 'self' | { fileId }, years, weeksPerYear, perWeek }
model/cellSeasons.ts (pure TS)
  sameWeekSamples(sample, years) -> sample indices at the same week in each baseline year
  cellAnomaly(valuesNow, valuesSameWeek[]) -> Float32Array (NaN-aware)
store/      seasonsMode { typicalYear, anomaly, spread: 'p10p90'|'minmax', baseline, includeAll }
views/      TypicalYearChart (uPlot), AnomalyToggle, BaselinePicker, YearsChip
```

- Views call only `model/` and the store (UI/business separation rule).
- Feature 2, 3 and 5 views take an optional `transform` from the store: in anomaly mode it is `anomaly(·, baseline)`. Those views don't learn about seasons.

### Week of year

- Samples are `{tick, label, band}` (DiaCellHistory export). Only `band = Week` samples are used.
- `parseLabel` reads the CoW form `Y<year> W<week>`. A label it can't parse drops that sample and counts it in a "samples ignored" note. It is the only place the label format is known.
- `weeksPerYear` is the highest week number seen in a full year (52 for CoW today). A year is **full** when it has every week 1..n.
- Aggregate gaps (eviction, DiaCellHistory gap markers) make a year partial for that metric only.

### Typical year (series)

- Per week of year: `mean`, `p10`, `p90`, `min`, `max`, `n` over the chosen years. Percentiles use linear interpolation between ranks.
- **Fewer than 5 years:** p10-p90 is not meaningful, so the band switches to min-max with a note "fewer than 5 years, showing min-max".
- The chart overlays one chosen year (default: the year containing the brush end) as a line.

### Anomaly (series)

- `anomaly[s] = value[s] - baseline.mean[week(s)]`. Samples whose week has no baseline (`n = 0`) are NaN and drawn as gaps.
- Axis label: `Δ <label> vs typical (<baseline name>)`, unit unchanged.

### Anomaly (mini-map)

- Per-cell anomaly at sample s needs each cell's value at the same week in each baseline year. The editor fetches **only those samples** through `cells.history.cells {layer, from, to}` (one request per year, a single sample each), instead of the whole per-cell series. 10 years at Large is about 10 × 6,656 cells, roughly 130 KB per layer, in about 9 chunks under the 15,000-byte reply cap.
- **Window limit:** per-cell history only covers tracked layers within the per-cell window (often a few years, DiaCellHistory budget). Years outside the window are skipped, and the chip shows "per-cell: N of M years". With fewer than 2 years in the window the map shows "not enough per-cell history for an anomaly" and offers to track the layer earlier.
- Drawn with the layer's delta ramp, symmetric around zero (the same ramp and scale rules as Feature 7's compare).

### Reference file baseline

- Any opened history file can be the baseline. Its typical year uses the same rules, and the chip says `baseline: <file> (years a-b)`.
- **Series:** groups are matched by id. A group missing in the reference is NaN, with a note. World-level always works.
- **Mini-map:** needs the reference file's per-cell data (exported with `--cells`) and a matching world id. Otherwise the map anomaly is disabled with the reason.

### Weather view

A built-in saved view (Feature 2 format, read-only, can be copied): `weather.precipitation`, `weather.temperature`, `weather.snow_pack`, `weather.water_to_ground` (amended 2026-11-17, A5 Hydrology): layer name set by the A4 amendment, TBD), world group, typical-year mode on. A water view (`hydro.soil`, `hydro.ground`, `hydro.river_flow`, `hydro.ice`; candidate, TBD, needs A5) may sit beside it (amended 2026-11-17, A5 Hydrology)). An ecology view sits beside it (`ecology.producers_total`, `ecology.group.herbivore`). Shipped as JSON under the plugin's `views/builtin/`.

### Logging and observation

Channel `[EDITOR] T1`. Logged: baseline changes (source, years), samples ignored by label parsing, per-cell anomaly fetch (layer, years, chunks, ms) and refusals. Dev builds add `performance.mark` around `typicalYear` and `cellAnomaly`.

## Acceptance Criteria

1. `parseLabel` reads `Y<year> W<week>` and rejects anything else. Rejected and non-Week samples are excluded and counted in a visible note.
2. With default settings the typical year leaves out year 1 and every partial year, and the years chip lists exactly the years used. The include-all toggle and the brush change the set as stated.
3. For each week of year, `mean`, `p10`, `p90`, `min`, `max` and `n` match a hand-computed fixture (including interpolated percentiles).
4. With fewer than 5 years the band falls back to min-max and says so.
5. The typical-year chart works for any metric and overlays the chosen year, defaulting to the year at the brush end.
6. Anomaly mode turns Feature 2 series, Feature 3 breakdown and Feature 5 correlation inputs into value minus baseline mean for the same week. Weeks with no baseline are gaps, not zero.
7. The anomaly axis and tooltips read "Δ <label> vs typical (<baseline>)" with the layer's unit from DiaValueSchema.
8. Per-cell mini-map anomaly fetches only the same-week samples for each baseline year, chunked under the reply cap, and matches a fixture computed from full series.
9. When the per-cell window holds fewer than 2 baseline years, the map shows the reason and offers Track; otherwise the chip shows "per-cell: N of M years".
10. A reference file can be chosen as the baseline. Unmatched groups are NaN with a note. The map anomaly is disabled with a reason when the file lacks per-cell data or its world id differs.
11. The built-in weather and ecology views open with the listed metrics in typical-year mode. They can be copied but not overwritten.
12. Everything works the same for a live source and a file source.
13. Perf: typical year for 20 metrics over a 40-year weekly run (2,080 samples) computes in under 100 ms. Toggling anomaly mode redraws within the Feature 2 brushing budget (60 fps).

## Test Strategy

- **Vitest (model/):**
  - label parsing (good, malformed, other bands)
  - full and partial year detection, including aggregate gaps
  - percentile interpolation; min-max fallback under 5 years
  - anomaly with missing weeks
  - same-week sample selection; per-cell anomaly with NaN and window limits
  - reference-file group matching
- **React Testing Library:** typical-year chart from a fixture; years chip; baseline picker; anomaly toggle changes the Feature 2 axis label; mini-map refusal states; built-in views open.
- **E2E:** with a stub game publishing 6 years of fixture weekly history, typical-year mode shows 5 years (year 1 skipped), and anomaly mode on the mini-map issues only same-week `cells.history.cells` requests.
- **Perf:** AC 13 on a generated 40-year fixture.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code in `Cluiche/CoW/Editor/WorldSimInspector/UI/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + uPlot (signed off 2026-11-05) |
| Tool tiers (Inspector) | Inspector reads, never tunes | Reads only. The one query is `cells.history.cells`, already on the read path |
| UI/business separation | Typed model first | `calendar.ts`, `seasons.ts`, `cellSeasons.ts`; other views get a transform, not season logic |
| CH-004 | Live and file data share one format | One `Source` model; a baseline can be either |
| CH-005 (DiaCellHistory) | Time labels come from the game, generic engine | The engine stays generic. T-1 is CoW-tier, so it may read CoW's `Y W` label, and only in `parseLabel` |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| T-1 Features 1, 2, 3, 5 | Source model, time series and saved views, mini-map, correlation | Approved, not built |
| DiaCellHistory (aggregates, `cells.history.cells`, export with `--cells`) | Series and per-cell data | Done |
| A4 Feature 5 `weather.*` layers | Content for the weather view | Approved, not built. Fixtures are enough to build |
| A5 Hydrology `hydro.*` layers (optional, candidate water view) | Water series and seasons | Draft, not built |
| S-2.5 Feature 5 history files | Real weather runs to open | Approved, not built |

## Open Design Questions

1. **Per-cell anomaly is thin on real runs.** The per-cell window (the DiaCellHistory budget split across tracked layers) may hold only a few years at Large. That makes map anomalies noisy or unavailable exactly where they're wanted. If it proves too short in S-2.5, the options are: a S-2.5 tracked-layer config with fewer layers, a bigger budget for weather testbeds, or per-region anomaly maps from aggregates. The last one always works but is coarse.
2. **Reference runs need a shared calendar.** Week-of-year alignment assumes both runs use 52-week years starting at W1. If a preset or world ever changes the calendar, `weeksPerYear` differs and the baseline must refuse. That is not specified beyond "refuse". Confirm when A4 is built.
3. **Is p10-p90 the right default for precipitation?** Rain is skewed, with many dry weeks. A median line might read better than the mean. It's cheap to add as a toggle once real A4 data exists. Left out of v1 until someone looks at real curves.

## Change-over-time additions (2026-11-16, user request, approved with the v2 mockup)

### Calendar heatmap

- A second view beside the typical-year chart: **weeks of the year across, years down**, colour is a metric's value (or its anomaly, Feature 8's baseline). Droughts, wet years and slow drift show as visible bands and tilts.
- Rows are the years in the chosen baseline set plus the partial years (greyed). Hover shows year, week and value; clicking a cell moves the brush to that week and raises a navigation intent (Feature 6 `event`-style history view).
- Built from the same aggregates and `typicalYear` baseline as the typical-year chart, no new data. Colour: the layer's ramp for raw values, a symmetric diverging ramp for anomalies, with a fixed or auto scale as in Feature 7.
- Up to 40 years × 52 weeks; weeks without a sample are hatched.

### Added criteria

C1. The calendar heatmap shows one row per year and one column per week of the year, for any metric, raw or as an anomaly against the chosen baseline, matching a fixture.
C2. Partial years are greyed, and missing weeks are hatched.
C3. Clicking a cell sets the brush to that week and sends the navigation intent; hover shows year, week and value with the schema unit.
C4. Perf: 40 years × 52 weeks renders in under 100 ms.

## Status

`Approved` (2026-11-16). **Amended 2026-11-17 (A5 Hydrology):** ground water leaves the weather view (`water_to_ground` replaces it); a water view from A5 layers is a candidate. No plan. The views are drawn in the new world-sim mockup (owed, see the parent amendment) before UI work.
