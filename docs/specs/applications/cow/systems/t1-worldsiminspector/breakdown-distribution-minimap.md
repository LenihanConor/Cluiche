# Feature Spec: T-1 Breakdown, Distribution and Mini-Map

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md (Feature 6)

**Gameplay Domains:** debug

## Summary

Three views that answer "where, and how spread out?": a **breakdown** table of groups (regions, biomes or any category layer) against chosen layers with a sparkline per row; a **distribution** view of a tracked layer's per-cell values as small-multiple histograms split by group; and a **hex mini-map** of a tracked layer at the brush end. All three read the Feature 1 `Source` and the shared brush, and send clicks through Feature 6's navigation model.

The mini-map needs a shared `CellGridCanvas` component in `@dia/editor-ui` (`Dia/DiaEditorUI`). That is a Dia addition, so this spec defines the API it needs and flags the build as a Dia task (see Dependencies).

## Problem

A time series shows that something changed; it does not show *which regions*, *how unevenly across cells*, or *where on the map*. Without a group table, a distribution and a map, the user must jump into the game for every "where" question.

## Goals

1. **Breakdown.** Rows are the values of a chosen category layer; columns are the mean over the brush of up to 6 chosen layers; each row has a sparkline. Clicking a row selects that group in-game.
2. **Distribution.** For one tracked layer at the brush end: a 32-bin histogram per group value (small multiples with a shared x axis), plus an "All cells" histogram.
3. **Mini-map.** A tracked layer at the brush end, coloured with the schema ramp or palette on the hex grid. Hover shows the value; clicking a cell navigates.
4. **Honest empty states.** An untracked layer, or a file exported without per-cell data, shows why and what to do, never a misleading fallback.
5. **Every number is explained.** Labels, units, ramps and tooltips come from DiaValueSchema.

## Non-Goals

- Source B overlay, Δ column or side-by-side maps (Feature 5).
- The cause "show cells" list and contribution bars (Feature 4).
- Brush, paint and selection-editing tools on the canvas (T-2 Feature 6).
- Smoothed or kernel-density distributions, box plots, or custom bin counts.
- Time scrubbing inside the mini-map; it follows the shared brush end.
- Editing any value.

## Architecture

Files under `Cluiche/CoW/Editor/WorldSimInspector/UI/`:

```
model/
  breakdown.ts    breakdown(source, groupLayer, layers[≤6], brush) → rows { group, means[], spark[] }
  histogram.ts    histogram(values, range, bins=32) ; histogramsByGroup(cells, groupCells, limit=12)
  minimap.ts      cellColours(values, scheme) ; hoverInfo(cell, source)
store/             groupLayer, breakdownLayers[], distributionLayer, minimapLayer
views/
  BreakdownTable.tsx   table + sparklines
  Distribution.tsx     small multiples
  HexMiniMap.tsx       wraps CellGridCanvas
  UntrackedNotice.tsx  empty state (+ Track button)
```

Views call `model/`, the store and the Feature 6 navigation model only; they never touch the bridge.

### Breakdown

- Group-by picker lists every category layer in the source schema (region, biome and any other). Default: the first group layer.
- Up to 6 layer columns; a seventh is refused with a message.
- Each cell is the mean over the brush of that layer's aggregate for the group; each row also has a sparkline of the first chosen layer across the brush.
- Rows sort by any column; the sparkline, tooltip, units and column headers come from the schema.
- Clicking a row raises a `group` navigation intent (Feature 6). If navigation is disabled, the row is still selectable locally and the tooltip explains why navigation is off.

### Distribution

- One tracked layer, read at the brush end through `cellQuery(layer, sample)`.
- **32 bins** across the schema `min`–`max`; when a layer's schema has no range, the observed range at that sample is used and labelled as such.
- Values outside the schema range go in the first or last bin and are counted in a footer.
- One histogram per group value on a shared x axis. At most 12 groups are shown (largest cell count first); the rest are combined under **Other**. An **All cells** histogram is always shown first.
- Hover shows the bin range and count; units from the schema.

### Hex mini-map

- Draws the tracked layer at the brush end using `CellGridCanvas` with the layer's ramp (continuous) or palette (category) from the schema; legend from the same schema.
- Hover shows `(q, r)`, the value with unit, and the schema description via `ValueTooltip`.
- Clicking a cell raises a `cell` navigation intent at the brush end sample.
- Grid topology and size come from the source (`groups`/grid info), not hard-coded.

### `CellGridCanvas` API needed (Dia, `@dia/editor-ui`)

Generic; nothing CoW-specific:

```ts
interface CellGridCanvasProps {
  topology: 'hex-flat-odd-q' | 'square';
  width: number; height: number;                 // cells
  values: Float32Array | Uint16Array;            // one per cell
  colouring: { ramp: Ramp } | { palette: Palette };
  hover?: (cell: {q:number,r:number}|null) => void;
  onCellClick?: (cell: {q:number,r:number}) => void;
  highlight?: Set<number>;                       // optional highlight set
}
```

T-2 Feature 6 extends it with brush and selection tools; T-1 only needs the props above. Whichever feature builds first creates the component.

