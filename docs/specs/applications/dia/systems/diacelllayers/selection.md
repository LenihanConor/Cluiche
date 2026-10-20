# Feature Spec: DiaCellLayers Selection

**Parent:** @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md
**Builds on:** @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md (Feature 1)
**Mockup:** @docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html

**Gameplay Domains:** debug, spatial

## Summary

The cell selection model and its tools. A `CellSelection` is a set of cells on the active grid. It can be built by clicking, shift-click toggling, dragging a box, alt-dragging a lasso, picking a region or biome (or ctrl-clicking a cell), or by an explicit cell list from another tool such as T-1. Every tool is a DiaAPI command, so the panel, the console and the editor use the same path. The selection is drawn as an outline over the map, and other systems can read it.

## Problem

Debugging a world sim means looking at groups of cells: a region, a coastline, the cells a cause touched. A single-cell inspector (what S-1, A4 and A1 each planned) cannot answer "what is going on over here?". The selection model has to exist before the query language, the inspector with its stats, and the Inspector plugin can use it.

## Goals

1. **A selection set.** One `CellSelection` per grid, with a version that changes whenever the set changes.
2. **Seven tools**, each a command: click, toggle, box, polygon (lasso), category, cell list, clear.
3. **Mouse and keyboard gestures** that run those commands, without clashing with camera pan.
4. **Visible.** The selection is outlined on the map within the debug-draw budget.
5. **Readable by systems.** Other modules read the current selection and its version.
6. **Safe.** Out-of-bounds and duplicate cells are ignored; bad arguments change nothing.

## Non-Goals

- The query language and `cells.select.query` (Feature 3).
- Per-layer stats, histograms, pin and compare (Feature 4).
- The `cells.view.state` topic and the editor mirror (Feature 5).
- Editing values in the selection (systems own their edit commands and read the selection).
- Highlight by a second CellField request (needs CellField Step 2 stacking).
- Selection across several grids at once.

## Architecture

Lives in `Dia/DiaCellLayers/` (module from Feature 1), namespace `Dia::CellLayers`.

```
CellSelection        bitset over the active grid (width × height) + count + version
SelectionTools       the seven operations; pure functions over a grid and a layer reader
LayerReader          per-layer cached read (by layer version), shared with Features 3 and 4
SelectionOutline     perimeter edges of the selection, cached per selection version
SelectionInput       mouse gestures → commands (uses Feature 1's CellPicker)
ICellSelectionView   read-only access for other systems
```

### `CellSelection`

- A bitset sized to the active grid, a cached count, and a `uint32` version that increments on every change.
- Tied to the grid: switching the active grid (Feature 1) or a change in the grid's width, height or id **clears** the selection.
- Cell identity is the grid index (row-major, as in Feature 1).
- Scrubbing history (DiaCellHistory) does not change the selection.

### Commands

| Command | Effect on the selection |
|---|---|
| `cells.select.cell <q> <r>` | Replace with that cell |
| `cells.select.toggle <q> <r>` | Add the cell if absent, remove it if present |
| `cells.select.box <q0> <r0> <q1> <r1>` | Replace with the inclusive offset rectangle between the two corner cells |
| `cells.select.poly <q,r …>` | Replace with the cells whose centres lie inside the polygon (even-odd rule); at least 3 vertices |
| `cells.select.category <layerId> <value>` | Replace with every cell whose value in that category layer equals `<value>` (an enum id or number) |
| `cells.select.cells <q,r …>` | Replace with exactly the listed cells; out-of-bounds ignored; at most 2,048 per call, extras ignored; returns the accepted cells |
| `cells.select.clear` | Empty the selection |

