# Feature Spec: DiaCellHistory Snapshots

**Parent:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Builds on:** @docs/specs/applications/dia/systems/diacellhistory/recorder-and-tiers.md (Feature 1), @docs/specs/applications/dia/systems/diacellhistory/export.md (Feature 5)

**Gameplay Domains:** debug, tools

## Summary

Named snapshots of the world that survive the rings rolling over and a restart. `cells.snapshot.take <name>` asks the game's `ISnapshotStore` to save the game state and, in the same frame, keeps a quantised copy of every recorded layer. `cells.snapshot.restore <name>` loads the game state back through the store and truncates the history to the snapshot's moment (or, for a snapshot from an earlier session, starts a new run). `list` and `delete` manage them.

DiaCellHistory owns the interface, the orchestration and the per-cell copies. The game owns the state: CoW implements `ISnapshotStore` over E1 SaveLoad.

## Problem

To answer "what changed because of that drought?" you need to keep a known state, run on, and later compare against it or return to it. The per-cell rings roll over and are lost on exit, and a savegame alone has no history. A snapshot ties together the game's saved state, the per-cell data needed for compares, and the point in the recorded timeline, with a clean rule for what restore does to history.

## Goals

1. **Take** a named snapshot at a frame boundary, capturing game state and per-cell data of the same instant.
2. **Restore** it: game state back, history truncated consistently.
3. **Survive restarts.** Snapshots are files; restoring one from an earlier session works.
4. **Compare later.** The per-cell copies cover every recorded layer, so a compare works after the rings roll past.
5. **Bounded.** At most 16 snapshots, separate from the game's save slots.
6. **Generic.** The game supplies the state store; nothing here knows CoW.

## Non-Goals

