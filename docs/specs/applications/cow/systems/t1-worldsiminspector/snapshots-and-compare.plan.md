# Plan: T-1 Snapshots, Cell Compare and History Status (Feature 7)

**Spec:** @docs/specs/applications/cow/systems/t1-worldsiminspector/snapshots-and-compare.md
**Status:** Not Started
**Mockup (approved 2026-11-16):** `mockups/world-sim-inspector.html`, tab "Snapshots & Compare" (17 switchable states). The visual gate for every view task.

## Prerequisites (gate, checked in Task 0)

Feature 7 builds on pieces that do not exist yet. As of 2026-11-16 `Cluiche/CoW/Editor/` holds only the `CoWEditor` stub; there is no `WorldSimInspector/` folder and no UI project.

| Needs | From | Used by tasks |
|---|---|---|
| UI project scaffold (Vite, React 18, zustand, `@dia/editor-ui`, Vitest, RTL) and `model/` + `store/` layout | T-1 Feature 1 | 1-6, 8-12 |
| `Source` model with `cellQuery()`, shared brush, schema tooltips | T-1 Feature 1 | 3, 6, 10 |
| `CoWWorldSimInspectorPlugin` bridge call and allow-list, `canNavigate`, toast, `navigation.ts` | T-1 Feature 6 | 2, 7, 12 |
| Hex mini-map on `CellGridCanvas`, tracked-layer "Track this layer" pattern | T-1 Feature 3 | 10, 11 |
| CoW `ISnapshotStore` adapter over E1 (E1 Feature 6) | CoW | 16 only (live Take/Restore proof) |

**Do not start Task 1 until Features 1, 3 and 6 are Done.** Tasks 1 to 5 are pure TS, so they can start as soon as Feature 1's scaffold exists, even if 3 and 6 are not finished. Task 16 stays Blocked until the adapter is built.

## Sign-offs (AD-001)

None new. All code is CoW editor code (`Cluiche/CoW/Editor/WorldSimInspector/`). The Feature 6 allow-list change is inside the same plugin. The `CluicheEditor.vcxproj` reference and uPlot were signed off 2026-11-05 for the parent system.

## Implementation Patterns

