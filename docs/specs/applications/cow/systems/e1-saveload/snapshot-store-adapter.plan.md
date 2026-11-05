**Spec:** @docs/specs/applications/cow/systems/e1-saveload/snapshot-store-adapter.md
**Status:** Done

Decisions (2026-11-05): directory override is CoW-only (config swap, no DiaSaveGame change); `SaveLoadModule` owns the adapter and runs it at the end of its `DoUpdate`; defer limit N = 120.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Snapshot save and load in `SaveLoadModule` with the directory override (AC 1, 2, 3, 6) | RED-GREEN: round trip, missing/corrupt/no-manifest restore leaves game untouched, not in `cow.save.list`, slot cap unaffected | Done | sonnet | `ScopedSnapshotConfig` points the config at `snapshots/` (`{id}.sav`, `maxSlots` 0) for one call. |
| 2 | `SnapshotStoreAdapter`: name validation, queueing, deferral with limit N, callbacks, `Exists`/`Delete`/`Folder` (AC 4, 5, 7, 8, 9) | Deferral behind a pending user save, N expiry, bad names touch no disk, busy refusal, callback once | Done | sonnet | Also fails queued callbacks once on module stop. |
| 3 | Wiring: construct the adapter, hand it to DiaCellHistory's `SnapshotManager`, `cow_main.diaapp` and `CoW.vcxproj` entries | Build; `SnapshotManager` over the real store | Done | sonnet | Adapter constructed (owned by `SaveLoadModule`), vcxproj entries added (CoW, GoogleTests). The `SnapshotManager` hand-off is carried by whichever module first hosts DiaCellHistory in CoW (S-3.5 `HistoryDriverModule` / T-1 Feature 7): pass it `GetSnapshotStore()`. |
| 4 | `CaptureState()` (AC 10, 11) | Order, no disk write, no counter or pending-flag change | Done | sonnet | |
| 5 | Metrics, health and logs (AC 12) | Failure degrades health, next success clears it | Done | sonnet | `cow.save.snapshot_takes/restores/failures`, `last_snapshot_failed`. |
| 6 | Exhaustive pass: gap audit, edge tests, sanitizer | Empty participants, take over existing name, restore behind pending user load, stop-time callback, clock restore in the same frame | Done | sonnet | 14 new tests. `dia check sanitizer` clean (14049); full `googletest` 2 shards passed (6960 each). |
