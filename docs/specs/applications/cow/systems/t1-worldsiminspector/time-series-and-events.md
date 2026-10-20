# Feature Spec: T-1 Time Series and Events

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1)

**Gameplay Domains:** debug

## Summary

The first real view: a uPlot time series of any aggregate (layer × group × stat) over the whole run, with typed event markers, plus a filterable event timeline. It also adds saved views and CSV export. The chart and timeline read only the Feature 1 `Source` model and the shared brush.

## Problem

The reason T-1 exists is to see how the world behaves over time and tie changes to events (droughts, storms, biome transitions, ecology crashes). Without a chart and an event list, the sources from Feature 1 are not usable, and an analysis cannot be repeated or shared.

## Goals

1. **Plot any aggregate.** Pick any layer × group × stat from the source and plot it over the run.
2. **Honest axes.** Up to 8 series; axes are grouped by unit, two at most. A third unit is refused with a message.
3. **See events.** Events are drawn as vertical markers by type, with colour and glyph from DiaValueSchema.
4. **Browse events.** A list filterable by type, group and time; clicking selects the event (navigation is Feature 6).
5. **Shared time range.** The brush from Feature 1 sets the x range; dragging on the chart sets the brush.
6. **Repeat an analysis.** Named saved views store metrics, brush, sources and layout.
7. **Take data out.** CSV export of the plotted series over the brush.
8. **Fast.** 40 years weekly (2,080 samples) × 20 layers × 30 groups plots in under 300 ms and brushes at 60 fps.

## Non-Goals

- Sending commands to the game on click (Feature 6). This feature publishes a "selected event" in the store and nothing more.
- Source B overlay and Δ (Feature 5). Series are built from a list of sources so B can be added without changing the chart.
- PNG export (deferred), events CSV, copy to clipboard.
- Zoom beyond the brush; no separate pan/zoom state.
- Editing or annotating events.

## Architecture

Files under `Cluiche/CoW/Editor/WorldSimInspector/UI/`:

```
model/
  series.ts       series(source, metric, brush) → { x: Float64Array, y: Float32Array, unit }
  axes.ts         assignAxes(metrics) → { axes[≤2], error? }   (group by unit)
  events.ts       filterEvents(events, {types, groups, range}), markerStyle(event, schema)
  savedView.ts    serialise / restore (pure)
  csv.ts          toCsv(seriesList, brush)
store/            metrics[], eventFilter, selectedEvent, savedViewName
views/
  TimeSeries.tsx  uPlot wrapper (draw hook for markers, hover crosshair tooltip)
  MetricPicker.tsx
  EventTimeline.tsx
  SavedViewsMenu.tsx
```

Views call `model/` and the store only; they never call the bridge. Saved-view and CSV file writes go through one C++ bridge request each (`writeInspectorFile`) restricted to `out/CoW/inspector_views/` and `out/CoW/inspector_exports/`.

### Metric

`{ sourceId, layer, group, stat }` where `stat ∈ min | mean | max`. Labels, units and descriptions come from the schema through `ValueLabel` and `ValueTooltip` (Feature 1).

### Axes

`assignAxes` groups the metrics by unit. One unit gives one axis; two units give a left and a right axis; a third unit returns an error and the picker rejects the add with "Only two units can share a chart".

### Event markers

- One vertical line per event at its sample, coloured and glyphed by the event kind's schema entry.
- Per-type show/hide toggles in the legend.
- Markers closer than a pixel threshold collapse into one marker showing a count; hovering lists the events inside it.
- Hover crosshair tooltip shows each series value at the sample, with unit.

### Event timeline

A virtualised list (sample label, type, group, layer, summary text from the schema). Filters: type, group, and the brush range (on by default, can be turned off). Selecting a row highlights its marker and sets `selectedEvent`.

### Saved views

`out/CoW/inspector_views/<name>.json`:

```json
{ "format": "cow.inspector_view", "version": 1,
  "name": "...", "sources": [{ "kind": "file|live", "path": "..." }],
  "metrics": [{ "layer": "...", "group": "...", "stat": "mean" }],
  "brush": { "start": "Y3 W12", "end": "Y9 W40" },
  "eventFilter": { "types": [], "groups": [] }, "layout": {} }
```

On load, anything that cannot be restored (a missing file, a metric not in the source, an untracked layer) is skipped, the rest is restored, and a message lists what was skipped. The brush is stored by sample label so it survives a different sample count.

### CSV export

One row per sample in the brush. Columns: `sample`, `label`, then one per plotted metric with the unit in the header. Written to `out/CoW/inspector_exports/<timestamp>.csv`.

### Logging and observation

Channel `[EDITOR] T1`. Logged: saved view saved/loaded/partially restored, CSV exported, rejected third unit. `performance.mark` around series build and chart draw in dev builds.

## Acceptance Criteria

1. Choosing any layer × group × stat from the active source plots it over the brush range with a correct label and unit.
2. Up to 8 series can be plotted; a ninth is refused with a message.
3. Series with one unit share one axis; two units get a left and right axis; a third unit is refused with a message.
4. Events appear as vertical markers whose colour and glyph come from the schema; toggling a type hides and shows its markers.
5. Markers inside the collapse threshold merge into one with a count, and hover lists the events inside.
6. Hovering the chart shows a crosshair tooltip with each series value and unit at that sample.
7. Dragging on the chart sets the shared brush; moving the toolbar brush redraws the chart.
8. The event timeline lists events with type, group and time filters; the brush filter is on by default and can be turned off.
9. Selecting an event row highlights its marker, and clicking a marker selects its row; `selectedEvent` is set in the store and no command is sent.
10. A saved view round-trips: save, reload the app, restore, and the chart is identical.
11. Restoring a saved view with a missing file or unknown metric restores the rest and reports what was skipped.
12. CSV export writes one row per sample in the brush with unit-labelled headers, and values match the plotted series.
13. A 2,080-sample run with 20 layers × 30 groups (8 series plotted) draws in under 300 ms and brush dragging holds 60 fps.
14. Visual match against the mockup's time series and event timeline.

## Test Strategy

- **Vitest (model/):** series extraction over a brush; axis assignment (1, 2, 3 units); event filtering; marker style from schema; collapse threshold; saved-view serialise/restore including missing parts; CSV text for a known fixture.
- **React Testing Library:** metric picker limits, legend toggles, timeline filters, selection sync between marker and row, saved views menu.
- **GoogleTests (C++):** `writeInspectorFile` writes only inside the two allowed folders and rejects paths that escape them.
- **Perf:** scripted draw of the 2,080 × 20 × 30 fixture, asserting the 300 ms budget.
- **Visual gate:** compare against `mockups/world-sim-inspector.html` before Done.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code in `Cluiche/CoW/Editor/WorldSimInspector/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` + uPlot |
| CH-004 | Live and file data share one format | The chart reads only the `Source` interface |
| System: resolved decisions 1 and 2 | Saved views and CSV in v1, PNG later | Both built here; PNG left out |

uPlot is a new npm dependency; adding it is a shared package file change, so it needs AD-001 sign-off at build time. No conflicts.

## Open Design Questions

1. **Marker density at long runs.** The collapse threshold is a pixel distance; the right value (and whether to collapse by type) is best set by looking at real event rates once A1 and A4 emit them.
2. **Stat choice.** Min, mean and max are the only stats now. Percentiles would need DiaCellHistory to record them.
3. **Saved-view paths.** Live sources are stored as `kind: live` with no path; restoring needs the game to be connected. Check that this is the right behaviour when a view is opened offline.

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the uPlot dependency.
