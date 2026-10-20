# Feature Spec: DiaCellHistory Scrub and Timeline

**Parent:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Builds on:** @docs/specs/applications/dia/systems/diacellhistory/recorder-and-tiers.md (Feature 1), @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md (DiaCellLayers Feature 1)
**Mockup:** @docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html (timeline)

**Gameplay Domains:** debug, tools

## Summary

Lets the viewer show the past. `cells.history.view <sample>` swaps the viewer's base values for the recorded sample, `cells.history.live` returns to live, and a timeline card shows the whole run with event markers, band ticks, gaps and the current position. Queries, the selection inspector and hover all follow what the viewer shows. Nothing touches the simulation.

The feature also defines the hook that makes this possible: an optional **view value source** that DiaCellLayers consults when it reads a layer. DiaCellHistory installs one; DiaCellLayers owns the interface.

## Problem

The recorder stores a run, but nobody can look at it. To find out why a crash happened you need to move back to year 12, see the map as it was, select the cells that mattered and read their numbers, all without stopping or changing the simulation. Doing that through one shared viewer means the picker, legend, query and inspector work on past data for free.

## Goals

1. **View a past sample** of the active layer on the map, with no change to the simulation.
2. **Return to live** in one action, and never confuse the two: a banner states when the view is not live.
3. **Navigate the run:** drag the slider, step by sample or by band, jump to the previous or next event.
4. **A timeline** of the whole run with events, band ticks, gaps and the retained per-cell range.
5. **Everything follows the view:** hover, query, inspector and `cells.layer.values` show the viewed sample's values.
6. **Honest gaps.** A layer with no per-cell data for the sample shows an explanation, never invented values.

## Non-Goals

