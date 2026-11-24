# Feature Spec: CellGridCanvas

**Parent:** @docs/specs/applications/dia/systems/diaeditorui/diaeditorui.md

**Gameplay Domains:** debug

## Summary

A generic React component in `@dia/editor-ui` that draws a flat-top hex or square grid of cells coloured by a value array through a ramp or palette, with hover and click reporting and an optional highlight set. It is a display component: no game, CoW or CellField knowledge. First consumers are T-1's hex mini-map and T-2's cell editor (T-2 Feature 6 adds brush and selection tools on top).

## Problem

Two CoW editor plugins need to draw a per-cell grid map. Each building its own canvas would duplicate hex maths, colour mapping and hit-testing, and the two would drift. One shared component keeps them consistent, and keeps CoW-specific code out of Dia.

## Goals

1. Draw a grid of up to Large size (6,656 cells) with ramp (continuous) or palette (category) colouring within one frame.
2. Report the hovered cell and clicked cell in grid coordinates.
3. Optionally highlight a set of cells.
4. Support flat-top hex (odd-q) and square topologies.
5. Take ramps and palettes as plain data, so DiaValueSchema is the source but this component does not depend on it.

## Extensions for T-2 Feature 6 (added 2026-10-20)

The cell editor needs more than display. These optional props and helpers are added; each is unused by T-1's mini-map, which stays on the base props. The canvas still draws and reports only: what a stroke or box means is the consumer's decision.

```ts
// additional optional props
selection?: ReadonlySet<number>            // cell indices drawn with a selection tint
outline?: ReadonlySet<number>              // cell indices drawn with an outline (for example edited cells)
footprint?: ReadonlySet<number>            // cells under a tool's preview, drawn translucent
onBox?: (cells: number[], additive: boolean) => void           // drag-selected cells (a box, in hex-offset terms)
onStroke?: (cells: number[], phase: 'start' | 'move' | 'end') => void   // pointer path cells while a stroke is active
strokeActive?: boolean                     // when true, left-drag reports strokes instead of a box
view?: { zoom: number; panX: number; panY: number }            // controlled zoom and pan
onViewChange?: (view) => void              // wheel zoom and middle/space drag pan
```

```ts
// exported geometry helpers (cellGeometry.ts)
hexDistance(a: number, b: number, width: number): number        // flat-top odd-q
cellsWithin(centre: number, radius: number, width: number, height: number): number[]
cellsInBox(a: number, b: number, width: number): number[]
```

- With `view` unset the grid still fits the box as in the base spec.
- Hit-testing, hover and click keep working under any zoom and pan.
- A 104 × 64 grid with a selection, an outline and a footprint set draws in under 16 ms.
- Tests: the helpers against hand-computed distances (odd and even rows), the callbacks for box and stroke gestures, hit-testing under zoom and pan.

## Non-Goals

- Brush, paint and selection *semantics* (the canvas only reports gestures; T-2 Feature 6 decides what they do).
- Zoom and pan are optional (the controlled `view` prop above); with it unset the grid fits the box.
- Animation, time scrubbing, labels or glyphs on cells.
- Reading any DiaCellLayers, CellField or CoW type.
- A WebGL renderer: canvas 2D is enough at Large size.

## Architecture

`Dia/DiaEditorUI/src/CellGridCanvas.tsx`, exported from the package index. Pure TypeScript; styles through the `theme` object (DEUI-002).

```ts
// Ramp and palette types are the existing ones from ramps.ts (colours are packed Rgba8), so DiaValueSchema output feeds
// the canvas unchanged. A palette value is the enum ordinal (position in entries); anything else uses fallback.
export type CellColouring = { ramp: ResolvedRamp } | { palette: PaletteData }   // cellColour.ts

export interface CellGridCanvasProps {
  topology: 'hex-flat-odd-q' | 'square'
  width: number; height: number                  // cells
  values: Float32Array | Uint16Array             // length width*height, row-major
  colouring: CellColouring
  highlight?: ReadonlySet<number>                // cell indices
  onHover?: (cell: { q: number; r: number; index: number } | null) => void
  onCellClick?: (cell: { q: number; r: number; index: number }) => void
}
```

