# Feature Spec: T-1 Snapshots, Cell Compare and History Status

**Parent:** @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md
**Builds on:** @docs/specs/applications/cow/systems/t1-worldsiminspector/plugin-shell-and-sources.md (Feature 1), @docs/specs/applications/cow/systems/t1-worldsiminspector/breakdown-distribution-minimap.md (Feature 3), @docs/specs/applications/cow/systems/t1-worldsiminspector/navigation.md (Feature 6)
**Engine side (built):** @docs/specs/applications/dia/systems/diacellhistory/snapshots.md, @docs/specs/applications/dia/systems/diacellhistory/compare.md, @docs/specs/applications/dia/systems/diacellhistory/recorder-and-tiers.md

**Gameplay Domains:** debug

## Summary

Re-homes the DiaCellHistory in-game panel work that was never built into the CluicheEditor Inspector. Three parts: (1) a **snapshot panel** (list, take, delete, restore), (2) **cell compare** on the hex mini-map (snapshot vs now, snapshot vs snapshot, sample vs sample, drawn as a delta ramp, computed in the editor), and (3) a **history status strip** (event-type legend, tracked layers and per-tier memory against budget, gaps) with Track/Untrack controls. The engine commands, queries, topics and export are all built; this feature is UI, a typed model and one bridge handler.

## Problem

DiaCellHistory was closed engine-complete with its panel deferred. Today a user can only take, restore and compare snapshots by typing console commands, and cannot see how full the history tiers are or where gaps are. The question "what changed because of that drought?" needs a known state kept, the world run on, and a visual diff. The user decided (2026-11-04) these controls live in the Inspector, not the in-game panel.

## Goals

1. **Snapshot panel.** List via `cells.snapshot.list`: name, sample, label, world, size, `sameRun`, status `pending|ready|failed` with reason; orphans shown as failed. Take, Delete and Restore, each a wire command. Restore shows a confirm stating the consequence.
2. **Cell compare.** Snapshot vs now, snapshot vs snapshot, sample vs sample for one chosen layer, drawn on the mini-map with a symmetric delta ramp, plus a per-group table (mean A, mean B, change) sorted by absolute change.
3. **History status strip.** Event-type legend swatches, per-tier memory against budget (cells, aggregates, events), tracked layer list with Track/Untrack, and gap markers. Fed by `cells.history.status`.
4. **Safe by gating.** Take, Restore, Delete, Track and Untrack need a live connection with a matching world id (the Feature 6 rule). File sources show snapshots read-only.
5. **Rules visible inline.** Name rule (1-32 of `A-Za-z0-9_-`), the 16-snapshot cap and the one-operation-in-flight rule are shown where they apply, and the control disables with the reason instead of failing.
6. **UI separate from logic.** A typed command and compare model; views never build command strings.

## Non-Goals