- Results are JSON: `{ ok, count }`; `cells.select.cells` also returns `{ requested, accepted: [[q, r], …] }`, so callers (T-1's "Show cells") can report "Selected N of M".
- Bad arguments (unknown layer or value, fewer than 3 polygon vertices, non-numeric coordinates, a non-category layer for `category`) return an error and leave the selection unchanged. The error text goes to `CEL` at Info.
- `box` is in cell coordinates; the corner cells may be given in any order.

### Category selection and the layer reader

`cells.select.category` needs the values of a category layer that is not necessarily the active base. `LayerReader` returns a layer's values, calling its `read` only when the layer's `version()` changed since the last read, and caching the buffer. It returns the values **the viewer shows**: it asks the installed `IViewValueSource` first (Feature 1, amended 2026-10-20 for DiaCellHistory), so while a past sample is viewed the reader returns that sample's values, and a layer with no data in that view reports none. Cached reads are keyed by `version()` and the source's `ViewEpoch()`. With 128 layers at Large (6,656 cells) the cache is at most about 3.4 MB. Features 3 (query) and 4 (aggregates) use the same reader, so each layer is read at most once per version.

### Gestures

The viewer card has a small tool strip (**Select** active by default, **Pan**). Camera pan stays on middle or right drag in any tool.

| Gesture (Select tool) | Command |
|---|---|
| Click a cell | `cells.select.cell` |
| Shift-click | `cells.select.toggle` |
| Drag | `cells.select.box` (start cell and end cell as corners) |
| Alt-drag | `cells.select.poly` (the cells under the path, sampled, as vertices) |
| Ctrl-click a cell | `cells.select.category <selection group> <that cell's value>` |
| Esc | `cells.select.clear` |

- **Selection group:** a dropdown in the card lists every category layer of the active grid; the default is the first whose schema has `ui.select_group: true`. The user can switch it. Any category layer can also be picked from the card's list, which runs `cells.select.category` directly.
- Gestures use Feature 1's `CellPicker`. A click outside the grid is ignored. Keyboard gestures (Esc, shift, ctrl, alt) need keyboard input to the panel; without it the mouse tools that don't need modifiers and the console commands still work.

### Outline

- The viewer draws the selection as an outline overlay through `IDebugDraw`: only the **perimeter edges** (edges between a selected cell and a non-selected or missing neighbour, found with `HexTopology` neighbours). A region therefore costs far fewer primitives than its cells.
- The edge list is rebuilt only when the selection version changes and cached.
- **Cap:** at most 4,000 edge primitives (about half of the 8,192 debug-draw budget). Above the cap the outline is skipped and the card shows "Selection too large to outline (N cells)". The selection itself is unaffected.

### Hand-off to systems

```cpp
class ICellSelectionView {
public:
    virtual uint32  Version() const = 0;
    virtual uint32  Count() const = 0;
    virtual bool    Contains(uint32 cellIndex) const = 0;
    virtual void    ForEach(void (*fn)(uint32 cellIndex, void* user), void* user) const = 0;
};
```

Reached through the registry's service (per active grid). A1's paint, for example, reads the selection to apply a value to it. Overlay `draw` callbacks receive the same view as their selection argument (Feature 1's `CellSelection*` parameter).

### Panel card additions

The card shows the selection count, the tool strip, the selection-group dropdown and a clear button, plus the "too large to outline" banner. The card JSON is added to the panel contract golden.

### Logging and observation

- **Channel:** `CEL`.
- **Info:** a selection command failed (with the reason); selection cleared by grid change.
- **Warning:** a `cells.select.cells` call over the 2,048 cap (once per call).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on polygon fill, category select and outline rebuild.
- **Metrics:** selection size, outline edges, commands run, rejected commands.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. Click, toggle, box, polygon, category, cell list and clear commands change the selection exactly as in the table, on a 66 × 42 and a 104 × 64 hex grid.
2. `box` accepts the corners in any order and selects the inclusive offset rectangle; `poly` selects the cells whose centres are inside (even-odd) and needs at least 3 vertices.
3. `category` selects every cell whose value equals the argument, accepts an enum id or a number, and rejects a non-category layer or an unknown value without changing the selection.
4. `cells.select.cells` ignores out-of-bounds and duplicate cells, accepts at most 2,048 per call, and returns the requested count and the accepted cells.
5. Out-of-bounds coordinates in any command are ignored, and bad arguments return an error and leave the selection unchanged.
6. The selection version increments on every change and not otherwise; switching grid or changing the grid's size or id clears the selection.
7. `LayerReader` calls a layer's `read` only when that layer's `version()` changed, and category selection from a non-base layer works.
8. Mouse gestures run the matching commands; camera pan on middle or right drag still works in the Select tool; the Pan tool pans on left drag.
9. The selection group dropdown lists the grid's category layers, defaults to the first with `ui.select_group: true`, and ctrl-click uses the current choice.
10. The outline draws only perimeter edges, rebuilds only when the version changes, and above 4,000 edges is skipped with the "too large to outline" banner while selection still works.
11. `ICellSelectionView` returns the same set as the commands produced, with its version; overlay `draw` callbacks receive it.
12. Without keyboard input to the panel, the console commands and unmodified mouse tools still work.
13. The card JSON for selection (count, tool, group, banner) is in the panel contract golden.
14. Selecting 6,656 cells (all of Large) and rebuilding the outline stay within a frame budget.
15. Visual match against the mockup's selection tools and outline.

## Test Strategy

- **GoogleTests (TDD):**
  - Each command on known grids: hand-computed expected sets for box, polygon (including concave and self-touching), category and cell list.
  - Edge cases: out-of-bounds, duplicates, corner ordering, cap, empty list, unknown layer, non-category layer.
  - Version semantics and clearing on grid change.
  - `LayerReader` version gating with a counting stub provider.
  - Perimeter edge count for a single cell (6), a region and the full grid.
  - Cap behaviour at 4,000 edges.
  - `ICellSelectionView` agreement with the commands.
  - `SelectionInput` with a fake picker: click, shift-click, drag, alt-drag, ctrl-click, Esc.
- **Panel contract golden** and page tests for the card additions.
- **Test utilities** in `DiaCellLayers/Testing/`: grid fixtures, a counting stub provider, a selection assertion helper.
- **Perf:** all-cells selection, category select at Large, outline rebuild.
- **Visual gate:** S-1 or S-3 testbed against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CL-003 | v1 selection: click, shift, box, lasso, category and query | Query is Feature 3; the rest are built here |
| CL-005 | Generic, no game types | Region and biome are just category layers; `ui.select_group` marks them |
| CL-006 | No singletons | Selection view is reached through the registry service |
| Editor actions via DiaAPI | Every action is a command | All tools are `cells.select.*` commands |
| Debug-draw budget | `DebugFrameData::kGeometryCapacity` = 8,192 | Outline capped at 4,000 edges |
| PD-004 | No STL in public APIs | Bitset and callbacks use DiaCore types and function pointers |
| Game logic on SimPU | Viewer and selection on SimPU | Same-PU contract from Feature 1 |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Registry, viewer, `CellPicker`, hover | Approved |
| `HexTopology` (DiaPathfinding) | Neighbours and cell centres | Done |
| DiaValueSchema Feature 1 | `ui.select_group`, enum values | Approved |
| DiaUIUltralight Game Input Bridge | Keyboard gestures | Spec only; keyboard gestures inert until built |
| T-1 Navigation | Calls `cells.select.cells` | Approved |

## Open Design Questions

1. **Box shape on screen.** `box` selects an offset rectangle in cell coordinates, which has a zig-zag edge on a flat-top hex map and may not match the rectangle the user dragged. If it looks wrong on real maps, make the mouse drag select by screen rectangle (cells whose centres are inside) and send the result as a cell list, keeping `box` for scripts.
2. **Lasso sampling.** Sampling the cells under a freehand path is easy to get too coarse or too heavy. Decide the sampling step (for example one vertex per cell crossed) when it is built, and test with fast mouse movement.
3. **Selection across rebuilds.** S-1 regenerates worlds, which clears the selection by design. If testbeds want to keep a selection across a regeneration of the same size, add a "keep if same grid size" option later rather than guessing now.

## Status

`Approved` (2026-10-20). Plan not yet created.
