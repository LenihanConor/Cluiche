# Feature Spec: E1 Snapshot Store Adapter

**Parent:** @docs/specs/applications/cow/systems/e1-saveload/e1-saveload.md
**Builds on:** @docs/specs/applications/dia/systems/diacellhistory/snapshots.md (defines `ISnapshotStore`; its Feature 3 is Done against a fake store)

**Gameplay Domains:** persistence

**Status:** `Done`

## Summary

Gives DiaCellHistory snapshots a real game-state store in CoW. It delivers:

- a CoW **`SnapshotStoreAdapter`** implementing `Dia::CellHistory::ISnapshotStore` over E1's `SaveLoadModule`: take and restore a named game state at the sim-frame boundary, in `out/saves/cow/snapshots/<name>.sav`, outside E1's slots;
- the small **`SaveLoadModule` additions** it needs (a snapshot save/load path with a directory override, public pending-flag use, the snapshot metrics);
- the read-only **`SaveLoadModule::CaptureState()`** that S-3.5 Regression hashes to compare whole-world state (the amendment already described in the E1 spec).

DiaCellHistory keeps the per-cell copies; CoW (this feature) keeps the game state. Neither touches the other's files.

## Problem

DiaCellHistory's `cells.snapshot.take` and `cells.snapshot.restore` need a game-supplied store to save and load game state, and CoW has none: only a test fake exists, so snapshots cannot capture or restore real CoW state, and the T-1 snapshot controls and S-3.5's R3 snapshot-restore check have nothing real to run against.

## Goals

1. **Real take and restore.** A snapshot saves and restores every registered E1 participant, by name.
2. **Never disturb the user's save or load.** E1 holds one pending save and one pending load; a snapshot request never overwrites either.
3. **Outside the slots.** Snapshot files never count toward E1's slots or appear in `cow.save.list`.
4. **Same restore rules as a load.** Pre-validation, atomic-load behaviour and the same-frame `RestoreState` / DiaSimTime clock restore all apply.
5. **Safe names.** A snapshot name can never write or read outside `snapshots/`.
6. **Read-only capture.** `CaptureState()` returns participant states without writing anything.

## Non-Goals

- Per-cell copies, naming rules and the cap of 16 (DiaCellHistory owns them; they count files from earlier sessions).
- Enumerating game state files that have no cell file (the `ISnapshotStore` interface has no enumerate; see Dia follow-up 11 in the backlog).
- A snapshot UI (T-1 Feature 7).
- Async I/O, compression, migration testing (same deferrals as E1).
- Changing what participants save.

## Architecture

CoW code only (`Cluiche/CoW/Modules/`). No new Dia header is added; `ISnapshotStore` already exists in Dia.

```
SnapshotStoreAdapter   implements ISnapshotStore; reaches SaveLoadModule through a ModuleRef; runs at the frame boundary
SaveLoadModule         gains snapshot save/load execution (directory override) and CaptureState()
```

### Flow

```
RequestTake(name, cb, user)
  1. validate name (Safe names, below); on failure call cb(false, reason) at the next boundary
  2. queue the request (one at a time; DiaCellHistory also serialises)
  3. at each frame boundary: if SaveLoadModule HasPendingSave() or HasPendingLoad(), wait (up to the defer limit N);
     otherwise save to snapshots/<name>.sav through the same SaveManager path as a slot save, then cb(ok, message, user)

RequestRestore(name, cb, user)
  same, using the pre-validated load path. A pre-validation failure leaves the game untouched and calls cb(false, reason).
  A successful restore runs RestoreState in the same frame as the DiaSimTime clock restore, as any load does.
```

The adapter runs from `SaveLoadModule`'s own update (which is ordered before `GameSpeedControllerModule`) or from a module ordered right after it. Either way it executes only at the frame boundary, where no system is mid-tick.

### Directory override

