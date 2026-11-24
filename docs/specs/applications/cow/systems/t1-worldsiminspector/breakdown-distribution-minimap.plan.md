# Plan: T-1 Breakdown, Distribution and Mini-Map (Feature 3)

**Spec:** @docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md
**Status:** Not Started
**Mockup (approved 2026-11-16):** `mockups/world-sim-inspector-v2.html` (Overview region table, Space workspace mini-map) and `mockups/world-sim-inspector.html` (breakdown, distribution, mini-map, empty states). The visual gate for every view task.

## Prerequisites (gate, checked in Task 0)

As of 2026-11-16 `Cluiche/CoW/Editor/` holds only the `CoWEditor` stub (`CoWEditor.h/.cpp/.vcxproj`); there is no `WorldSimInspector/` folder and no UI project.

| Needs | From | Used by tasks |
|---|---|---|
| UI project scaffold (Vite, React 18, zustand, `@dia/editor-ui`, Vitest, RTL), `model/` + `store/` layout | T-1 Feature 1 | 1-12 |
| `Source` model with `groups`, `aggregates()`, `cellQuery()` (rejects `NotTracked`), shared brush, schema tooltips (`ValueTooltip`) | T-1 Feature 1 | 1-4, 6-10 |
| `navigation.ts`, `canNavigate`, toast, `group` and `cell` intents, allow-list in `CoWWorldSimInspectorPlugin` | T-1 Feature 6 | 8-11 |
| `CellGridCanvas`, `CellColouring`, ramp resolver and legend in `@dia/editor-ui` (`Dia/DiaEditorUI/src/CellGridCanvas.tsx`, `cellColour.ts`, `cellGeometry.ts`, exported from `index.ts`) | Dia, built (archived plan `docs/archive/plans/cell-grid-canvas.plan.md`) | 4, 9 |
| DiaValueSchema ramps and palettes (`RampResolver`, `buildLegend`, `PaletteData` exported by `@dia/editor-ui`) | Dia | 4, 9 |

**Do not start Task 1 until Feature 1 is Done** (Feature 6 is needed from Task 8). Tasks 1-4 are pure TS and can start once Feature 1's scaffold exists.

## Sign-offs (AD-001)

None new. `CellGridCanvas` already exists in `Dia/DiaEditorUI` (built and tested), so the spec's "flagged, needs sign-off before build" is moot; this feature only consumes it. A Task 12 fallback (offscreen-cached fill layer) would edit `Dia/DiaEditorUI/src/cellDraw.ts` / `CellGridCanvas.tsx`: **stop and get AD-001 sign-off before doing that edit.** The `track`/`untrack` allow-list addition is Feature 6 / `navigation.md` (CoW-side). `CluicheEditor.vcxproj` and uPlot were signed off 2026-11-05 for the parent system.

## Implementation Patterns

### Models (Tasks 1-4), pure TS, no React, no bridge
- Files in `Cluiche/CoW/Editor/WorldSimInspector/UI/model/`: `breakdown.ts`, `histogram.ts`, `groupCells.ts`, `minimap.ts`, `emptyState.ts`. Views import these, the store and Feature 6's navigation model only.
- `breakdown.ts`: `breakdown(source, groupLayer, layers[<=6], brush) -> rows {group, means[], spark[]}`. Mean over the brush of the group's mean aggregate (`source.aggregates(layer, group, 'mean')`); `spark` is the first chosen layer across the brush. `sortRows(rows, col, dir)`; `addLayer()` refuses a 7th with a message string. `groupLayerChoices(schema, groups)` lists category layers and hides those with more than 64 values (with tooltip reason).
- `histogram.ts`: `histogram(values, range, bins=32) -> {counts, below, above}` (out-of-range goes into first/last bin and is counted in `below`/`above`); NaN skipped and counted; `resolveRange(schema, values)` returns `{min,max,observed:boolean}`; `histogramsByGroup(values, groupCells, limit=12)` returns All cells first, then up to 12 groups by cell count desc, the rest merged into Other with `otherCount`.
- `groupCells.ts`: `groupCells(source, groupLayer) -> Promise<Uint16Array | null>` (cell-to-group membership, one value per cell), read once per world and cached by `(worldId, groupLayer, structure hash)`; null when the source has no per-cell grouping data. **Hook for Feature 10 (space-over-time `regionFill`, region row ordering by mean latitude) and Feature 8/12 consumers: export it from `model/` with a stable signature and cache API (`getGroupCells`, `clearGroupCells`).**
- `minimap.ts`: `cellColours(values, schemaColouring) -> CellColouring` (delegates to `@dia/editor-ui` `CellColouring`; do not reimplement colour maths), `hoverInfo(cell, values, width, schema) -> {q,r,value,unit,description}`, `transformValues(values, fn?)` so Feature 8 can pass an anomaly array, and an optional `highlight: ReadonlySet<number>` passed straight through to `CellGridCanvas.highlight` (Feature 10 tracks, Feature 7 compare). The mini-map component takes `values` and `highlight` as props, never reading the source itself.
- `emptyState.ts`: `selectEmptyState({kind, tracked, hasSample, hasCells}) -> 'ok' | 'untracked' | 'file-no-cells' | 'no-sample-yet'` (Spec table; file kind never gets a button).