- Playback (auto-advance); only manual stepping in v1.
- Snapshots, restore (Feature 3), delta views (Feature 4), export (Feature 5).
- Showing past data for layers other than via the recorded rings (no group-mean maps).
- Rewinding or pausing the simulation (the game's speed controller does that).
- Editing history.

## Architecture

Lives in `Dia/DiaCellHistory/` (namespace `Dia::CellHistory`). The interface below lives in `Dia/DiaCellLayers/` (namespace `Dia::CellLayers`).

```
IViewValueSource (DiaCellLayers)   optional hook: values the viewer shows instead of live
HistoryViewState                    live | sample N ; set by the commands
HistoryValueSource                  implements the hook from the recorder's CellValues()
TimelineModel                       what the card shows: range, buckets, ticks, gaps, position
HistoryCommands                     view, live, step, event
```

### The hook (DiaCellLayers)

```cpp
class IViewValueSource {
public:
    virtual ~IViewValueSource() = default;
    // true: out holds the values to show; false: no data for this layer in the current view
    virtual bool GetValues(StringCRC layerId, float* out, uint32 count) = 0;
    virtual bool IsLive() const = 0;          // true: the viewer reads live values itself
    virtual uint32 ViewEpoch() const = 0;     // changes whenever the view changes (sample or mode)
};
```

- DiaCellLayers' `LayerReader` and the viewer's uploader ask the installed source first. While `IsLive()` they use the layer's own `read`. While not live they call `GetValues`; if it returns false the layer has **no data** in that view (the map shows the "no per-cell history" state), never a fallback to live values.
- Cached reads are keyed by the layer's `version()` **and** `ViewEpoch()`, so a view change re-reads once.
- At most one source is installed; installing a second is rejected with an Error.
- This is the single change DiaCellLayers needs; its Features 1 and 2 are amended to consult the hook (below).

### View state and commands

| Command | Effect |
|---|---|
| `cells.history.view <sample>` | Show recorded sample `<sample>` (an index from 0 to the latest). Enters scrub mode and stays on that sample as new ones arrive |
| `cells.history.live` | Return to live values; the view then follows the simulation |
| `cells.history.step <n> [band]` | Move `<n>` samples (negative = earlier). With `band`, move to the nearest sample whose band is at least that band (for example the previous Year start). Clamped to 0 and the latest sample |
| `cells.history.event prev \| next [type]` | Jump to the previous or next sample that has an event (optionally of one event type), from the current view (from the latest sample when live) |

Results are JSON: `{ ok, mode, sample, label }`. An out-of-range sample, an unknown type or a bad argument returns an error and changes nothing.

- A sample before the per-cell window start is accepted (aggregates exist), but the map shows the "evicted" state for tracked layers; a sample inside the window shows the recorded values.
- If the window later passes the viewed sample, the view stays and the map switches to the "evicted" state with a notice; it does not jump.
- `cells.history.view <latest sample>` is scrub mode on that sample, not live; only `cells.history.live` follows the simulation.

### What the viewer shows while scrubbed

- **Base layer:** dequantised values of the sample for a tracked layer; for a layer with no per-cell data the neutral "no per-cell history" map with a banner and a **Track this layer** button (runs `cells.history.track`; data exists from then on). The legend's observed range is computed from the viewed buffer.
- **Hover, query, inspector, `cells.layer.values`:** all read through `LayerReader`, so they show the viewed sample's values. Hover shows a "sample N" badge.
- **Selection** is unchanged by scrubbing.
- **Banner:** the viewer card shows "Viewing sample N (label) — not live" with a Back to live button whenever the view is not live.
- **Editing by systems:** edit commands that change the live simulation (for example A1's paint) should refuse while the view is not live. The registry service exposes `IsLive()` for this; the check lives in the system, not in Dia.

### Timeline card

The card draws one horizontal timeline for the whole run (all samples, since aggregates cover them), built from the recorder's read API:

- a **slider** with the current position and a live marker at the latest sample; dragging runs `cells.history.view`;
- **band ticks** and labels (for example a line at every Year start) from each sample's band;
- **event ticks**, coloured by event type from the schema (`ui.colour`), bucketed so the density is visible;
- **gaps** where aggregates, per-cell data or events were evicted, and a dimmed region before the per-cell window start;
- a **sparkline** of the active base layer's world mean across the run (aggregates), downsampled to at most 256 points;
- **step buttons** (−band, −1, +1, +band), **event buttons** (previous, next) and a **Back to live** button.

The card JSON (range, buckets, ticks, gaps, position, mode, sparkline, banner state) is added to the panel contract golden. Timeline data is rebuilt when a sample is recorded or the view changes, not every frame.

### `cells.view.state` history field

DiaCellLayers Feature 5's topic carries `history`: `null` when no source is installed, otherwise `{ "mode": "live" | "sample", "sample": <index or null>, "label": "<text or null>" }`. That shape replaces the placeholder in that spec.

### Logging and observation

- **Channel:** `HIS`.
- **Info:** entered scrub mode (sample, label), returned to live.
- **Warning:** the viewed sample's per-cell data was evicted; a second value source install attempt (Error).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on a view change (read and upload) and on the timeline rebuild.
- **Metrics:** view changes, time in scrub mode (seconds), timeline rebuild ms.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. `cells.history.view <sample>` makes the viewer show the recorded values of that sample for a tracked layer, and the simulation is not affected.
2. A scrubbed view stays on its sample when new samples are recorded; only `cells.history.live` returns to live, including after viewing the latest sample.
3. `cells.history.step` moves by samples or to the nearest sample at or above a band, clamped at both ends; `cells.history.event` jumps to the previous or next sample with an event, optionally of one type.
4. Out-of-range samples, unknown event types and bad arguments return errors and change nothing.
5. A layer with no per-cell data for the viewed sample shows the "no per-cell history" map and banner with a Track button, never live values; the Track button runs `cells.history.track`.
6. A sample before the per-cell window start shows the "evicted" state; if the window passes the viewed sample the view stays and switches to the evicted state with a notice.
7. Hover, query, the selection inspector and `cells.layer.values` show the viewed sample's values; hover shows a sample badge.
8. The banner "Viewing sample N (label) — not live" and Back to live appear whenever the view is not live.
9. The view value source hook is consulted by `LayerReader` and the uploader; a layer's cached read is re-done when `ViewEpoch()` changes; installing a second source is rejected.
10. The timeline card shows the slider, live marker, band ticks, coloured event ticks, gaps, the dimmed pre-window region, a sparkline of at most 256 points, and the step, event and live buttons.
11. Dragging the slider runs `cells.history.view`; the timeline is rebuilt only when a sample is recorded or the view changes.
12. The registry service's `IsLive()` is false while scrubbed.
13. `cells.view.state.history` reports `null`, or `{mode, sample, label}`.
14. Switching the view costs at most one read and one upload at Large.
15. The timeline card JSON is in the panel contract golden, and the page renders it.
16. Visual match against the mockup's timeline.

## Test Strategy

- **GoogleTests (TDD):**
  - View state machine: live to scrub to live, staying put as samples arrive, evicted samples, latest-sample scrub.
  - Commands: view, live, step (samples, bands, clamps), event prev and next with type filter, error cases.
  - `HistoryValueSource` returns dequantised recorded values; returns false for untracked and evicted data.
  - Hook integration with a fake DiaCellLayers: `LayerReader` and uploader consult the source, re-read on `ViewEpoch()` change, second install rejected.
  - Query, inspector and values read the viewed sample (using fixtures).
  - Timeline model: buckets, ticks, gaps, sparkline downsampling.
- **Test utilities** in `DiaCellHistory/Testing/`: a recorder pre-filled with fixture samples, a fake view value source, a timeline assertion helper.
- **Panel contract golden** and page tests for the timeline card.
- **Perf:** view change at Large (one read, one upload); timeline rebuild with 2,080 samples.
- **CoW proof (S-3, from the system spec):** scrub to year 5 and see the recorded map.
- **Visual gate:** S-3 testbed against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-002 | Going back is a view-only scrub; no re-simulation | The hook swaps displayed values only; the simulation is untouched |
| CH-005 | Generic: labels, groupings come from the game | Labels and bands come from the recorder |
| CL-005 / CL-006 (DiaCellLayers) | Generic; no singletons | The hook is an interface installed through the registry service |
| Editor actions via DiaAPI | Every action is a command | `view`, `live`, `step`, `event` |
| PD-004 | No STL in public APIs | DiaCore types |
| Game logic on SimPU | View state and recorder on SimPU | Same-PU contract |
| Panel contract golden | Panel JSON and page share golden files | Timeline card added |
| AD-001 (Dia) | Edit to DiaCellLayers (the hook) | Amended in DiaCellLayers Features 1 and 2; sign-off before build |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Recorder, read API | Approved |
| DiaCellLayers Features 1 and 2 | Viewer, `LayerReader`, the hook (amended here) | Approved, not built |
| DiaCellLayers Feature 5 | `history` field in `cells.view.state` | Approved, not built |
| DiaValueSchema Feature 1 | Event colours | Approved |
| A1 Ecology | Should refuse paint while not live | Approved, not built (consumer rule) |

## Open Design Questions

1. **Edits while scrubbed.** Dia can only expose `IsLive()`; each system's edit command must check it. A1's Feature 9 (paint, `set_field`) and S-1's panel should refuse while the view is not live. Add the rule to those specs when they are planned.
2. **Bucket size on long runs.** With 2,080+ samples the slider and event density are bucketed to the card's pixel width. Choose the bucket rule (fixed count of 256 versus pixel-based) when the card is built and check events don't hide at coarse zoom.
3. **Viewing a sample while a tracked layer set changes.** Tracking a layer at sample 100 while viewing sample 40 gives that layer no data at 40. The banner explains it; check it reads clearly in a real session.

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the `IViewValueSource` hook in DiaCellLayers.
