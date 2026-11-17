# Feature Spec: T-1 Space Over Time

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md (Feature 10, amendment 2026-11-16)
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md (Feature 3, mini-map and `groupCells`), @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md (Feature 6), @docs/specs/applications/cow/systems/t1-worldsiminspector/seasons.md (Feature 8, anomaly transform)
**Requests:** S-1 consumer amendment 7 (latitude band grouping layer), A4 Feature 5 amendment (storm track and drought end events)

**Gameplay Domains:** debug

## Summary

The mini-map shows one moment. This feature shows how the world changes **across space and time together**:
- a **strip chart**: time across, space down (rows are regions or latitude bands), colour is a layer's mean, so moving drought fronts and seasonal belts show as diagonal and wavy bands;
- **maps per period**: a mini-map for each season, year or decade in the brush;
- **playback**: the mini-map animated over the brush;
- **storm and drought tracks**: storm paths and drought episodes drawn on the mini-map, clickable to jump the game there.

Where per-cell history doesn't reach (outside the per-cell window), maps fall back to **region level**, filling each region with its aggregate mean. So the whole run is always viewable, and the label says which level you're looking at.

## Problem

"Where did the drought start and how did it spread?", "Do storms track along the coast?", "Is the snow line creeping north over decades?" These are questions about movement. Curves lose the space and a single map loses the time. Today the only way to answer them is to scrub the mini-map sample by sample.

## Goals

1. **Strip chart** for any layer: x is samples in the brush, y is rows (regions or latitude bands), colour is the row's mean (min or max as an option) with the layer's ramp. Anomaly mode (Feature 8) applies. Events can be overlaid on their rows.
2. **Whole run** for the strip chart: rows are grouping layers, so it is built from aggregates, not per-cell data.
3. **Maps per period** (season, year, decade): up to 12 small mini-maps across the brush. Each is the value at the period's last sample, with a toggle for the mean over the period (inside the per-cell window only).
4. **Playback** of the mini-map over the brush at a chosen speed, with play, pause, step and loop.
5. **Region-level fallback:** outside the per-cell window, period maps and playback fill each region with its aggregate value, labelled "region level".
6. **Storm tracks:** the path of each storm as a line through its weekly centroids, with size markers and colour by minimum pressure. **Drought episodes:** each region's drought from start to end, outlined and shaded.
7. **Jump to the game:** clicking a track point or a drought episode raises a Feature 6 event intent at that week.

## Non-Goals

- A vector wind view (DiaCellLayers has no vector layer kind; wind stays an in-game overlay).
- Exporting animations (GIF or video). PNG export is already "later" for T-1.
- Tracking moving things other than storms and droughts (for example herds). Later systems can emit track events in the same shape.
- Changing per-cell history budgets. This feature works within what is recorded.

## Architecture

```
model/strip.ts       (pure TS)
  stripMatrix(source, layer, groupLayer, stat, brush, transform?) -> { rows[], samples[], values: Float32Array }
model/periods.ts     (pure TS)
  periods(samples, kind: 'season'|'year'|'decade', brush, max=12) -> [{ label, from, to, endSample }]
  coverage(period, cellWindow) -> 'cell' | 'region'
model/regionFill.ts  (pure TS)
  regionFill(groupCells, aggregates, sample, layer) -> Float32Array    each cell = its region's mean
model/tracks.ts      (pure TS)
  stormTracks(events, brush) -> [{ id, points: [{ sample, centroid, size, minPressure, anchor }] }]
  droughtEpisodes(events, brush) -> [{ region, start, end | open, weeks }]
model/playback.ts    (pure TS; injected clock)
  Playback { speed (samples/s), play, pause, step(±1), loop, frame -> sample }
store/   stripLayer, stripRows, periodKind, periodValue('end'|'mean'), playback, tracksOn
views/   StripChart (canvas), PeriodMaps (CellGridCanvas ×N), PlaybackBar, TrackOverlay (CellGridCanvas overlay)
```

- Views call only `model/`, the store and Feature 6's navigation model (UI/business separation rule).
- Per-cell values come through Feature 1's `Source.cellQuery()` (`cells.history.cells`), one sample at a time, prefetched ahead of playback.
- Cell-to-group membership (`groupCells`) is the same input Feature 3 uses for its histograms. It is read once per world and cached.

### Strip chart