### Data and state
- Distribution and mini-map read one `cellQuery(layer, brushEnd)` through a store selector keyed `(source.id, layer, sample)`; stale results are dropped if the brush end moves. `NotTracked` rejection maps to the untracked empty state; chunk failures log `[EDITOR] T1`.
- Store slices (`store/`): `groupLayer`, `breakdownLayers[]`, `distributionLayer`, `minimapLayer`, `trackedByInspector` (so only layers T-1 tracked are untracked).
- Track button: sends `cells.history.track`/`untrack` via Feature 6's allow-list and plan type; states cost first (per-sample bytes at Large from the 64 MB budget), disabled in the same cases as navigation, shows Feature 6's failure toast on refusal.

### Views (Tasks 8-11)
- `BreakdownTable` (virtualised list from the start; use an `@dia/editor-ui` list component if one exists, Task 0 checks, otherwise a small local windowing hook), `Distribution` (small multiples, shared x axis), `HexMiniMap` (wraps `CellGridCanvas`; maps its `onHover` and `onCellClick` to `ValueTooltip` and a `cell` intent at the brush end sample), `UntrackedNotice`.
- Disabled controls stay visible with a model-supplied tooltip reason; never computed in the view.

### Verification cadence
- Per task: scoped Vitest (`npx vitest run <file>` from the UI folder). The only C++ touch is none for this feature, so GoogleTests appear only in the single full run (Task 13).
- Full Vitest, `dia run googletest --shards 2` and E2E run once, in Task 13. No mutation checks (decided 2026-11-12). Task 14 is a gap audit plus edge tests only.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Gate: confirm T-1 Feature 1 is Done (scaffold, `Source`, brush, `ValueTooltip`) and Feature 6 status; re-verify `CellGridCanvas` in `Dia/DiaEditorUI/src/` exports `topology, width, height, values, colouring, highlight, onCellClick` and the hover callback; check for a list component in `@dia/editor-ui`; record baseline Vitest counts (UI project and `Dia/DiaEditorUI`) | - | Not Started | haiku | No code. If Feature 1 is not Done, stop and report. Known now: canvas exists but the hover prop is `onHover` (spec says `hover`), `colouring` takes `ResolvedRamp`/`PaletteData` (spec says `Ramp`/`Palette`), and extra props exist (`selection, outline, footprint, view, onBox, onStroke`) |
| 1 | RED -> GREEN: `histogram.ts` bins, range resolution, out-of-range and NaN counts | AC 6, 8, 9: edge values at min/max, 0 and 1 cell, out-of-range in first/last bin with footer counts, no schema range uses observed range and flags it | Not Started | sonnet | Pure TS |
| 2 | RED -> GREEN: `histogramsByGroup` limit 12 + Other, All cells first; `groupCells.ts` read-once cache and null case | AC 6, 7: 13 and 30 groups merge into Other with count; ties by id; cache hit on same `(world, layer, structure)`, miss on world change | Not Started | sonnet | `groupCells` is reused by Feature 10 (`regionFill`, row order by mean latitude); keep its signature stable |
| 3 | RED -> GREEN: `breakdown.ts` means over brush, sparkline, sort, 6-column cap, group-layer choices (hide >64 values) | AC 1-4: hand-computed fixture over brush; brush change updates means; 7th column refused with message; sort asc/desc by any column; units/labels from schema; 30 groups x 6 layers x 2,080 samples within 50 ms | Not Started | sonnet | Aggregates only, works for every source |
| 4 | RED -> GREEN: `minimap.ts` (`cellColours`, `hoverInfo`, value-array transform, highlight passthrough) and `emptyState.ts` | AC 10-12: ramp and palette colouring via `CellColouring`; hover gives (q,r), value, unit, description; empty-state choice per situation (live untracked, file without cells, tracked not yet sampled, ok); transform hook returns a new array without mutating input | Not Started | sonnet | Hooks: Feature 8 anomaly values, Feature 10 tracks, Feature 7 compare overlay |
| 5 | Store slices: `groupLayer`, `breakdownLayers`, `distributionLayer`, `minimapLayer`, `trackedByInspector`; per-cell fetch selector with stale-drop on brush move | Vitest on store: brush end change refetches; superseded fetch dropped; `NotTracked` sets untracked state; chunk failure logs | Not Started | sonnet | Depends on Feature 1 store layout |
| 6 | Per-cell fetch + `groupCells` read through `Source.cellQuery` (live chunked, file with `--cells`); add `groupCells` accessor to `Source` if Feature 1 lacks it | AC 12, 13: reassembly of chunks, bad chunk gives clear error, file without cells gives `file-no-cells` | Not Started | sonnet | Spec assumes `groupCells` exists on the Source; Feature 1's `Source` only has `groups: GroupInfo[]`, so the membership read is new. It is CoW-side, no sign-off |
| 7 | `UntrackedNotice` with Track button: cost text, same disable rules as navigation, toast on refusal, untrack only own layers | RTL AC 12: three empty-state texts exact; no button for file; Track sends `cells.history.track` plan; disabled with reason when offline | Not Started | sonnet | Requires `track`/`untrack` in Feature 6's allow-list (Feature 6 amended); if absent, stop and report |
| 8 | `BreakdownTable`: group-by picker, column picker with limit message, sort, sparklines, virtualised rows, row click raises `group` intent, local select + reason tooltip when navigation disabled | RTL AC 1-5, 13: picker lists every category layer; 7th column message; sort; click raises `group`; disabled-navigation tooltip | Not Started | sonnet | Visual check vs v2 Overview region table and v1 breakdown |
| 9 | `Distribution` small multiples and `HexMiniMap` on `CellGridCanvas` with legend, `ValueTooltip` hover, `cell` intent at brush end; props accept `values` transform and `highlight` set | RTL AC 6-7, 10-13: histogram rows + footer; legend from schema; hover and click raise right intent; moving brush end updates both | Not Started | sonnet | Visual check vs v2 Space workspace mini-map and v1 views. Grid topology/size from source, not hard-coded |
| 10 | Wire the three views into the shell (Overview region table, Space mini-map slots); `[EDITOR] T1` logs (query chunk failures, fallback range, Other merges); `performance.mark` around histogram build and map draw | RTL + Vitest: views render from fixture source; log lines emitted; marks only in dev builds | Not Started | sonnet | Depends on Feature 1 shell and v2 layout |
| 11 | Visual gate: compare each state against both approved mockups, one screenshot per state group (breakdown, distribution with >12 groups, mini-map, three empty states) | AC 14 | Not Started | sonnet | Report mismatches, fix in the owning task's files |
| 12 | `CellGridCanvas` follow-up: measure 104x64 hex, all layers, on the editor's GPU-backed canvas (DevTools Performance paint cost, not JS call time) against the 16 ms budget; 6,656-cell histogram + map within a frame | `cell-grid-canvas.md` AC 6 and this spec's Perf test | Not Started | sonnet | If it misses 16 ms, fall back to an offscreen-cached fill layer: that edits `Dia/DiaEditorUI`, so get AD-001 sign-off first. Report pass/fail and numbers |
| 13 | Full verification, once: full Vitest, `dia run googletest --shards 2`, E2E scenario with a stub game publishing fixture history (breakdown, distribution, click raises commands, untracked state) | All ACs end to end | Not Started | sonnet | `dia test e2e` does not build: run `dia pipeline --target cluichetest --stage compile-code` first. Report pass/fail and failing names only |
| 14 | Gap audit and edge tests (no mutation checks): re-read ACs against tests; add empty source, single-cell grid, all-NaN layer, group of 0 cells, world change mid-fetch, brush end beyond samples, square topology | Added tests fail first where they find a bug | Not Started | sonnet | Explicit task per the exhaustive-test rule |
| 15 | Close-out: Observation Opportunity Scan (suggestions only), `dia docs spec-done docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md` (add `--archive-plan` if wanted), update parent features table, backlog row to history, note `groupCells` hook for Feature 10 | - | Not Started | haiku | Scan findings do not block Done |
