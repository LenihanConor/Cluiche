# NotifySubscribers Call-Site Audit

**Date:** 2026-05-18
**Purpose:** Gate task F4-1 — determine what each `NotifySubscribers` call site needs before `SubscriptionManager` is deleted.

## Call sites

| # | File | Line | Topic StringCRC | Action for F4 |
|---|------|------|-----------------|---------------|
| 1 | `Dia/DiaDebugServer/DebugServer.cpp` | 498 | N/A — **definition** | Delete after migrating all callers to tap-based path |
| 2 | `Dia/DiaVisualDebugger/DebugLayerManager.cpp` | 253 | `"debug.layer.state"` | **Needs new stream or alias** — see below |

## BroadcastStageTransition
- Definition: `DebugServer.cpp:473`
- Only caller: `DebugServer.cpp:115` (internal, called from `DebugServer::Update` when stage changes)
- Action: Replace with tap on `$lifecycle` filtered to `kStageTransitionCommitted` events (F4-AC8/AC9)

## Call site analysis

### Call site 2: `DebugLayerManager.cpp:253` — `"debug.layer.state"`

`DebugLayerManager` holds a pointer to `DebugServer*` and calls `debugServer->NotifySubscribers(StringCRC("debug.layer.state"), payload)` when layer state changes. This is **not** a stream in the `.diaapp` manifest; it is a push notification from `DiaVisualDebugger` to connected WebSocket clients.

**Options:**

| Option | Cost | Risk |
|--------|------|------|
| A. Add a new `"debug.layer.state"` EventStream to DiaVisualDebugger's manifest and emit via `EventStreamWriter<LayerStateEvent>` | Medium — new type, new stream, manifest change | Clean; follows the stream model. Needs a new `LayerStateEvent` type. |
| B. Keep `DebugServer::NotifySubscribers` as a thin public shim (wraps sending JSON directly to subscribed clients, no `SubscriptionManager`) | Low — shim stays; no manifest change needed | `SubscriptionManager` can still be deleted. `NotifySubscribers` becomes a direct `mClientTaps`-bypassing send to all connected clients. |
| C. Register `"debug.layer.state"` as a topic alias and have `DebugServer::HandleSubscribe` open a "wildcard" tap | High — non-trivial protocol change | Out of scope. |

**Recommendation: Option B for F4 scope.** `DebugLayerManager` is not a stream-producing module; it's a debug subsystem inside `DiaVisualDebugger`. Forcing it to emit on a manifest stream would require adding a stream dependency between `DiaVisualDebugger` and `DiaApplicationFlow`, which violates the current module boundary (DiaVisualDebugger does not depend on DiaApplicationFlow streams today). Keep a minimal `NotifySubscribers` shim on `DebugServer` that sends JSON directly to all connected clients — no `SubscriptionManager` backing, just iterate open connections. This unblocks SubscriptionManager deletion while keeping the DiaVisualDebugger integration working. Add a `"debug.layer.state"` stream as a follow-up when DiaVisualDebugger is refactored.

## Summary for F4 plan tasks

- `SubscriptionManager.h/.cpp` → **delete** (F4-11)
- `BroadcastStageTransition` → **replace** with tap on `$lifecycle` (F4-13)
- `NotifySubscribers` definition → **keep as minimal shim**: remove `SubscriptionManager` backing, replace with a direct broadcast to all open WebSocket connections. `DebugLayerManager` continues to call it unchanged (F4-14)
- `mSubscriptionManager` member → **delete** (F4-12)
- `mStats.subscriptionCount` → either remove field or wire to `mClientTaps.Size()` (F4-12)
