**Spec:** @docs/specs/applications/dia/systems/diaeditorui/cell-grid-canvas.md
**Status:** In Progress

## Implementation Patterns

### Gate before Task 1
Spec Binding Decisions require CoW AD-001 (ask before Dia edits) sign-off for `Dia/DiaEditorUI`. Confirm with the user before dispatching Task 1.

### Package conventions (from diaeditorui.plan.md)
- TypeScript only, no `.vcxproj`. Files in `Dia/DiaEditorUI/src/`, exported from `src/index.ts`.
- Inline styles / `theme` only (DEUI-002). Named export, `React.FC<Props>`, exported `CellGridCanvasProps`, `data-testid` on the root element.
- Tests: Vitest + jsdom (`npm test` in `Dia/DiaEditorUI`), setup in `src/test/setup.ts`. jsdom has no canvas, so component tests stub `HTMLCanvasElement.prototype.getContext` with a recording mock.
- Run with `npm test -- <file>` from `Dia/DiaEditorUI`; CluicheEditor consumers verify with `dia pipeline --target cluicheeditor` at the end.

### Reuse ramps.ts, not new types
`src/ramps.ts` already holds `RampData`/`ResolvedRamp`/`PaletteData`, `sampleRamp`, `rampT`, `rampColourAt` and `parseHexColour` (Rgba8 packed `0xRRGGBBAA`). The spec's `Ramp`/`Palette` shape (colour strings) is an earlier sketch. Decision for the build: props accept `{ ramp: ResolvedRamp } | { palette: PaletteData }` plus a `categorical value -> entry` lookup built from the palette entries, so DiaValueSchema output feeds the canvas with no conversion. If the spec's plain-string shape is wanted instead, say so before Task 3. Amend the spec's Architecture section in place at Task 3 (CLAUDE.md: specs are amended as code diverges).

### Colour pipeline
Build a per-draw colour lookup rather than calling `sampleRamp` per cell: for ramps, a 256-entry LUT of `rampT` buckets; for palettes, a `Map<number, Rgba8>`. 6,656 cells x per-cell sample is fine, but the LUT keeps the 16 ms budget safe once outline/footprint layers are added.

### Drawing model
One `<canvas>`, device-pixel-ratio aware. Layer order: fill, outline set, highlight, selection tint, footprint (translucent), hover. Redraw in a `useEffect` keyed on `values`, `colouring`, `highlight`, `selection`, `outline`, `footprint`, `view`, size, hovered cell. Hover is drawn as an overlay layer and must not trigger a full colour recompute (cache the fill as an offscreen canvas / `ImageData`, keyed on values + colouring + size).

### Geometry (cellGeometry.ts)
Pure functions, no DOM. Hex: flat-top, odd-q (odd columns shifted down half a cell), index = `r * width + q`, same as `HexTopology`. Verify the odd-column shift against `Dia/DiaGraphics/CellField/HexFlatTopOddQBuilder.h` in Task 2, not from memory. Layout takes `{ width, height, topology, boxW, boxH, view }` and returns a transform; `cellCentre`, `cellFromPoint` (hex: axial round via cube coords) and corner offsets all use that one transform, so hit-testing is the exact inverse of drawing.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Get AD-001 sign-off for `Dia/DiaEditorUI` edits (this plan + the placement above) | User confirms | Done | haiku | AD-001 sign-off given 2026-10-29; ramps.ts reuse approved |
| 1 | `cellGeometry.ts`: `hexDistance`, `cellsWithin`, `cellsInBox` (hex-offset box semantics, square too if topology passed), index/`{q,r}` helpers | RED then GREEN: hand-computed distances on odd and even rows; `cellsWithin` radius 0/1/2 incl. edge clipping; `cellsInBox` symmetric in argument order | Done | sonnet | 13 tests pass. Odd-q confirmed vs HexTopology.cpp: odd columns shift down half a cell. Node is not on PATH: prepend C:Program Files
odejs for npx |
| 2 | `cellGeometry.ts` layout: `computeLayout` (fit to box, then zoom/pan), `cellCentre`, `cellFromPoint`, hex corner offsets, both topologies | Round trip every centre (both topologies, 3x3 and 104x64); points near edges and corners; out-of-bounds returns null; same results under zoom 2 and non-zero pan | Pending | sonnet | AC 4, Extension "hit-testing under zoom and pan" |
| 3 | `colourMap.ts`: build ramp LUT / palette map, clamp, fallback, NaN handling; amend spec types to reuse `ramps.ts` | Ramp endpoints and clamping; palette hit and `fallback`; Uint16 and Float32 inputs | Pending | sonnet | AC 2. Spec amendment in same commit |
| 4 | `CellGridCanvas.tsx` base render: canvas sizing + DPR, fill layer, highlight outline, `data-testid`, export from `index.ts` | RTL with recording canvas mock: fillStyle/path calls per cell, highlight stroked on exactly the set (AC 5); redraw only on prop change (AC 6); AC 7 import check | Pending | sonnet | Depends on 2, 3 |
| 5 | Pointer events: `onHover` (cell / `null`), `onCellClick`, mouseleave clears hover; hover overlay without recolouring | RTL: pointer move over a centre reports index; outside grid reports `null`; click reports cell | Pending | sonnet | AC 3 |
| 6 | Pixel fixture test: 3x3 hex and 3x3 square rendered to real pixels | Expected colours at cell centres. Needs a canvas implementation (e.g. `@napi-rs/canvas` devDependency) or fall back to checking the recorded draw calls if the dependency is unwelcome | Pending | sonnet | AC 1. Ask before adding a devDependency |
| 7 | Extension layers: `selection` tint, `outline`, `footprint` translucent | RTL canvas-mock assertions per layer, draw order, sets independent of each other | Pending | sonnet | T-2 Feature 6 |
| 8 | Controlled `view`: zoom/pan applied to layout, wheel zoom (about cursor) and middle/space-drag pan call `onViewChange`; `view` unset = fit | RTL: wheel emits zoomed view with cursor anchored; space/middle drag emits pan; unset view fits box | Pending | sonnet | Consumer owns state |
| 9 | Gestures: left-drag reports `onBox(cells, additive)` (shift = additive) ; with `strokeActive`, reports `onStroke` start/move/end with de-duplicated cells along the path | RTL: box covers `cellsInBox` result; stroke fills gaps between fast pointer moves (interpolate along segment); `end` fires on pointerup and on pointer leave; click without movement still goes to `onCellClick` | Pending | sonnet | Depends on 1, 5 |
| 10 | Perf test: 104x64 grid with selection, outline and footprint sets drawn in under 16 ms | Timing assertion with generous margin in jsdom mock; note real-canvas timing manually in the editor | Pending | haiku | AC 6, Extension perf |
| 11 | Module/docs: update `diaeditorui.md` component list, export types, BACKLOG row (resume point), mark spec Done via `dia docs spec-done` once child criteria met | `dia docs spec-done` accepts; `npm test` full package green | Pending | haiku | Spec Status line references this plan |

## Task order and parallelism
1 and 3 are independent (different files) and can run in parallel. 2 follows 1 (same file). 4 needs 2 and 3. 5 follows 4. 7, 8 follow 4 and can be parallel only if split across files (they both edit `CellGridCanvas.tsx`, so run them sequentially). 9 needs 5 and 1. 6 can run after 4.

Base spec (tasks 1-6, 10) is what T-1's mini-map needs; tasks 7-9 are the T-2 Feature 6 extensions and can ship in a second commit batch.

## Observation scan (at Verify)
Pure UI component: no engine instrumentation expected. Note any hover/redraw timing worth exposing to the editor's debug panel.