### Models (Tasks 1-5), all pure TS, no React, no bridge
- Files in `model/`: `snapshots.ts`, `compare.ts`, `deltaScale.ts`, `historyStatus.ts`. Views import these and the store only (UI/business separation rule).
- `snapshots.ts`: `parseList(reply) -> SnapshotRow[]` (status `pending|ready|failed`, `orphan`, `sameRun`, `grid`); `nameCheck(name, rows, inFlight, gate)` returns the first failing reason in this order: gate, cap (16), empty, name regex `^[A-Za-z0-9_-]{1,32}$`; `restoreConsequence(row, meta, activeGrid)` returns `{kind:'truncate', dropped} | {kind:'reset'} | {kind:'refused', reason}`; `planFor(action, row)` returns a `CommandPlan` using Feature 6's plan type.
- Command strings live in one `SNAPSHOT_COMMANDS` table next to `navigation.ts`'s table; the C++ allow-list is generated from or checked against the same names by a shared fixture (a test reads both lists).
- `compare.ts`: `CompareSide = now | sample(n) | snapshot(name)`; `delta(a: Float32Array, b: Float32Array) -> {values, nanMask, min, max}` where NaN on either side stays NaN; `groupTable(delta, groupValues)` ignores NaN cells in means and sorts by `abs(change)` descending, ties by group name.
- `deltaScale.ts`: a small class taking an injected `now()` clock. Grow immediately when `max(abs) > extent`. Shrink only after `max(abs) < 0.4 * extent` has held for 2000 ms, then to the new max. Same rule as DiaCellHistory Feature 4; do not reimplement differently.
- `historyStatus.ts`: `parseStatus(reply)`; `tierLevel(pct)` returns `ok` below 75, `amber` from 75, `red` above 90 (matches the engine's Degraded-over-90% rule); the event legend comes from the schema's `kind: event` entries.

### Data fetch (Task 6)
- Snapshot cell data: `cells.history.snapshot {name, layer, offset, count}` chunked under the 15,000-byte reply cap, reassembled into one `Uint16Array`, decoded with the conformance rule `min + q/65534 x (max - min)`, `q = 0xFFFF` is NaN (conformance fixtures in `Dia/DiaCellHistory/Testing/Conformance/`).
- Cache decoded layers per `(snapshotName, layer)`; invalidate on delete, overwrite and world change (Open Question 2 decides if the cache is kept after measurement).
- Status: Task 6's spike decides topic vs 1 s poll (Open Question 3). A poll is throttled to once per second and paused while the tab is hidden.

### Bridge (Task 7)
- C++ `CoWWorldSimInspectorPlugin` allow-list gets the six commands from the spec. Unlisted names are rejected as today. GoogleTests cover accept, reject and gate (world mismatch).

### Views (Tasks 9-12)
- Match the mockup structure: `SnapshotPanel`, `ConfirmRestore` (three variants), `CompareControls`, `DeltaMiniMap` (on `CellGridCanvas`, diverging ramp `#3b6fd4` / `#2e2e2e` / `#e08a3c`, hatched NaN), `GroupDeltaTable`, `HistoryStatusStrip`.
- Disabled controls stay visible with a tooltip reason from the model, never computed in the view.
- Nothing is sent before a confirm; Restore and Delete go through `ConfirmRestore` / a delete confirm that call the store action only on the confirm button.

### Verification cadence
- Per task: scoped Vitest (`npx vitest run <file>` from the UI folder) or scoped GoogleTests (`dia run googletest --filter="*WorldSimInspector*" --shards 2`).
- Full GoogleTests, E2E and the perf check run once, in Tasks 14 and 17.
- No mutation checks (decided 2026-11-12). Task 15 is a gap audit plus edge tests only.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Gate: confirm T-1 Features 1, 3 and 6 are Done (scaffold, `Source`, mini-map canvas, bridge, `navigation.ts`); if not, stop and report. Record baseline: current Vitest and `*WorldSimInspector*` GoogleTests pass counts | — | Not Started | haiku | No code. Blocks everything |
| 1 | RED → GREEN: `snapshots.ts` list parsing, `nameCheck`, gate reasons | AC 1-3: all three statuses and orphans; name regex edges (0, 1, 32, 33 chars, bad chars); 16 cap; in-flight; each gate reason precedence | Not Started | sonnet | |
| 2 | RED → GREEN: `restoreConsequence` and `planFor` (take, delete, restore) with the shared `SNAPSHOT_COMMANDS` table; `overwrite` only on confirmed replace | AC 2, 4, 5: same run gives dropped count; earlier session gives reset; other grid gives refused with the grid sizes; plans contain only listed commands | Not Started | sonnet | Extends Feature 6's command table (CoW-side, no sign-off) |
| 3 | RED → GREEN: `compare.ts` delta, NaN handling, `groupTable` | AC 6, 8, 9: hand-computed fixture; NaN on one or both sides; all-NaN group; ordering by abs change with ties; sample vs sample, snapshot vs now, snapshot vs snapshot | Not Started | sonnet | |
| 4 | RED → GREEN: `deltaScale.ts` with fake clock | AC 7: grow at once; no shrink at 39% for 1.9 s; shrink at 2.0 s; a new large delta during the window resets it; fixed never changes | Not Started | sonnet | |
| 5 | RED → GREEN: `historyStatus.ts` parse and thresholds; event legend from schema; Track/Untrack plans that untrack only layers T-1 tracked | AC 11, 12: 74/75/90/91% boundaries; gaps; layer tracked by the game is not untrackable from T-1 | Not Started | sonnet | |
| 6 | **Spike then build:** (a) is there a history status topic, or is polling needed (Open Q3)? (b) measure snapshot-vs-snapshot fetch of 6 layers at Large (chunk count, ms) and decide the cache (Open Q2). Then implement chunked `cells.history.snapshot` fetch + decode + cache in `Source` | AC 15 data half: reassembly of N chunks, bad chunk gives a clear error, decode matches the conformance fixtures; spike numbers quoted in notes | Not Started | sonnet | Record both open-question answers in the spec's Open Design Questions |
| 7 | C++: extend the bridge allow-list with the six commands; world-mismatch gating covers them | AC 10, 13: accepts each listed command, rejects others (including a near-miss like `cells.snapshot.export`), refuses mutations on world mismatch, accepts read-only list on a file source | Not Started | sonnet | GoogleTests; `dia run googletest --filter="*WorldSimInspector*"` |
| 8 | Store slices: `snapshots`, `compareSides`, `deltaScale`, `historyStatus`, in-flight tracking, toast on failure | Vitest on the store: take then pending then ready; failed command leaves state unchanged and sets a toast; world change clears the cache | Not Started | sonnet | AC 14 |
| 9 | `SnapshotPanel` + `ConfirmRestore` (same run, earlier session, refused) + delete confirm, Take box with inline rules | RTL AC 1-5: every mockup state (empty, list, failed + orphan, cap, in flight, offline, file, other world); nothing sent before confirm | Not Started | sonnet | Visual check against the mockup, one screenshot per state group |
| 10 | `CompareControls`, `DeltaMiniMap` on `CellGridCanvas`, `GroupDeltaTable`, scale selector and note | RTL AC 6-9: renders from a fixture; hatched NaN cells; ramp labels follow the scale; table order | Not Started | sonnet | Layers not tracked show "Track this layer" (Feature 3 pattern) |
| 11 | `HistoryStatusStrip`: tier bars with thresholds, tracked chips with Track/Untrack, gaps, event legend | RTL AC 11-12: colours at boundaries; chips disabled with reason when not live; Track sends the plan | Not Started | sonnet | |
| 12 | Wire into the plugin shell as the "Snapshots & Compare" tab; gating via `canMutate`; failure toasts; `[EDITOR] T1` logs | RTL + Vitest: tab switches without losing Analysis state; gate reasons shown; log lines for take, restore, delete, compare | Not Started | sonnet | |
| 13 | E2E scenario: stub game with fixture snapshots | AC E2E list: take then list shows the row; restore confirm text per case; in-flight stub disables controls; different-world file disables mutations | Not Started | sonnet | `dia test e2e` does not build: run `dia pipeline --target cluichetest --stage compile-code` first (Loose End) |
| 14 | Perf check AC 15 on a Large (104x64) fixture; full Vitest and GoogleTests run once | AC 15: compare render within 16 ms on `CellGridCanvas`; fetch chunked under the cap; full suites pass with `--shards 2` | Not Started | sonnet | If it misses 16 ms, fall back to the offscreen-cached fill layer noted in the parent spec. Report pass/fail only |
| 15 | Gap audit and edge tests (no mutation checks): re-read the spec ACs against the tests, add missing edges (empty list, 16-name boundary, unicode names, simultaneous fast clicks, world change mid-fetch, status poll failure) | Added tests fail first where they find a bug | Not Started | sonnet | Explicit task per the exhaustive-test rule |
| 16 | Live proof of Take and Restore: take "pre-drought", force a drought, restore, compare A1 state hash | Manual / E2E against the real CoW build | Blocked | sonnet | Blocked on the CoW `ISnapshotStore` adapter (E1 Feature 6). Everything else is shippable without it |
| 17 | Close-out: Observation Opportunity Scan, spec Status to Done (`dia docs spec-done`), update parent features table, backlog row to history, Feature 6 allow-list note | — | Not Started | haiku | Scan findings are suggestions only. Spec flips Done only if Task 16 is Done, otherwise Done with the adapter-gated item recorded as a backlog row |