### Untracked and file-only states

| Situation | Behaviour |
|---|---|
| Live, layer not tracked | Empty state: "This layer is not tracked." with a **Track this layer** button (see Open Q1) |
| File exported without `--cells` | Empty state: "This file has no per-cell data. Re-export with `--cells`." (no button) |
| Layer tracked but not yet sampled | Empty state: "No data at this sample yet" |

The breakdown only needs aggregates, so it works for every source.

### Logging and observation

Channel `[EDITOR] T1`. Logged: cell query chunk failures, a layer fallback range used, groups combined into Other. `performance.mark` around histogram build and map draw in dev builds.

## Acceptance Criteria

1. The breakdown group-by picker lists every category layer in the source; selecting one rebuilds the rows.
2. Up to 6 layer columns can be chosen; a seventh is refused with a message.
3. Each breakdown cell equals the mean of the group's aggregate over the brush; changing the brush updates every cell and sparkline.
4. Columns sort by any layer; headers show label and unit from the schema.
5. Clicking a breakdown row raises a `group` intent; with navigation disabled, the row selects locally and a tooltip gives the reason.
6. The distribution shows an "All cells" histogram plus one per group with 32 bins over the schema range, on a shared x axis.
7. With more than 12 groups, the 12 largest are shown and the rest combine under Other, with the count in the footer.
8. Values outside the schema range land in the first or last bin and are counted in a footer.
9. A layer with no schema range uses the observed range and the view labels it as such.
10. The mini-map colours each cell by the layer's schema ramp or palette, and shows a legend from the same schema.
11. Hovering a cell shows `(q, r)`, value, unit and description; clicking a cell raises a `cell` intent at the brush end sample.
12. For a live source with an untracked layer, the distribution and mini-map show the "not tracked" empty state with a Track button; for a file without per-cell data, they show the re-export message with no button.
13. Moving the brush end updates the distribution and mini-map; the breakdown works with aggregates alone for every source.
14. Visual match against the mockup's breakdown, distribution and mini-map.

## Test Strategy

- **Vitest (model/):** breakdown means over a brush for a fixture; sparkline series; histogram bins (edge values, out-of-range counted, fallback range); grouping with the 12-group limit and Other; cell colours from ramp and palette; empty-state selection per situation.
- **React Testing Library:** group-by picker, column limit message, sort, empty states, hover tooltips, click raises the right intent.
- **`CellGridCanvas` (Dia, when built):** draws the expected pixels for a tiny hex and square fixture; hover and click report the right cell.
- **Perf:** 6,656 cells (Large) maps and bins within a frame budget; breakdown of 30 groups × 6 layers over a 2,080-sample brush within 50 ms.
- **Visual gate:** compare against `mockups/world-sim-inspector.html` before Done.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-001 | Ask before Dia / shared build edits | `CellGridCanvas` in `Dia/DiaEditorUI` is flagged and needs sign-off before it is built |
| CoW AD-002 | Four-tier placement | T-1 code in `Cluiche/CoW/Editor/`; the canvas is generic and goes in Dia, nothing CoW-specific |
| Engine vs Application boundary (escalation rule) | A new Dia header from a game project is flagged first | API defined above; user confirms placement before build |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui` |
| CH-004 | Live and file data share one format | Views read the `Source` interface only |
| UI/business separation | Typed model first | All calculations in `model/`; views are display only |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 and 6 | Source, brush, navigation intents | Approved |
| **`CellGridCanvas` in `@dia/editor-ui`** | Mini-map | Dia feature spec drafted ([cell-grid-canvas.md](../../../dia/systems/diaeditorui/cell-grid-canvas.md)); needs approval and AD-001 sign-off before build |
| DiaCellHistory Feature 2 | Tracked-layer per-cell queries | Approved, not built |
| DiaValueSchema Features 1, 3, 5 | Labels, tooltips, ramps and palettes | Approved, not built |

## Resolved Decisions

1. **Track button allowed** (2026-10-20). `cells.history.track` changes what the game records, not the simulation, so it fits the Inspector tier. It is the only non-navigation command T-1 sends; `track` and `untrack` are added to Feature 6's allow-list (amended). The button states the cost first (for example "about 13 KB per sample at Large", from DiaCellHistory's 64 MB budget), shows Feature 6's failure toast if the game refuses on budget, is disabled in the same cases as navigation, and T-1 untracks only layers it tracked itself.
2. **Large group counts** (2026-10-20). The breakdown body is a virtualised list from the start, using `@dia/editor-ui`'s list component if one exists. The group-by picker hides category layers with more than 64 values, with a tooltip saying so. No other limit until a real layer needs one.
3. **Canvas ownership** (2026-10-20). The first feature built owns the `CellGridCanvas` API and the other adopts it. A small Dia feature spec for it is drafted now ([cell-grid-canvas.md](../../../dia/systems/diaeditorui/cell-grid-canvas.md)), with T-1's props as the baseline and T-2's tools as the planned extension; T-1 and T-2 depend on that spec, not on each other.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. The mini-map needs `CellGridCanvas` (Dia, AD-001 sign-off at build time).