- Saving or loading game state itself (the game's `ISnapshotStore`; CoW: E1).
- Compare views and delta layers (Feature 4).
- Re-simulating to a snapshot; restore goes to the stored state only.
- Automatic or timed snapshots.
- Syncing snapshots between machines.

## Architecture

Lives in `Dia/DiaCellHistory/` (namespace `Dia::CellHistory`). The CoW adapter lives in CoW.

```
ISnapshotStore            game-implemented: save/load game state by name (frame-boundary, asynchronous)
SnapshotManager           orchestration, snapshot list, cap, naming, truncation rules
SnapshotCellFile          reads/writes the per-cell copies file (dia.cell_snapshot)
```

### `ISnapshotStore` (game side)

```cpp
using SnapshotCallback = void(*)(bool ok, const char* message, void* user);

class ISnapshotStore {
public:
    virtual ~ISnapshotStore() = default;
    virtual void RequestTake   (const char* name, SnapshotCallback cb, void* user) = 0;   // runs at the next frame boundary
    virtual void RequestRestore(const char* name, SnapshotCallback cb, void* user) = 0;   // runs at the next frame boundary
    virtual bool Exists(const char* name) const = 0;
    virtual bool Delete(const char* name) = 0;
    virtual const char* Folder() const = 0;     // where this store keeps files; the cell files go next to them
};
```

The callbacks fire on the same thread at the same frame boundary the work ran in. Take and restore are asynchronous because the game executes them at the simulation frame boundary (E1 does).

### Names and files

- A snapshot name is 1 to 32 characters from `A-Z a-z 0-9 _ -`; anything else is rejected (no paths, no traversal).
- The game store writes its state file; DiaCellHistory writes `<Folder>/snapshot_<name>.cells.json` (format `dia.cell_snapshot` v1):

```json
{ "format": "dia.cell_snapshot", "version": 1,
  "header": { "name": "pre-drought", "runId": "5a1f0c9e2b7d4410", "sample": 612, "label": "Y12 W9",
              "createdUnix": 1792510000, "world": "coast_range" },
  "grid": { "id": "world.hex", "width": 104, "height": 64 },
  "layers": { "weather.precipitation": { "kind": "float", "min": 0, "max": 1, "data": "<b64 u16>" },
              "terrain.biome": { "kind": "category", "values": ["Grassland", "Forest"], "data": "<b64 u8>" } } }
```

- Encodings are Feature 5's: base64 little-endian uint16 for float layers (quantised over the schema `min`–`max`, error at most `range / 65,535`) and uint8 ids for category layers.
- `runId` is a random 64-bit id the recorder creates at start (and on a reset); `sample` is the index of the most recent sample when the snapshot was taken.

### Take

`cells.snapshot.take <name> [overwrite]`:
1. Validate the name; refuse an existing name unless `overwrite`; refuse when 16 snapshots already exist (counting snapshot files from earlier sessions); refuse while another take or restore is in flight; error and change nothing otherwise.
2. At the frame boundary: capture a quantised copy of **every recorded layer** from the live values (the layers Feature 1 aggregates, up to 64) into memory.
3. Call `store.RequestTake(name, ...)`. On success, write the cell file atomically (temporary file, then rename) next to the store's file, and record the snapshot in the list with status `ready`.
4. If the store fails, or the cell file cannot be written, remove any partial file (the store's if it wrote one, via `Delete`) and mark the request `failed` with the reason. Nothing half-made is left.
5. The command returns `{ ok: true, pending: true }` immediately; the outcome is logged and shown in `cells.snapshot.list` (`status`).

A take records the snapshot at the latest sample index, but the copies are the state at take time, so a compare against a snapshot uses exactly what was saved even if the state moved on since the last sample.

### Restore

`cells.snapshot.restore <name>`:
1. Require both the store's state and the cell file to exist, no other snapshot operation in flight, and the cell file's grid (id, width, height) to equal the active grid; otherwise error and change nothing.
2. Read the cell file header. Call `store.RequestRestore(name, ...)`.
3. On success, in the same frame:
   - **Same run** (`runId` equals the recorder's, and `sample` is within the recorded samples): truncate the history after `sample`. That drops later samples, their aggregate rows and per-cell ring slots (the ring's newest end moves back; if the snapshot is older than the window start the ring becomes empty), the events attached to later samples, and any events emitted but not yet attached. Log a Warning stating how many samples were dropped.
   - **Foreign run** (a different `runId`, or a sample the history does not contain): clear all history, create a new `runId`, and restart sample numbering at 0, so the first sample after restore follows the restored state. Log a Warning that history was reset.
4. The viewer returns to live (Feature 2). Selection is kept.
5. If the store fails, nothing changes in history.

Game systems are restored by the store, in the order the game defines (CoW: E1, including the DiaSimTime clock restore in the same frame).

### Other commands

| Command | Effect |
|---|---|
| `cells.snapshot.list` | Returns `[ { name, sample, label, createdUnix, world, bytes, sameRun, status } ]`; `status` is `pending`, `ready` or `failed` |
| `cells.snapshot.delete <name>` | Deletes the store's state and the cell file; an unknown name is an error |

### Query for editors

`cells.history.snapshot {name, layer}` returns that snapshot's values for one recorded layer in the Feature 5 encoding (`{ name, layer, kind, min, max, width, height, data }`), for the editors and for Feature 4. The snapshots list in `cells.history.meta` and in the exported file is filled from this feature: `{ name, sample, label }` per snapshot.

### Cap and housekeeping

- At most 16 snapshots exist. The count is read from the store's folder, so snapshots left by earlier sessions count. A 17th `take` is refused ("delete a snapshot first"); nothing is evicted automatically, so a script cannot delete an old baseline silently.
- Snapshot files and the game's state files are matched by name; an orphan on either side (a cell file with no state, or the reverse) is listed with `status: failed` and a reason, and `delete` removes both halves.

### Logging and observation

- **Channel:** `HIS`.
- **Info:** take requested and completed (name, sample, bytes); restore completed (samples dropped or history reset); delete.
- **Warning:** history truncated, history reset by a foreign snapshot, an orphan file found.
- **Error:** store failure, cell file write failure (with the reason).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on the capture and quantise pass and on the cell file write.
- **Metrics:** snapshots (count), take ms, restore ms, snapshot bytes.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. `cells.snapshot.take <name>` returns pending and, at the frame boundary, produces the store's state and `snapshot_<name>.cells.json`, both or neither.
2. The cell file holds a quantised copy of every recorded layer (up to 64) taken at take time, with the sample index, run id, label, world and grid, within `range / 65,535` of the live values.
3. A name outside `A-Z a-z 0-9 _ -` or over 32 characters is rejected; an existing name is refused unless `overwrite`; a 17th snapshot (counting earlier sessions' files) is refused with a clear message; none of these change anything.
4. If the store or the file write fails, no partial files remain and the list shows the request as `failed` with the reason.
5. `cells.snapshot.restore` of a same-run snapshot restores the game state through the store and truncates samples, aggregate rows, ring slots and events after `sample`, with a Warning stating how many samples were dropped.
6. Restoring a snapshot older than the per-cell window start leaves the ring empty and records a gap; aggregates and events up to `sample` remain.
7. Restoring a snapshot from another run (or a sample not in the history) clears the history, creates a new run id, restarts sample numbering at 0 and logs a Warning.
8. A failed store restore changes nothing in the history.
8a. Restoring a snapshot whose grid (id, width, height) differs from the active grid is refused with a message naming the snapshot's size, before the store is called; a second take or restore while one is in flight is refused.
9. After a restore the viewer is live, the selection is kept, and the next `Sample` continues from the truncated history (or from index 0 after a reset).
10. `cells.snapshot.list` returns name, sample, label, created time, world, bytes, `sameRun` and `status` for every snapshot, including orphans marked `failed`; `delete` removes both halves.
11. `cells.history.snapshot {name, layer}` returns the snapshot's values for a recorded layer in the Feature 5 encoding and an error for an unknown name or layer.
12. The `snapshots` field of `cells.history.meta` and of the exported file lists `{name, sample, label}`.
13. A take at Large with 64 recorded layers completes its in-memory capture within a frame budget, and the cell file stays under about 1 MB.
14. All snapshot and restore operations use the store's callbacks; DiaCellHistory never reads or writes the game's state files.

## Test Strategy

- **GoogleTests (TDD) with a fake `ISnapshotStore`:**
  - Take success and each failure path (store fails, file write fails), with partial-file clean-up.
  - Naming rules, overwrite, the 16 cap including pre-existing files.
  - Cell file content and encoding round trip within the quantisation error; header fields.
  - Restore same run: truncation of every tier, window-start edge case, event removal, Warning counts.
  - Restore foreign run: history reset and new run id; numbering from 0.
  - Store failure on restore; viewer returns to live; selection kept.
  - List with orphans and statuses; delete both halves.
  - The snapshot query and the meta field.
- **Test utilities** in `DiaCellHistory/Testing/`: a fake store with scripted success or failure and a recorded callback, a temp-folder helper, a recorder pre-filled with fixture samples.
- **Perf:** capture of 64 layers at Large.
- **CoW proof (S-3, from the system spec):** take "pre-drought", force a drought, restore, and the A1 state hash matches the hash at the time of the take.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-002 | Going back is a view-only scrub; restore is loading a named snapshot through the game's store, with no re-simulation | Restore goes through `ISnapshotStore` only |
| CH-003 | Compare covers snapshot vs snapshot, now vs past, run vs run | Snapshots store the per-cell data Feature 4 compares |
| CH-005 | Generic: the snapshot store comes from the game | `ISnapshotStore` is implemented by the game |
| Resolved decision 1 (system spec) | Snapshots are files | State file by the store, cell file next to it |
| PD-009 | Generated output under `Cluiche/out/` | Files live in the store's folder (CoW: under `out/saves/cow/`) |
| PD-004 | No STL in public APIs | DiaCore types and function pointers |
| Game logic on SimPU | Manager on SimPU, store at the frame boundary | Same-PU contract |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Feature 1 | Recorder, stores, run id | Approved |
| Feature 5 | Encodings, meta, the `snapshots` field | Approved |
| Feature 2 | Viewer returns to live after restore | Approved |
| **CoW `ISnapshotStore` adapter over E1** | Game state take and restore | **CoW consumer task, not built.** Uses the snapshot namespace added to E1's spec (a `snapshots/` folder, separate cap of 16) and defers to E1's single pending save or load |
| E1 SaveLoad | `RestoreState` in the same frame as the DiaSimTime clock restore | Done (existing rule) |

## Resolved Decisions

1. **E1 needs a snapshot namespace** (2026-10-20). E1 caps saves at 8 slots named `slot_<id>.sav`. The CoW adapter keeps snapshots in `out/saves/cow/snapshots/<name>.sav` with its own cap of 16, outside the 8 slots. E1's spec carries the amendment (see E1 "Snapshot namespace"); the work is part of the adapter task.
2. **One snapshot operation in flight** (2026-10-20). E1 keeps a single pending save and a single pending load, so a second request would overwrite the first. Two rules cover it:
   - `SnapshotManager` allows only one take or restore at a time; a second command while one is pending returns an error ("snapshot operation in progress") and changes nothing.
   - The `ISnapshotStore` contract requires the store to serialise with its own other traffic. The CoW adapter checks E1's pending flags and defers its request to a later frame boundary when `cow.save` or `cow.load` is pending; it never overwrites a user's pending request, and its callback reports `ok=false` if it cannot run.
3. **A different grid is refused in v1** (2026-10-20). The recorder is bound to one grid. `cells.snapshot.restore` compares the cell file's grid (id, width, height) with the active grid *before* calling the store; a mismatch is refused with "snapshot is for a different grid (`<w>` x `<h>`); load that world first", and nothing changes. This also keeps the foreign-run rule for what it is meant for: the same grid, a different session. Restoring across grids can be specced later with E1's world-load story (E1 aborts a failed load and leaves the game unchanged, so a store failure is already safe).

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. The CoW `ISnapshotStore` adapter over E1 is a separate consumer task.