- Re-simulating to a snapshot. Restore loads stored state only (CH-002).
- Run-vs-run compare of aggregates (Feature 5).
- Selecting cells from the compare view beyond the existing mini-map click (Feature 6's `cell` intent).
- In-game panel versions of any of this (the user ruled them out).
- Writing snapshot or history files directly. Everything goes through the game's commands.
- Editing or tuning any simulation value.

## Architecture

```
model/snapshots.ts   (pure TS)
  SnapshotRow, parseList(reply), canTake(name, list, inFlight) -> {ok, reason?}
  restoreConsequence(row, meta) -> { kind: 'truncate', dropped } | { kind: 'reset' } | { kind: 'refused', reason }
  plan: take | delete | restore -> CommandPlan    (shared allow-list with navigation.ts)
model/compare.ts     (pure TS)
  CompareSide = { kind: 'now' } | { kind: 'sample', n } | { kind: 'snapshot', name }
  delta(layer, a, b) -> Float32Array + stats      (NaN-aware, "no data" cells kept as NaN)
  DeltaScale: fixed | auto  (grow at once, shrink after 2 s under 40%)
  groupTable(delta, groupLayer) -> rows sorted by |change|
model/historyStatus.ts (pure TS)
  parseStatus(reply) -> { tiers: [{name, bytes, budget, pct}], tracked, gaps, legend }
store/       snapshots, compareSides, deltaScale, historyStatus
views/       SnapshotPanel, ConfirmRestore, CompareControls, DeltaMiniMap (CellGridCanvas), GroupDeltaTable, HistoryStatusStrip
bridge       C++ CoWWorldSimInspectorPlugin: allow-list extended (below)
```

Views call `model/` and the store only. The compare reads cell data through the existing Feature 3 `Source.cellQuery()` (`cells.history.cells`) for samples and through `cells.history.snapshot {name, layer, offset, count}` (chunked) for snapshots, so there is no new data path.

### Commands (allow-list additions)

| Action | Command |
|---|---|
| List | `cells.snapshot.list` |
| Take | `cells.snapshot.take <name> [overwrite]` |
| Delete | `cells.snapshot.delete <name>` |
| Restore | `cells.snapshot.restore <name>` |
| Snapshot cell data | `cells.history.snapshot {name, layer, offset, count}` |
| Status | `cells.history.status` |

`cells.history.track` / `untrack` already exist on the Feature 6 allow-list; this feature adds the UI. T-1 untracks only layers it tracked itself (Feature 6 AC 11).

### Restore confirm

| Case | Behaviour |
|---|---|
| Same run | Confirm: "History after sample S is dropped (N samples)." |
| Earlier session (foreign run) | Confirm: "History is reset; numbering restarts at 0." |
| Other grid | Restore button disabled, reason shown (the engine refuses before calling the store) |

### Compare

- Layers offered are those with per-cell history (tracked) or present in both snapshots; otherwise "Track this layer" (Feature 3 pattern).
- Delta ramp is symmetric around zero. Scale is fixed (user-set) or auto: **grows at once, shrinks only after 2 s under 40%**, the same rule as DiaCellHistory Feature 4.
- A cell with no data on either side stays NaN and draws as the "no data" colour, never zero.
- The per-group table groups by a chosen grouping layer (region or biome) and shows mean A, mean B, change, sorted by absolute change.

### Status strip

Reads `cells.history.status` on connect and on topic activity, throttled to once per second. Shows memory per tier against budget (amber over 75%, red over 90%, matching the engine health rule), tracked layers with Track/Untrack, gap markers, and a legend of event types with swatches from the schema (`kind: event`).

### Gating

`canMutate` is Feature 6's `canNavigate` plus: not while a take or restore is in flight (tracked from `pending` rows and the in-flight error). Disabled controls stay visible with a tooltip reason. A file source shows its `snapshots` list (`{name, sample, label}`) read-only; compare of file snapshots is out of scope for v1.

### Logging and observation

Channel `[EDITOR] T1`. Logged: take/restore/delete requested and result, rejected commands, compare computed (layer, sides, cells, ms), status poll failures. UI timings via `performance.mark` in dev builds.

## Acceptance Criteria

1. The snapshot list shows name, sample, label, world, size, `sameRun` and status; `failed` rows show their reason and orphans show as failed.
2. Take sends `cells.snapshot.take` with the entered name; an invalid name or a full list (16) disables Take with the reason, and `overwrite` is only sent when the user confirms replacing an existing name.
3. While one take or restore is pending, Take, Restore and Delete are disabled with "operation in progress".
4. Restore of a same-run snapshot confirms with the number of samples that will be dropped; of an earlier-session snapshot confirms a history reset; of another grid is disabled with the engine's reason. Nothing is sent before the user confirms.
5. Delete sends `cells.snapshot.delete` after a confirm.
6. Compare supports snapshot vs now, snapshot vs snapshot and sample vs sample for any layer available on both sides, drawn on the mini-map with a delta ramp symmetric around zero.
7. Auto scale grows immediately when a larger delta appears and shrinks only after 2 s under 40% of the current extent; fixed scale never changes.
8. Cells with no data on either side render as "no data", not zero, and are excluded from group means.
9. The group table shows mean A, mean B and change per group and is sorted by absolute change; group means match a hand-computed fixture.
10. Take, Delete, Restore, Track and Untrack are disabled with a reason unless the game is connected and, for a file source, the world id matches. A file source lists snapshots read-only.
11. The status strip shows each tier's bytes against budget with the 75% and 90% colour thresholds, tracked layers, gap markers and the event legend.
12. Track and Untrack from the strip send the Feature 6 commands and update the strip; T-1 untracks only layers it tracked.
13. The bridge handler rejects any command not on the extended allow-list; no command string appears in a view.
14. A failed command shows a toast with the reason and leaves T-1 state unchanged (Feature 6 rule).
15. Compare of one float layer at Large (104x64 hex) renders within the 16 ms frame budget on `CellGridCanvas`, and the data fetch is chunked under the message cap.

## Test Strategy

- **Vitest (model/):** list parsing (all statuses, orphans); name and cap validation; restore consequence for the three cases; delta of known fixtures including NaN sides; auto-scale grow and shrink with a fake clock; group table means and ordering; status parsing and threshold colours; allow-list contains only listed commands.
- **React Testing Library:** panel states from fixtures (pending, ready, failed, orphan); confirm dialogs and "nothing sent before confirm"; disabled-with-reason for each gate; delta mini-map renders from a fixture; status strip colours.
- **GoogleTests (C++):** the bridge handler accepts the new commands and rejects others; gating when the world id does not match; chunked `cells.history.snapshot` reassembly.
- **E2E:** stub game with fixture snapshots; take then list shows the row; restore confirm text matches the case; an in-flight stub disables the controls; loading a different world's file disables mutations.
- **Perf:** AC 15 on a Large fixture.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CoW AD-002 | Four-tier placement | All code in `Cluiche/CoW/Editor/WorldSimInspector/` |
| AED-005 | React and DiaUICEF UI stack | React 18 + zustand + `@dia/editor-ui`, `CellGridCanvas` |
| Tool tiers (Inspector) | Inspector reads a live game and does not tune it | **CONFLICT, see Open Design Question 1.** Restore rewrites live game state through E1, which is not observation |
| Navigation AC 11 | Only listed commands plus track/untrack can be sent | Needs an amendment to Feature 6's allow-list (listed above) |
| UI/business separation | Typed model first | `snapshots.ts`, `compare.ts`, `historyStatus.ts` |
| Editor actions via DiaAPI | Actions are DiaAPI commands | Every action is a `cells.*` command; the panel calls the same ones as the console |
| CH-002 (DiaCellHistory) | Restore is a store load, no re-simulation | Restore uses `cells.snapshot.restore` only |

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaCellHistory Features 1, 3, 4, 5, 6 | Status, snapshots, compare queries, export, catch-up | Done (engine scope, 2026-11-04/05) |
| CoW `ISnapshotStore` adapter over E1 (E1 Feature 6) | Game-state take and restore | **Approved 2026-11-05, not built.** Take and Restore return an error until it exists; list, compare of samples and the status strip work without it |
| T-1 Features 1, 3, 6 | Source model, mini-map canvas, bridge and gating | Approved, not built |
| `docs/mockups/world-sim-inspector.html` | Visual reference | **Extended 2026-11-16: new "Snapshots & Compare" tab with 17 switchable states. Approved by the user 2026-11-16 (visual gate passed)** |

**Amendments when Approved:**
- **Feature 6 (navigation):** extend the bridge allow-list with the six commands above.
- **Parent spec:** add Feature 7 to the features table; the "Proposed Amendment" section is resolved (mark it approved 2026-11-16 and point here).
- **Mockup:** port the snapshot states from `docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html` into `world-sim-inspector.html`, add the compare view and the status strip, and get the user's approval **before any UI task** (editor mockup rule).

## Open Design Questions

1. **Restore is not read-only.** The Inspector tier is "live game, observe and navigate". Restore rewrites game state through E1, and Take writes snapshot files. Proposed resolution: the tier rule's real line is "never edit simulation parameters"; snapshots are a recording and time-travel facility, the same class as `track`/`untrack`, and the engine already confines them to `ISnapshotStore`. Confirm, or restrict v1 to list and compare only with Take and Restore left to the console.
2. **Snapshot data volume for compare.** `cells.history.snapshot` returns one layer per call, chunked under the 15 KB reply cap. A Large float layer is 6,656 cells at 16 bits, about 13 KB, so roughly 1 to 2 chunks, but snapshot vs snapshot of several layers at once is unmeasured. Decide whether to cache fetched layers per snapshot name and invalidate on delete or overwrite.
3. **Status polling vs topic.** `cells.history.status` is a query. If the history topics do not carry tier bytes, a 1 s poll is needed. Check whether a `status` topic exists before building; if not, request one from DiaCellHistory rather than polling forever.

## Status

`Approved` (2026-11-16). Parent amendment approved the same day. Open Question 1 was resolved by approval: snapshots are a recording and time-travel facility like `track`/`untrack`, so Take and Restore stay in v1 (the tier rule's line is "never edit simulation parameters"). Questions 2 and 3 stay open for implementation. Plan: [snapshots-and-compare.plan.md](snapshots-and-compare.plan.md) (written 2026-11-16, nothing started; gated on T-1 Features 1, 3 and 6). Build needs the CoW `ISnapshotStore` adapter only for Take and Restore.