- Cell geometry and hit-testing live in a pure helper (`cellGeometry.ts`): `cellCentre`, `cellFromPoint`, hex corner offsets. Hit-testing is the inverse of drawing and shares the same layout code.
- The component draws to a `<canvas>` and redraws only when `values`, `colouring`, `highlight` or size change.
- Out-of-range values clamp to the ramp ends; values missing from a palette use `fallback`.
- Index order matches `HexTopology` (flat-top, odd-q, row-major) so a cell index means the same on both sides.

## Acceptance Criteria

1. A hex-flat-odd-q and a square grid render at the requested size, with cell positions matching `HexTopology`'s odd-q layout.
2. Ramp colouring maps values to colours across `min`–`max`, clamping outside values; palette colouring maps values to entries and uses `fallback` for unknown values.
3. `onHover` reports the cell under the pointer, and `null` outside the grid; `onCellClick` reports the clicked cell.
4. Hit-testing returns the drawn cell for every cell centre and for points near cell edges, for both topologies.
5. A highlight set draws a visible outline on exactly those cells.
6. A 104×64 grid draws in under 16 ms and redraws only on prop changes.
7. The component imports nothing from CoW, DiaCellLayers or CellField.

## Test Strategy

- **Vitest (pure helper):** `cellCentre`/`cellFromPoint` round trip for both topologies; odd-q offsets; edge points; out-of-bounds returns null.
- **React Testing Library with a canvas mock:** props change redraws, hover and click callbacks, highlight set.
- **Pixel test:** a tiny fixture (3×3 hex and square) checked against expected colours.
- **Perf:** a 104×64 grid draw timing.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| DEUI-001 | Local npm workspace package | Added under `Dia/DiaEditorUI/src/` |
| DEUI-002 | No bundled CSS; theme object | Inline styles and theme only |
| DEUI-007 | `file:` path reference | No change to how editors consume the package |
| CoW AD-001 | Ask before Dia edits | Spec reviewed and sign-off taken before build |
| Engine vs Application boundary | Generic capability belongs in Dia | No CoW types in the component |

No conflicts.

## Open Design Questions

1. **Hex orientation.** `HexTopology` is flat-top odd-q; DiaScalarField has a separate pointy-top axial topology (a known debt in the backlog). Only flat-top odd-q is supported; add others only if a consumer needs them.
2. **Zoom and pan.** T-2's cell editor will probably need them for Large maps. Add in the T-2 feature, behind optional props, rather than designing now.

## Status

`Done`

Built 2026-10-29 per [cell-grid-canvas.plan.md](../../../../../archive/plans/cell-grid-canvas.plan.md) (AD-001 sign-off given). All tests pass on a software canvas. Palette values are enum ordinals. Created 2026-10-20 for T-1 Feature 3 and T-2 Feature 6.

## Follow-ups (not blocking Done)

1. **Validate the 16 ms budget (AC 6) on the editor's GPU-backed canvas** when the first consumer (T-1 mini-map) is built. Software-canvas numbers for 104x64 hex: fill ~11 ms, selection/outline/footprint layers ~7 ms. If it misses, cache the fill layer to an offscreen canvas. Read paint cost from the DevTools Performance panel, not just JS call time.
2. **Box-drag rubber band.** `onBox` fires on mouseup only; there is no live outline while dragging. Add when the T-2 cell editor (Feature 6) needs it, probably drawn on the overlay layer.
3. **Pointer events and capture.** The canvas uses mouse events (jsdom drops `clientX` on PointerEvent). A drag that leaves the component ends on mouse-leave. Move to pointer events with pointer capture if that feels wrong in the editor.
