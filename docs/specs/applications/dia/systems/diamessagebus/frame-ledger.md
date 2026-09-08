# Feature Spec: Frame Ledger

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

`GetLastTickLedger()` — owned by core-bus — exposes only the single most recent completed tick; there is no window into the ticks that preceded it. Diagnosing intermittent message-flow problems such as delivery spikes, drop bursts, or gaps requires correlating many consecutive ticks over a meaningful wall-clock window. This feature adds a fixed-size, allocation-free ring buffer that retains the last `kLedgerCapacity` `LedgerSnapshot` objects and provides a read API so the visual debugger's History tab can display flow over time. The ring and its entire API compile out of Release builds entirely, leaving no production overhead.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | After N pushes where N ≤ kLedgerCapacity, the ring retains all N snapshots | Unit test: push N ≤ kLedgerCapacity snapshots; assert `Count() == N` |
| AC-2 | After N pushes where N > kLedgerCapacity, the ring holds exactly kLedgerCapacity snapshots and the oldest entry is evicted | Unit test: push kLedgerCapacity + 10 snapshots; assert `Count() == kLedgerCapacity`; assert the lowest-tickIndex snapshot is no longer present |
| AC-3 | The history accessor returns snapshots in chronological order (oldest-first) | Unit test: push 5 snapshots with ascending `tickIndex`; assert each retrieved snapshot's `tickIndex` is greater than or equal to the previous |
| AC-4 | Ticks where `droppedCount > 0` are queryable and distinct from clean ticks | Unit test: push a mix of snapshots, some with `droppedCount > 0`, some zero; assert the dropped-tick query returns only the former and none of the latter |
| AC-5 | All history API symbols are absent from a Release\|x64 build | Compile in Release\|x64; inspect link map or run `dumpbin /symbols` — no `LedgerHistory` symbols present |
| AC-6 | Zero heap allocation occurs after `LedgerHistory` construction | Unit test with a tracking allocator or `_CrtMemCheckpoint` pair: construct the ring, push kLedgerCapacity snapshots, assert allocation delta is zero |
| AC-7 | `Bus` (or `MessageBusModule`) automatically pushes the completed-tick snapshot into the ring immediately after core-bus performs the ledger double-buffer swap | Integration test: run 5 flush ticks; assert ring `Count() == 5` and each entry's `tickIndex` matches the expected sequence |

## Design

### Overview

`GetLastTickLedger()` is owned and implemented by core-bus; the double-buffer swap timing is defined there. This feature builds on that seam: after each flush, when core-bus promotes the current-tick snapshot to the "last completed" slot, the push wiring calls `LedgerHistory::Push(bus.GetLastTickLedger())`. The push wiring lives in `MessageBusModule::Update()` and is guarded by `#ifdef DIA_DEBUG`.

`LedgerSnapshot`, `LedgerMessageEntry`, and `kLedgerCapacity` are defined in the system spec's public interface and are referenced here without redefinition.

### Ring buffer

The ring holds up to `kLedgerCapacity` (3600) `LedgerSnapshot` objects. When full, the oldest entry (FIFO) is overwritten. The backing store is a fixed compile-time array — no heap after construction.

Because `LedgerSnapshot` already embeds a `DynamicArrayC<LedgerMessageEntry, 64>`, the per-slot footprint is approximately 1.3 KB. At 3600 slots the ring occupies roughly 4.7 MB of static debug-only storage. Implementors should confirm `sizeof(LedgerSnapshot)` before committing and document it in the module YAML.

The ring exposes at minimum:

```cpp
#ifdef DIA_DEBUG
namespace Dia::MessageBus {

    class LedgerHistory {
    public:
        // Push a snapshot; evicts oldest if at capacity. Called from flush path only.
        void Push(const LedgerSnapshot& snapshot);

        // Number of snapshots currently held (0..kLedgerCapacity).
        uint32_t Count() const;

        // Read API — shape resolved by ODQ-1 before implementation.
        // Option A (span-style): caller provides scratch buffer; ring linearises into it.
        // Option B (visitor):    ForEachSnapshot(Fn&&) iterates oldest-first, no copy.
        // [See ODQ-1]

        // Filtered: iterate or count snapshots where droppedCount > 0.
        // Exact shape follows ODQ-1 resolution.
    };

}  // namespace Dia::MessageBus
#endif  // DIA_DEBUG
```