The snapshot save and load reuse the existing `SaveManager` and its participants, with the folder set to `out/saves/cow/snapshots/` for that call only. The slot cap does not apply. How the override is done (swapping the config for one call inside CoW, versus an optional directory argument in DiaSaveGame) is Open Design Question 2.

### Safe names

A snapshot name is accepted only if it is non-empty, at most 63 characters, and contains only letters, digits, `_` and `-`. Separators, `..`, dots and spaces are rejected, so the file is always `snapshots/<name>.sav` directly under the folder. (E1's own slot ids already pass `SlotManager::IsValidSlotId`; the adapter reuses that check if its rules are equal or stricter.)

### `CaptureState()`

```cpp
// Returns each registered participant's id and the Json::Value it would write, in registration order.
// Writes no slot, touches no disk, changes neither the slot count nor the pending flags.
// Runs at the frame boundary like a save; calling it never changes the game.
void CaptureState(CapturedStateList& out) const;
```

### Instrumentation

Counters `cow.save.snapshot_takes`, `cow.save.snapshot_restores`, `cow.save.snapshot_failures` (with `MetricInfo`), and a log line on each failure. `SaveLoadHealthReporter` goes Degraded after a failed snapshot take or restore, as for a slot save or load.

## Acceptance Criteria

1. `RequestTake(name)` writes the game state to `out/saves/cow/snapshots/<name>.sav` at the next frame boundary and calls back with `ok = true`.
2. `RequestRestore(name)` loads that file through the pre-validate-then-load path, calls back with `ok = true`, and `RestoreState` runs in the same frame as the DiaSimTime clock restore.
3. A missing, corrupt or incompatible snapshot file leaves the game untouched and calls back with `ok = false` and a reason.
4. If `cow.save` or `cow.load` is pending, the snapshot request defers to later frame boundaries and never overwrites the user's request.
5. A deferred request that cannot run within N frames calls back with `ok = false` and a reason.
6. Snapshot files do not count against E1's slots: `cow.save.list` ignores them and the slot cap is unaffected.
7. `Exists(name)`, `Delete(name)` and `Folder()` work against `snapshots/`; `Folder()` is where DiaCellHistory puts its `snapshot_<name>.cells.json` files.
8. A snapshot name that is not a valid file name (separators, `..`, empty, over 63 characters, other characters) is rejected with `ok = false` and never touches the disk outside `snapshots/`.
9. The callback fires exactly once per request, on SimPU, at the frame boundary.
10. `CaptureState()` returns each participant's id and JSON in registration order and writes no slot and does no disk I/O.
11. `CaptureState()` never changes game state, the slot or save/load counters, or the pending flags.
12. The snapshot take/restore/failure counters and the failure log lines are in place.

## Tasks

1. Snapshot save and load in `SaveLoadModule` with the directory override (AC 1, 2, 3, 6).
2. `SnapshotStoreAdapter`: name validation, queueing, deferral with limit N, callbacks, `Exists`/`Delete`/`Folder` (AC 4, 5, 7, 8, 9).
3. Wiring: construct the adapter, hand it to DiaCellHistory's `SnapshotManager`, `cow_main.diaapp` and `CoW.vcxproj` entries.
4. `CaptureState()` (AC 10, 11).
5. Metrics, health and logs (AC 12).
6. Exhaustive pass: gap audit, edge tests (names, empty participants, a request during a user save/load, deferral expiry), sanitizer.

## Test Strategy

GoogleTests with a fake `ISaveable` and the real `SaveLoadModule` over a temp folder:

- Take then restore round trip; mutate state between; state comes back.
- Each failure path of ACs 3, 5 and 8, with the game unchanged.
- Deferral: a pending `cow.save` or `cow.load` is not overwritten and runs first; N expiry.
- Snapshot files absent from `cow.save.list`; the slot cap unaffected after 16 snapshots.
- Callback exactly once; thread is SimPU.
- `CaptureState()` order, no disk write (folder unchanged), no counter change.
- A live smoke through `dia run cow` is the manual check E1 still owes (CoW has no headless launch).

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-005 (DiaCellHistory system) | The snapshot store comes from the game | The adapter is CoW code implementing the Dia interface |
| E1 Snapshot namespace | Folder, cap of 16 outside the slots, single pending request, failure and restore-ordering rules | ACs 1 to 6 |
| PD-002 | No singletons or statics | `ModuleRef` to `SaveLoadModule`; no `GetStatic` |
| PD-004 / AD-002 | No STL in public APIs | The adapter and `CaptureState()` use DiaCore containers |
| AD-001 | Dia edits ask permission | Planned as CoW-only; if the directory override needs a DiaSaveGame change (Open Question 2), stop and ask first |

## Open Design Questions

1. **Defer limit N.** How many frame boundaries a snapshot request may wait behind a pending user save or load before it fails. Proposal: 120 frames. Too low fails in a slow 100 MB save; too high lets a stuck request hold DiaCellHistory's one in-flight operation. Revisit once real save times at Large size are measured.
2. **Directory override mechanism.** `SaveManager` holds `const SaveConfig*` and `SlotManager` is built from the config. If swapping the config for one call is enough, this is CoW-only. If `SlotManager` caches the directory, the override needs an optional directory argument in DiaSaveGame (AD-001 sign-off). Task 1 starts by reading it and decides; the user chose this approach over a second `SaveManager`.
3. **Who builds the adapter and owns `SnapshotManager`'s store reference.** `SnapshotManager` takes `ISnapshotStore&` at construction, so a CoW module must create the adapter before DiaCellHistory's host module builds the manager, and outlive it. The module that does this (a new small module, or an existing CoW debug-host module) is not decided.
4. **Snapshot enumeration.** `cells.snapshot.list` cannot discover a state file with no cell file because the interface has no enumerate. Not fixed here; an interface change would be a Dia amendment.

## As Built (2026-11-05)

- **Q2 resolved, CoW-only.** `SaveManager` and `SlotManager` read the config through a pointer on every call, so `SaveLoadModule` points its own config at `<base>/snapshots/` (pattern `{id}.sav`, `maxSlots` 0) for the one call and restores it. No DiaSaveGame change.
- **Q3 resolved.** `SaveLoadModule` owns the `SnapshotStoreAdapter` and runs it at the end of its `DoUpdate`; consumers get it from `GetSnapshotStore()` (via `ModuleRef<SaveLoadModule>`). No new module. Task 3's hand-off to `SnapshotManager` is **not done**: CoW has no DiaCellHistory host module yet, so whoever adds one passes `GetSnapshotStore()`.
- **Q1:** N = 120 (`kDeferLimitFrames`), unchanged.
- A second request while one is queued, or an invalid name, gets `ok = false` at the next boundary ("busy" / "invalid snapshot name"); counted as a snapshot failure.
- Health reason `last_snapshot_failed` (Degraded) clears on the next successful snapshot.
- Known gap: a request still queued when the module stops never gets its callback.
- Tests: `GoogleTests/CoW/TestSnapshotStoreAdapter.cpp` (9). Live `dia run cow` smoke still owed (no headless launch).

## Dependencies

**Requires:** E1 `SaveLoadModule` (Done), DiaSaveGame (Done), DiaCellHistory Feature 3 (`ISnapshotStore`, `SnapshotManager`; Done), DiaSimTime clock restore (Done).

**Required by:** T-1 Feature 7 (snapshot controls), S-3.5 Regression R3 (snapshot restore) and R-state hashing (`CaptureState()`).

## Status

**Status:** `Done` (live smoke signed off by the user 2026-11-05; the `SnapshotManager` hand-off is carried by the host module, see the backlog)
**Plan:** [snapshot-store-adapter.plan.md](snapshot-store-adapter.plan.md)