- Rows: every value of the chosen grouping layer. Today that means regions (S-1 region layer) and latitude bands (S-1 amendment 7), plus any other category layer flagged as a group (biome works too, but biome membership changes over time; the row is still the biome's aggregate).
- Row order: latitude bands north to south; regions by the mean latitude of their cells (from `groupCells`), otherwise by id.
- Up to 2,080 columns (a 40-year weekly run). Above the pixel width, columns are binned by mean, and the tooltip says how many samples each pixel holds.
- **Events overlay:** events whose `group` is a row are drawn as markers on that row; storm tracks are drawn on the latitude-band rows they pass through.

### Maps per period and playback

- `periods` splits the brush on the CoW calendar (Feature 8's `parseLabel`): seasons are 13 weeks, years 52, decades 520. At most 12 maps; if more periods fit, the kind steps up (seasons to years to decades) and says so.
- **Value:** the period's last sample by default (one fetch per map). The mean toggle fetches every sample in the period, and only inside the per-cell window; outside it the toggle is disabled with the reason.
- **Coverage:** a period fully inside the per-cell window draws per-cell. Otherwise it draws `regionFill` from aggregates, with a "region level" badge.
- **Playback:** 1 to 52 samples per second (default 8), looping over the brush. Frames inside the window are per-cell, prefetched about 2 seconds ahead. Frames outside switch to region level, and the badge flips as playback crosses the boundary. A cell fetch that can't keep up holds the last frame and shows "buffering", never a blank map.

### Tracks (needs A4 amendment)

| Event | Payload | Emitted |
|---|---|---|
| `weather.storm` (existing) | gains `id` | first week of a storm |
| `weather.storm_track` (new) | `{ id, anchor, centroid: [q, r], size, minPressure }` | every applied week for each active storm, at most 8 per week (largest first) |
| `weather.drought_end` (new) | `{ region, weeks }` | the first non-dry week of a region already reported in drought |

- A storm keeps its `id` while its low-pressure region overlaps last week's region with that id (the largest overlap wins). A split gives the smaller part a new id, and a merge keeps the older id. This is the same continuity rule A4 already uses to decide that a storm is new.
- Drought end is detected on A4's drought cadence (every 4th applied week), so an end can be up to 4 weeks late. The episode tooltip says so.
- Volume: a 40-year run with 8 active storms every week is about 2.5 MB of events, inside DiaCellHistory's 16 MB event budget (normal runs are far below that).
- `tracks.ts` tolerates a missing end (open episode), a missing start (window or eviction gap), and storm tracks without a `weather.storm` start event.

### Region level needs `groupCells`

- Live: the same read Feature 3 uses for `groupCells`. The grouping layer is read through `cellQuery` once and cached per world and `structure` hash.
- File: needs the grouping layers' cells in the file. If it lacks them, region-level maps and region row ordering by latitude are disabled with the reason, while the strip chart still works.

### Logging and observation

Channel `[EDITOR] T1`. Logged: strip build (layer, rows, columns, ms), period map coverage (cell or region), playback buffering, track parsing (storms, points, open episodes, gaps), and missing `groupCells`. Dev builds add `performance.mark` around `stripMatrix` and each playback frame.

## Acceptance Criteria

1. The strip chart shows any layer with one row per group value of the chosen grouping layer and one column per sample in the brush, using the layer's ramp, over the whole run, for live and file sources.
2. Latitude-band rows are ordered north to south. Region rows are ordered by mean latitude when `groupCells` is available, otherwise by id.
3. Above the pixel width, columns are binned by mean, and the tooltip names the sample range and count.
4. Anomaly mode (Feature 8) applies to the strip chart, and min, mean and max can be chosen.
5. Events whose group is a row are drawn on that row. Storm tracks are drawn on the latitude-band rows they cross.
6. Period maps split the brush by season, year or decade on the CoW calendar, at most 12, stepping up the period kind when needed and saying so.
7. Inside the per-cell window a period map is per-cell, using the end-of-period value (one fetch) or, with the toggle, the period mean. Outside, it is region level with a badge, and the mean toggle is disabled with the reason.
8. `regionFill` gives every cell its region's aggregate value at the sample, matching a fixture.
9. Playback runs at 1 to 52 samples per second with play, pause, step and loop. It switches between per-cell and region level at the window boundary, and shows "buffering" instead of a blank map when fetches fall behind.
10. Storm tracks are built from `weather.storm` and `weather.storm_track` events: one line per id through weekly centroids, with size markers and colour by minimum pressure. Splits and merges follow the payload ids.
11. Drought episodes run from `weather.drought` to `weather.drought_end` per region. An episode without an end is drawn as open, and the tooltip notes the up-to-4-week end lag.
12. Clicking a track point or a drought episode raises a Feature 6 event intent at that sample and cell.
13. Without `groupCells` (for example, a file lacking grouping-layer cells), region-level views are disabled with the reason, and the strip chart still works.
14. Perf: a strip chart of 2,080 samples × 16 rows builds in under 50 ms and redraws at 60 fps while brushing. Playback holds 8 samples per second at Large inside the window.

## Test Strategy

- **Vitest (model/):**
  - strip matrix values and binning
  - row ordering
  - period splitting and step-up
  - coverage at window edges
  - region fill
  - storm track assembly (continuous, split, merge, missing start)
  - drought episodes (open, gap)
  - playback with a fake clock (speed, loop, step, buffering)
- **React Testing Library:** strip chart from a fixture with an anomaly toggle; period maps with mixed coverage badges; the playback bar; track overlay hover and click raising an intent.
- **E2E:** a stub game with fixture history including track events. The strip chart renders, a track click sends the Feature 6 event commands, and playback advances the frame label.
- **Perf:** AC 14 on generated fixtures.
- **Upstream (in their own plans):**
  - S-1: the latitude band layer test and a golden update.
  - A4: track continuity tests (split, merge, cap of 8) and drought end detection.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All T-1 code in `Cluiche/CoW/Editor/WorldSimInspector/`. The S-1 and A4 changes are in their own CoW modules |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` (`CellGridCanvas`) |
| Tool tiers (Inspector) | Inspector reads, never tunes | Reads only. Clicks use Feature 6 navigation |
| UI/business separation | Typed model first | `strip.ts`, `periods.ts`, `regionFill.ts`, `tracks.ts`, `playback.ts` |
| CH-005 (DiaCellHistory) | Groupings, labels and events come from the game | Latitude bands are an S-1 grouping layer and tracks are A4 events. The engine stays generic |
| A4 Feature 5 event rules | Events are schema-described, payload under 1 KB | New event types get `kind: event` schema entries. Payloads are about 100 bytes |

No conflicts. The S-1 change touches built code: CoW-side, so no AD-001 sign-off, but it changes S-1's layer goldens (see Requested amendments).

## Requested amendments (applied on approval)

- **S-1 consumer amendment 7: latitude band grouping layer.** `WorldLayerSource` registers `world.latitude_band`: a category layer with 8 equal-width bands over the map's latitude range (from the stage-11 `latitude` layer), enum ids `band_0` (north) to `band_7`, labels in degrees from the world's north and south extent, and schema `ui` `history.group: true`. It is static per world, so it is recomputed on regenerate and import. The S-1 golden file gains this layer (re-run with `DIA_UPDATE_GOLDEN=1`). The other layers' hashes must be unchanged, and the test checks that.
- **A4 Feature 5 amendment: storm track and drought end events.** As in the Tracks table, including the `id` on `weather.storm`, the continuity rule, the cap of 8 per week (excess counted in `cow.weather.storm_tracks_dropped`), and `kind: event` schema entries for both new types.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| T-1 Features 1, 3, 6, 8 | Sources, mini-map and `groupCells`, navigation, calendar and anomaly | Approved, not built |
| S-1 amendment 7 | Latitude band rows | Approved 2026-11-16, not built (S-1 itself is built) |
| A4 Feature 5 + T-1 consumer amendment | `weather.*` layers, storm and drought events, tracks | Approved, not built |
| DiaCellHistory | Aggregates, events, per-cell window | Done |

Build against fixtures first. Latitude rows and tracks light up when S-1 amendment 7 and A4 Feature 5 land.

## Open Design Questions

1. **Static grouping layers waste per-cell budget.** `groupCells` comes from reading the grouping layer per cell. If that needs the layer *tracked*, a static region or latitude layer takes a share of the per-cell window it never needs (13 KB per sample at Large). A one-shot "read this layer's current values" query, or recording static layers once, would be better. That would be a DiaCellHistory or DiaCellLayers change needing AD-001. Check what Feature 3 actually does when it is built, and request the change only if it is tracking.
2. **Eight latitude bands, equal width.** Equal-width bands put few land cells in polar bands on some presets. Bands by equal cell count would read better but have uneven, world-dependent edges. Revisit after looking at real worlds.
3. **Storm continuity in A4 vs in the editor.** A4 owning the ids keeps tracks exact and the editor simple, but it puts tracking rules into the simulation's debug path. If the rules need tuning often, tuning means rebuilding the game. Accepted for v1; revisit if the track rules change more than once.

## Status

`Approved` (2026-11-16). S-1 amendment 7 (`consumer-amendments.md`) and the A4 Feature 5 T-1 consumer amendment (`weather-debug.md`, criteria 27-31) applied the same day. No plan. The views are drawn in the new world-sim mockup (owed, see the parent amendment) before UI work.
