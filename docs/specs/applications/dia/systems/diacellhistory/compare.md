# Feature Spec: DiaCellHistory Compare

**Parent:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Builds on:** @docs/specs/applications/dia/systems/diacellhistory/scrub-and-timeline.md (Feature 2), @docs/specs/applications/dia/systems/diacellhistory/snapshots.md (Feature 3), @docs/specs/applications/dia/systems/diacelllayers/selection-inspector.md (DiaCellLayers Feature 4), @docs/specs/applications/dia/systems/diavalueschema/ramps.md (`ResolveDeltaRamp`)
**Mockup:** @docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html (delta view)

**Gameplay Domains:** debug, tools

## Summary

Shows what changed between two moments. `cells.history.delta <layer> <A> <B>` puts the viewer in **delta mode**, where each cell shows B minus A, where A and B are each `now`, `sample:<i>` or `snapshot:<name>`. The delta uses a diverging ramp that is symmetric around zero. In delta mode **every float layer** returns deltas through the view value source, so hover, queries (`vegetation < -0.1`) and the selection inspector all work on the change, and the inspector adds a per-group table of changes in the mean.

Comparing two runs is T-1's job over exported files (aggregates and events); in-game compare covers one run only (CH-003).

## Problem

A scrub shows one moment; the question is usually "what is different between then and now, or before and after the drought?". Eyeballing two maps does not find a 0.1 change on a wide-range layer, and it cannot select the cells that changed. A delta view, built on the same viewer, makes the change visible, selectable and countable.

## Goals

1. **Delta view.** B minus A for the chosen layer, drawn with a zero-centred diverging ramp.
2. **Any two sides.** `now`, a recorded sample, or a snapshot, in any combination.
3. **Everything follows the delta.** Hover, query, inspector and the editor mirror see delta values, so "select all cells that dropped by more than 0.1" works.
4. **Group summary.** The inspector shows, per group, the mean on each side and the change.
5. **Honest scale.** The colour scale is symmetric around zero and shown in the legend; it can be fixed to compare two deltas on the same colours.
6. **Honest gaps.** A layer with no data on a side shows none, never a delta against live values.

## Non-Goals

- Run-versus-run compare (T-1 over exported files).
- Delta of category layers (see Category layers); no "changed" map in v1.
- Three-way or multi-sample comparisons, animation between two states.
- Exporting a delta layer.
- Editing anything in delta mode.

## Architecture

Lives in `Dia/DiaCellHistory/` (namespace `Dia::CellHistory`). It extends Feature 2's view value source and adds one optional method to the hook in DiaCellLayers.

```
HistoryViewState        Live | Sample(n) | Delta(A, B, scale)       (Feature 2 state machine extended)
DeltaValueSource        implements IViewValueSource for delta mode: GetValues = B - A
SideReader              reads one side for a layer: now | sample (ring) | snapshot (cell file cache)
SnapshotCellCache       loads snapshot cell files on demand (LRU, 2 snapshots)
GroupDeltaTable         per-group means and change for the inspector
```

### The hook addition (DiaCellLayers)

Feature 2's `IViewValueSource` gains one optional method, so a source can tell the viewer how to colour what it returns:

```cpp
struct ViewPresentation { StringCRC rampId; float rangeMin; float rangeMax; const char* unitPrefix; };  // e.g. "Δ"
virtual bool GetPresentation(StringCRC layerId, ViewPresentation& out) { return false; }   // default: schema colouring
```

When it returns true, the viewer uses that ramp and range instead of the schema's (registering the ramp with CellField as usual), and the legend shows `unitPrefix` before the unit. The default keeps scrub unchanged.

### Sides and data

| Side | Source of values | Available when |
|---|---|---|
| `now` | the layer's live `read` | always |
| `sample:<i>` | the dequantised per-cell ring (Feature 1) | `i` is in the retained window and the layer is tracked |
| `snapshot:<name>` | the snapshot's cell file (Feature 3), loaded into the cache | the snapshot exists, is for the active grid and holds the layer |

- A delta cell is `B − A`. It is NaN (and shown as no data) when either side is NaN. A layer with no data on a side has no delta at all; `GetValues` returns false.
- Snapshot cell files are read when the command runs (one synchronous read of about 1 MB), not per frame; the cache keeps the 2 most recent snapshots.
- Both sides use the same quantisation as the rings, so a delta between two recorded sides has error at most `2 × range / 65,535`; a `now` side is exact.

### State machine and commands

Views are `Live`, `Sample(n)` (Feature 2) and `Delta(A, B, scale)`.

