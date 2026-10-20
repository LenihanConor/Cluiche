# Feature Spec: DiaCellLayers Selection Inspector

**Parent:** @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md
**Builds on:** @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md (Feature 1), @docs/specs/applications/dia/systems/diacelllayers/selection.md (Feature 2)
**Mockup:** @docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html

**Gameplay Domains:** debug, spatial

## Summary

Shows what the selected cells look like across every layer. For a selection it gives, per layer, the count, minimum, mean and maximum with units (float layers) or counts per value (category layers), a 16-bin histogram of the active base layer, and a pinned selection A to compare against the current selection B with a delta column. For a single cell it shows exact values for every layer plus a details section that systems fill in (for example A1's change ledger). The same aggregates are available as a command, `cells.inspect`, which Feature 5's `cells.select.stats` query reuses.

## Problem

Seeing a region on the map answers "where"; the next question is "what are its numbers, across all the layers, and how does it differ from there?". Today each system plans its own single-cell inspector. A selection inspector over the shared registry gives the same view for terrain, weather, ecology and anything a game adds, and gives the Inspector plugin a ready-made source of per-selection stats.

## Goals

1. **Stats for any selection.** Per layer: count, min, mean, max and unit; category counts per value.
2. **Cheap by default.** Only layers the user can see are computed; the rest on demand.
3. **Compare.** Pin a selection as A and see A, B and the delta (B minus A), live or frozen.
4. **One cell in detail.** Exact values for every layer, plus system-provided details.
5. **Histogram.** 16 bins of the active base layer over the schema range.
6. **Scriptable.** `cells.inspect` returns the same data as JSON.
7. **Explained.** Labels, units and tooltips come from DiaValueSchema.

## Non-Goals

- Median, percentiles, weighted means or any statistic beyond count, min, mean, max, NaN count, histogram and category counts.
- Whole-grid statistics with no selection (see Empty selection).
- Editing values or applying them to the selection (systems do that with the selection view).
- Time series of a selection (DiaCellHistory and T-1).
- More than one pinned selection.

## Architecture

Lives in `Dia/DiaCellLayers/` (namespace `Dia::CellLayers`).

```
SelectionStats        compute LayerStats for a selection over a layer (float and category)
InspectorState        expanded groups, pinned set A, freeze flag, cached stats, dirty flags
InspectorModel        what the card shows: layers, histogram, compare, single-cell table, details
CellDetailRegistry    per-grid detail providers (the describeCell slot)
```

### Aggregates

For a selection and a layer, `SelectionStats` reads the layer through Feature 2's `LayerReader` and returns:

- **Float layers:** `count` (non-NaN cells), `nanCount`, `min`, `mean`, `max`, and the unit and label from the schema. Mean uses a `double` accumulator. An empty or all-NaN result reports count 0 and no min, mean or max.
- **Category layers:** one count per enum value, in schema order, plus `other` for ids outside the enum and `nanCount`.
- **Histogram:** for a float base layer, 16 equal bins over the schema `min`–`max`; values outside the range are counted in the first or last bin and reported in an "out of range" footer. For a category base layer, the category counts serve as the histogram.

### When aggregates are computed

- **Which layers:** the active base layer plus every layer in an **expanded group** of the inspector (the active group is expanded by default). Collapsed groups compute when expanded. This keeps the per-selection cost near the 20 layers the perf target assumes, even with 100+ layers registered.
- **When:** after the selection version, a shown layer's `version()`, or the expanded set changes, at most **one recompute pass per frame**, on the viewer's PU (SimPU). A pass handles all dirty layers.
- **Stale marker:** the card shows a small "updating" mark in the frame a pass is pending, never stale numbers without it.

### Empty selection

With nothing selected the inspector shows "Select cells to inspect" and computes nothing.

### Pin and compare

| Command | Effect |
|---|---|
| `cells.select.pin` | Stores the current selection's cell set as **A** |
| `cells.select.pin freeze` | Stores A and also computes and stores A's stats for **every** registered layer of the grid once (one explicit cost) |
| `cells.select.unpin` | Clears A |

- With A pinned the card shows A and B side by side and a **delta** column (B minus A) for count, min, mean and max, with units.
- **Live (default):** A's stats recompute with B's, from A's cell set, so it answers "region X against region Y now".
- **Frozen:** A's stored stats are shown unchanged, so it answers "these cells before and after". A badge marks frozen values.
- A layer that has no frozen stats (registered after the freeze) shows "not frozen".
- Pinning with nothing selected is an error and changes nothing. A is cleared when the active grid changes or its size or id changes, like the selection (Feature 2).

### Single cell

With exactly one cell selected, the card shows a table of exact values for every layer in the shown groups (full precision, unit, enum label for categories), and a **details** section filled by providers:

```cpp
// registered by a system for a grid
cellDetails.Register(gridId, "Ecology", [this](uint32 cellIndex, CellDetailBuilder& b) {
    b.Section("Change ledger (last band)");
    b.RowValue(StringCRC("cow.ecology.cell/producers_total"), delta);   // label, unit from the schema
    b.Row("Dominant cause", "Grazing");
});
```

- Several providers may register for a grid; sections appear in registration order.
- `RowValue` takes a schema key so the label, unit and tooltip come from DiaValueSchema; `Row` takes literal text.
- A provider runs only when exactly one cell is selected and the card is open, on the viewer's PU, and may add at most 64 rows (extras are dropped with a Warning once per provider).

### Commands and card

| Command | Effect |
|---|---|
| `cells.inspect [group \| layerId … \| all]` | Returns the stats JSON for the selection: shown layers by default, the named group or layers, or every layer with `all`; includes the histogram, the pinned A and delta when pinned, and the single-cell table and details for one cell |

The card shows the layer groups as accordions with per-layer rows (label with tooltip, count, min, mean, max, unit, plus a small bar for position in the range), category rows as counts, the histogram, compare columns, and the details section. Its JSON is added to the panel contract golden. Result JSON uses the schema keys, so Feature 5 and T-1 can label it.

### Logging and observation

- **Channel:** `CEL`.
- **Info:** pin, unpin, freeze (layers frozen).
- **Warning:** a detail provider over its row cap; a layer with an unresolved `schemaKey` in the inspector (once per key).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on a recompute pass and on each provider call.
- **Metrics:** recompute passes, layers computed per pass, pass ms, pinned flag.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. For a selection, each shown float layer reports `count`, `nanCount`, `min`, `mean`, `max` and unit, matching hand-computed values on fixtures; NaN cells are excluded and counted; an empty or all-NaN result reports count 0 and no min, mean or max.
2. Each shown category layer reports a count per enum value in schema order, `other` for ids outside the enum, and `nanCount`.
3. The histogram of a float base layer has 16 equal bins over the schema range, counts out-of-range values in the end bins, and shows the out-of-range footer; a category base layer shows its category counts.
4. Only the active base and layers in expanded groups are computed; expanding a group computes its layers; each layer is read at most once per version.
5. A recompute pass runs at most once per frame after a change in selection version, a shown layer's version, or the expanded set, and the card marks the pending frame.
6. With nothing selected the inspector shows the prompt and computes nothing.
7. `cells.select.pin` stores the cell set as A; with A pinned the card shows A, B and a delta (B minus A) for count, min, mean and max with units; live A recomputes with B.
8. `cells.select.pin freeze` stores A's stats for every registered layer once and shows a frozen badge; layers registered later show "not frozen"; `unpin` clears A.
9. Pinning an empty selection is an error with no change; A is cleared when the active grid or its size or id changes.
10. With one cell selected, the card shows exact values for the shown layers and the details sections from registered providers in registration order; a provider is not called otherwise.
11. `RowValue` takes label and unit from the schema; a provider adding more than 64 rows has the extras dropped with one Warning.
12. `cells.inspect` returns the stats JSON for the shown layers by default, for a named group or layers, or for all layers with `all`, including histogram, pinned A with delta and single-cell data.
13. Aggregating all cells of a Large grid over 20 layers takes at most 2 ms in Release.
14. The inspector card JSON is in the panel contract golden, and the page renders it.
15. Visual match against the mockup's inspector, histogram and compare columns.

## Test Strategy

- **GoogleTests (TDD):**
  - `SelectionStats` on known data: min, mean, max, NaN handling, empty, single value, category counts with `other`, histogram bins and out-of-range footer.
  - Lazy computation: expanded group rules, one read per version, one pass per frame (counting stub provider).
  - Pin, unpin, live and frozen compare, delta values, "not frozen", clearing on grid change, pin with empty selection.
  - Single cell: exact values, provider ordering, row cap, provider not called for multi-cell selections.
  - `cells.inspect` forms: default, group, layer list, `all`.
- **Test utilities** in `DiaCellLayers/Testing/`: layer fixtures with known values, stats assertion helpers, a stub detail provider.
- **Panel contract golden** and page tests for the card.
- **Perf:** all-cells aggregate over 20 layers at Large in Release.
- **Visual gate:** S-3 testbed against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CL-004 | The inspector shows stats for every layer, with pin-and-compare | Built, with expanded-group computation so cost stays bounded |
| CL-005 | Generic, no game types | Details come from registered providers; no CoW types |
| CL-006 | No singletons | Detail registry is reached through the registry service |
| Editor actions via DiaAPI | Every action is a command | `cells.select.pin`, `unpin`, `cells.inspect` |
| PD-004 | No STL in public APIs | DiaCore types and function pointers |
| Game logic on SimPU | Aggregates and providers on the viewer's PU | Same-PU contract from Feature 1 |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 and 2 | Registry, viewer, selection, `LayerReader` | Approved |
| DiaValueSchema Features 1 and 3 | Labels, units, enum values, tooltips | Approved |
| A1 Ecology | Registers the ledger details provider (A1 Feature 9) | Approved, not built |
| Feature 5 | Reuses the aggregator for `cells.select.stats` | Draft (TBD) |

## Open Design Questions

1. **No select-all command.** Feature 2 has no `cells.select.all`, and an empty selection shows only a prompt. A box over the whole grid works, but a one-key select-all would be handy for whole-grid numbers. Consider adding `cells.select.all` to Feature 2; this spec does not depend on it.
2. **Frozen compare and the layer set.** Freezing computes every registered layer once, which at Large with 128 layers is a visible but one-off cost. If it hitches, spread the freeze over a few frames.
3. **Details sections and the 64-row cap.** The cap is a guess. Revisit when A1's ledger provider exists and shows how many rows it needs.

## Status

`Approved` (2026-10-20). Plan not yet created.