No `StringCRC` IDs are used in the ring's own API — the IDs are inside the `LedgerSnapshot` entries themselves, already populated by core-bus.

### Open Design Questions

| # | Question | Impact |
|---|----------|--------|
| ODQ-1 | **Read shape**: should the accessor expose a contiguous `Span<const LedgerSnapshot>` (zero-copy if ring has not wrapped; requires linearisation into a scratch buffer at the wrap boundary) or a visitor / callback `ForEachSnapshot(Fn&&)` (handles wrap transparently, no extra buffer, but slightly harder to bind in the visual debugger)? | Drives the public API of `LedgerHistory` and the usage pattern in the visual-debugger History tab. Resolve before Task 2. |
| ODQ-2 | **Ownership**: should `LedgerHistory` be a separate type owned by `MessageBusModule` (independently testable, keeps `Bus` focused on routing) or an embedded ring inside `Bus` (one fewer object, simpler lookup path for the visual debugger)? | Determines header layout and the path by which `MessageBusDebugDomain` accesses history. Resolve before Task 1. |

### Push seam

```cpp
// Inside MessageBusModule::Update(), after the flush passes complete (guarded):
#ifdef DIA_DEBUG
    mLedgerHistory.Push(mBus.GetLastTickLedger());
#endif
```

The push must happen after core-bus has completed the swap so that `GetLastTickLedger()` returns the tick that just finished, not the one in progress.

### Release-build strip

All symbols introduced by this feature — the `LedgerHistory` type, its methods, and any `Bus` / `MessageBusModule` members that reference it — are wrapped in `#ifdef DIA_DEBUG`. The last-tick ledger in core-bus is **not** stripped; it stays in all configurations for test assertions. No `#ifdef` ladder should appear in consumer call sites outside this module — the visual debugger already guards its own body.

### Deferred — ServiceStream cross-PU export

The `ServiceStream<LedgerSnapshot>` outlet is the designed seam for a future Inspector-tier CluicheEditor panel (connected to a running game via `DiaDebugServer`). The outlet is named here so the boundary is explicit, but it carries **no ACs in this feature** and no code should be written for it during this feature's implementation.

```cpp
// Deferred — Inspector tier. No ACs. Do not implement in this feature.
// A future ledger-servicestream-export feature will wire:
//   ServiceStream<LedgerSnapshot>* mLedgerExportStream = nullptr;
// on MessageBusModule, populated from mLedgerHistory on each push.
```

The outlet is separate from the Release strip question: it does not exist yet, so there is nothing to guard.

### Prerequisite

**core-bus** must be complete before Task 4 can proceed. Specifically, `GetLastTickLedger()` must be live and the double-buffer swap timing must be stable so the push wiring is placed at the correct point in the flush sequence.

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Define `LedgerHistory` type (or ring member inside `Bus` per ODQ-2): fixed-size backing array, `Push()`, FIFO eviction, `Count()`; verify zero heap after construction | Resolve ODQ-2 before starting; covers AC-1, AC-2, AC-6 |
| 2 | Implement read API (span or visitor per ODQ-1) with oldest-first ordering | Resolve ODQ-1 before starting; covers AC-3 |
| 3 | Implement dropped-tick query: filter or iterate snapshots where `droppedCount > 0` | Covers AC-4 |
| 4 | Wire `LedgerHistory::Push()` into `MessageBusModule::Update()` immediately after core-bus ledger swap; guard with `#ifdef DIA_DEBUG` | Requires core-bus complete; covers AC-7 |
| 5 | Apply `#ifdef DIA_DEBUG` guards to all history symbols; confirm Release\|x64 compile strip via link-map inspection | Covers AC-5 |

## Status

`Approved`