| Command | Effect |
|---|---|
| `cells.history.delta <layer> <A> <B>` | Enter delta mode: the base layer becomes `<layer>` and every float layer shows B − A. `A` and `B` are `now`, `sample:<i>` or `snapshot:<name>` |
| `cells.history.delta scale <n \| auto>` | Fix the symmetric colour range to ±`n`, or return to auto |
| `cells.history.delta groupside <A \| B>` | Choose which side's grouping defines the group table's cells (default B) |
| `cells.history.delta off` | Leave delta mode and return to the view held before it (live or the sample) |

- `cells.history.view` and `cells.history.live` also leave delta mode. `cells.history.step` and `cells.history.event` are refused in delta mode with "leave delta mode first".
- Errors, with nothing changed: an unknown layer or a category layer, a bad side, a sample outside the range, an unknown snapshot, a snapshot for another grid, both sides identical, or no data on either side for `<layer>`.
- Results are JSON: `{ ok, mode: "delta", a, b, scale }`.
- `IsLive()` is false in delta mode.

### What the viewer shows

- **Base layer:** the delta, drawn with `ResolveDeltaRamp` (the layer's `ui.delta_ramp`, default `diverging.blue_red`), range `[-s, +s]` so zero is the neutral colour. `s` is the **scale**.
- **Scale:** in auto mode, the largest absolute delta over the layer's non-NaN cells, rounded up to a "nice" number (1, 2 or 5 × 10ⁿ). It grows immediately, so nothing is clipped, and shrinks only after the largest delta has stayed under 40% of the scale for 2 seconds of wall-clock time, so a live `now` side does not flicker; `delta scale <n>` fixes it. The legend shows the ramp with ±`s`, the unit with a "Δ" prefix, and "auto" or "fixed".
- **Banner:** "Δ B − A — A: `<a>` (label), B: `<b>` (label)" with an Exit button (`cells.history.delta off`).
- **Hover:** the delta with a sign, the unit, and both side values in the tooltip.
- **Query:** `LayerReader` returns deltas, so `vegetation < -0.1` selects the cells that fell by more than 0.1; a category layer in a query in delta mode is "no data".
- **Inspector:** per-layer stats of the delta over the selection (count, min, mean, max with the Δ unit). Layers with no data on a side are listed as "no data".
- **`cells.layer.values`** returns deltas, and `cells.view.state.history` reports `{ mode: "delta", a, b, scale }` (`scale` null in auto).
- **Timeline:** shows markers for A and B where they are samples or snapshots (a snapshot at its sample).

### Group summary in the inspector

In delta mode the inspector card adds a section for the active base layer: for each grouping layer's groups, a row with the mean on side A, the mean on side B and the change (B minus A), sorted by absolute change. The group's cells are those of the group on the chosen side (default B; `delta groupside <A|B>`), using that side's grouping layer values or the live values when that side has none, restricted to the current selection when there is one. The card names the side in use. A group with no cells in the restriction is omitted. It is computed on the viewer's PU when the selection, the layers or the view changes (one pass per frame, as in the inspector).

### Category layers

A category layer has no delta in v1: in delta mode it returns no data, a base layer that is a category layer is refused when entering delta mode, and a query on one reports "no data in this view". A "changed / unchanged" map is a later feature.

### Logging and observation

- **Channel:** `HIS`.
- **Info:** entered delta (layer, sides), left delta, scale fixed or auto.
- **Warning:** a layer with no data on a side (once per layer per delta); a snapshot cell file that fails to load (Error).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on the delta read and upload, snapshot load and the group table.
- **Metrics:** delta entries, snapshot cache hits and misses, delta read ms.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. `cells.history.delta <layer> <A> <B>` enters delta mode for `now`, `sample:<i>` and `snapshot:<name>` in all combinations, and each cell shows B − A.
2. A layer with no data on a side (untracked for a sample, absent from a snapshot) shows no data; no delta is ever computed against live values to fill the gap.
3. Errors (category layer, unknown layer, bad side, out-of-range sample, unknown snapshot, snapshot for another grid, identical sides, no data on either side) change nothing and say why.
4. The delta is drawn with the layer's delta ramp, symmetric around zero; zero is the neutral colour; the legend shows ±scale, the unit with a Δ prefix, and auto or fixed.
5. Auto scale is the largest absolute delta rounded up to 1, 2 or 5 × 10ⁿ; it grows immediately and shrinks only after the largest delta has stayed under 40% of it for 2 seconds; `delta scale <n>` fixes it; `delta scale auto` restores auto.
6. `GetPresentation` is honoured by the viewer for ramp, range and unit prefix; with the default scrub behaviour unchanged.
7. In delta mode, hover shows the signed delta and both side values; `vegetation < -0.1` selects exactly the cells whose delta is below -0.1; the inspector's stats are over the delta values.
8. NaN on either side gives no data for that cell.
9. The inspector's group section lists, per group, the mean on A, the mean on B and the change, sorted by absolute change, restricted to the selection when one exists; the cells of each group come from the chosen side (default B, `cells.history.delta groupside <A|B>` switches), or from live values when that side has no grouping data, and the card names the side.
10. `cells.history.delta off`, `view` and `live` leave delta mode; `step` and `event` are refused with the message; `off` returns to the view held before.
11. `IsLive()` is false in delta mode; `cells.layer.values` returns deltas; `cells.view.state.history` reports `{mode: "delta", a, b, scale}`.
12. Category layers are refused as the delta base layer and report no data in delta mode.
13. Snapshot cell files load once into a 2-entry cache; a failed load is an Error and the command changes nothing.
14. Entering delta or changing the view costs at most one read per layer in view and one upload at Large; the group table stays within a frame budget.
15. The timeline shows A and B markers for sample and snapshot sides.
16. The delta banner and legend are in the panel contract golden and the page renders them.
17. Visual match against the mockup's delta view.

## Test Strategy

- **GoogleTests (TDD):**
  - `SideReader` for each side kind, including window edges, untracked layers, missing snapshot layers.
  - Delta values on fixtures, NaN, quantisation error bound for recorded sides.
  - Scale: auto nice-number rounding, fixed, back to auto.
  - Commands and every error case; state machine transitions (live → delta → off, sample → delta → off, `view`/`live` while in delta, `step`/`event` refusal).
  - Hook: `GetPresentation` used by a fake viewer; scrub unchanged.
  - Query and inspector over deltas (using the DiaCellLayers fixtures); category layers report no data.
  - Group table: membership from side B and the fallback, selection restriction, sorting.
  - Snapshot cache: load, hit, LRU eviction, load failure.
- **Test utilities** in `DiaCellHistory/Testing/`: a recorder with fixture samples and a fake snapshot cell file builder.
- **Panel contract golden** and page tests for the banner, legend and group table.
- **Perf:** delta at Large (read, upload, group table).
- **CoW proof (S-3, from the system spec):** take "pre-drought", force a drought, and the delta view shows the expected sign.
- **Visual gate:** S-3 testbed against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-003 | Compare covers snapshot vs snapshot, now vs past; run vs run happens in the inspector over exported files | In-game compare is one run; run-vs-run left to T-1 |
| CH-002 | Going back is view-only | Delta changes only what is displayed |
| CH-005 | Generic | No game types; ramps and units come from the schema |
| Editor actions via DiaAPI | Every action is a command | `cells.history.delta` forms |
| PD-004 | No STL in public APIs | DiaCore types |
| Game logic on SimPU | Value source and group table on the viewer's PU | Same-PU contract |
| Panel contract golden | Panel JSON and page share golden files | Banner, legend, group table added |
| AD-001 (Dia) | Edit to DiaCellLayers (`GetPresentation`) | Amended in DiaCellLayers Feature 1; sign-off before build |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1, 2, 3 and 5 | Recorder, view state, snapshots, `history` field | Approved |
| DiaCellLayers Features 1, 2, 4, 5 | Viewer, `LayerReader`, inspector, mirror | Approved, not built |
| DiaValueSchema Feature 5 | `ResolveDeltaRamp` | Approved, not built |

## Resolved Decisions

1. **Group membership has a side switch** (2026-10-20). Both choices compare one fixed set of cells per group, so both are valid: side B answers "of the cells that are Grassland now, how did they change?", side A answers "of the cells that were Grassland, what became of them?". The default is side B (it matches what the map shows), and `cells.history.delta groupside <A|B>` (also a toggle in the card) switches it. A static grouping such as region gives the same table either way. The card labels the table with the side in use.
2. **Category delta is a named follow-up, not an open question** (2026-10-20). The design is already settled so the hook needs no rework: `ViewPresentation` gains an optional `paletteId`, and in delta mode the source returns 1 for a changed category cell and 0 for an unchanged one, with a two-entry palette ("unchanged", "changed") and the legend "changed / unchanged". It is left out of v1 because no testbed needs it yet (S-3's checks are float layers); it is recorded under Non-Goals and in the Deferred list below.
3. **Auto scale holds steady** (2026-10-20). The auto scale grows immediately, so no value is ever clipped. It shrinks only when the largest absolute delta has been under 40% of the current scale for at least 2 seconds of wall-clock time. This stops a live `now` side from making the colours flicker, while a delta between two fixed sides, whose values never change, settles on its first read.

**Deferred:** category "changed / unchanged" delta map (design in decision 2).

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Build needs AD-001 sign-off for the `GetPresentation` addition to DiaCellLayers' `IViewValueSource`.
