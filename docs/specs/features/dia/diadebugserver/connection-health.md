# Feature Spec: Connection Health

## Parent System
@docs/specs/systems/dia/diadebugserver.md

## Problem Statement

During the entity-inspector debugging session (2026-06-06), three bugs compounded silently — no single system reported failure. 4000+ messages were dropped without any visible indicator, the ObservationBridge flooded all clients unconditionally (no subscription gating), and subscribe messages went unacknowledged so the editor had no way to know subscriptions failed. These items would have surfaced the problem in minutes instead of hours.

This feature adds per-topic health metrics, subscription-gated observation data, throttled batching, subscribe acknowledgement, and a tiered warning system (yellow/red) surfaced in the editor's connection UI.

## Goals

1. Make message drops immediately visible to the developer via a tiered badge (yellow/red) and health popover
2. Gate ObservationBridge behind the subscribe protocol so it only sends to interested clients
3. Throttle observation data to prevent flooding (batched snapshots at configurable intervals)
4. Add explicit subscribe ACK so the editor can detect failed subscriptions within a timeout
5. Expose per-topic send/drop stats via `get_server_stats` for the editor to poll and display

## Acceptance Criteria

- [x] **AC-1**: Per-topic counters — DebugServer tracks `messages_sent` and `messages_dropped` per topic per connection
- [x] **AC-2**: `get_server_stats` response includes a `topics` array with `{topic, sent, dropped, rate}` per active topic
- [x] **AC-3**: ObservationBridge only sends to clients that have subscribed to `observation.log`, `observation.trace`, `observation.metric`, or `observation.health` respectively
- [x] **AC-4**: ObservationBridge batches: logs ≤10 per batch at max 200ms intervals, metrics ≤1 snapshot per 500ms, traces ≤5 per batch at max 200ms, health transitions always immediate
- [x] **AC-5**: Protocol extended with `MESSAGE_TYPE_SUBSCRIBE_ACK` — server sends ACK with `{data_type, success, message}` after processing a subscribe
- [x] **AC-6**: Editor validates subscribe ACK arrives within 3 seconds; on timeout, logs ERROR and shows toast
- [x] **AC-7**: Queue overflow fires `editor.notification` toast at ERROR level: "Connection degraded — messages being dropped"
- [x] **AC-8**: Queue overflow promoted from WARNING to ERROR log level in C++
- [x] **AC-9**: Connection button gains a badge dot: yellow (≥10 drops/5s) or red (≥100 drops/5s)
- [x] **AC-10**: Connection dropdown shows summary line (e.g. "3 topics healthy" or "Dropping messages") and a "Connection Health" button
- [x] **AC-11**: Health popover shows per-topic table (topic, received, dropped, drop rate %) and subscribe handshake status per subscription

## Design

### 1. Per-Topic Stats (C++ — DebugServer)

Extend `ServerStats` with a per-topic breakdown:

```cpp
struct TopicStats
{
    Dia::Core::StringCRC topic;
    int messagesSent;
    int messagesDropped;
    float dropRatePercent; // (dropped / (sent+dropped)) * 100
};

// Add to ServerStats:
static constexpr int kMaxTrackedTopics = 32;
TopicStats topicStats[kMaxTrackedTopics];
int topicStatCount;
```

Increment `messagesSent` on every successful `SendText`/`BroadcastText` per topic. Increment `messagesDropped` when the queue is full or send fails. Recalculate `dropRatePercent` on each `get_server_stats` query.

### 2. Extended `get_server_stats` Response

```json
{
  "game_side": { ... },
  "editor_side": { ... },
  "server": { ... },
  "topics": [
    { "topic": "entity.inspect", "sent": 142, "dropped": 0, "drop_rate": 0.0 },
    { "topic": "observation.metric", "sent": 4012, "dropped": 87, "drop_rate": 2.12 }
  ]
}
```

### 3. ObservationBridge Subscription Gating

Replace `mServer->BroadcastText(buf)` with targeted sends. ObservationBridge gains a reference to the subscription state (client taps list). For each observation topic:

- Check which connections have subscribed to that topic
- Send only to those connections
- Skip entirely if no subscribers exist

Topic mapping:
- `OnLogEntry()` → topic `"observation.log"`
- `OnSpan()` → topic `"observation.trace"`
- `OnSnapshot()`/`OnFinal()` → topic `"observation.metric"`
- `OnTransition()` → topic `"observation.health"`

### 4. ObservationBridge Throttle/Batching

Add batching buffers within ObservationBridge:

| Channel | Max batch size | Max interval | Behaviour |
|---------|---------------|--------------|-----------|
| Logs | 10 entries | 200ms | Accumulate, flush on size OR timer |
| Traces | 5 spans | 200ms | Accumulate, flush on size OR timer |
| Metrics | 1 snapshot | 500ms | Latest-wins (drop intermediate snapshots) |
| Health | 1 (immediate) | 0ms | Always send immediately (rare, important) |

Flush timer checked during `DebugServer::Tick()`. On flush, send as JSON array wrapper for logs/traces.

### 5. Subscribe ACK Protocol

Add to `debug_protocol.proto`:
```proto
MESSAGE_TYPE_SUBSCRIBE_ACK = 17;

message SubscribeAck {
  string data_type = 1;
  bool success = 2;
  string message = 3; // e.g. "subscribed" or "unknown_topic_queued"
}
```

Server sends `MESSAGE_TYPE_SUBSCRIBE_ACK` immediately after processing a subscribe request (after registering the tap or virtual topic). Success is always `true` (even for virtual topics — "queued" is valid). Editor-side validates arrival within 3s timeout.

### 6. Queue Overflow → ERROR + Toast

In `DebugServer::SendToClient()` / `BroadcastText()` fallback path:
- Promote `DIA_LOG_WARNING` → `DIA_LOG_ERROR` for queue-full events
- Increment `mStats.messagesDropped` and per-topic dropped counter
- Push `editor.notification` event to the connection (if it can still accept):
  ```json
  { "topic": "editor.notification", "level": "error", "title": "Connection degraded", "message": "Messages being dropped" }
  ```
- Rate-limit notification to max 1 per 5 seconds (don't spam toasts)

### 7. Connection Button Badge (Editor UI)

`ConnectionButton.tsx` polls `get_server_stats` every 2 seconds while connected. Tracks drops in a 5-second sliding window:

| Drops in 5s window | Badge | Colour |
|---|---|---|
| 0 | None | — |
| 10–99 | Yellow dot overlay | `#dcdcaa` |
| ≥100 | Red dot overlay | `#f48771` |

Badge renders as a small overlay circle on the connection dot.

### 8. Connection Dropdown Summary + Health Button

When connected, the dropdown shows (between status header and game info):

```
┌─────────────────────────────┐
│ ● Connected                 │
├─────────────────────────────┤
│ ⚠ Dropping messages    [!]  │  ← summary line (yellow/red text), or "3 topics healthy" (green)
├─────────────────────────────┤
│ Game: CluicheTest           │
│ ...                         │
│ [Connection Health]         │  ← button that opens popover
│ [Disconnect]                │
└─────────────────────────────┘
```

### 9. Health Popover

Opens from "Connection Health" button. Click-away or Escape to dismiss.

```
┌─────────────────────────────────────────────────────────┐
│ Connection Health                                    ✕  │
├─────────────────────────────────────────────────────────┤
│ Topic              Received   Dropped   Rate            │
│ entity.inspect          142         0    0.0%           │
│ observation.metric     4012        87    2.1%    ●red   │
│ observation.log         320         3    0.9%    ●yel   │
├─────────────────────────────────────────────────────────┤
│ Subscriptions                                           │
│ entity.inspect       ✓ ACK (12ms)                       │
│ observation.metric   ✓ ACK (8ms)                        │
│ observation.log      ⚠ No ACK (timeout 3s)              │
└─────────────────────────────────────────────────────────┘
```

Per-topic rows colour-coded: green (0%), yellow (>0% <5%), red (≥5%).

## Implementation Files

### C++ (Game-side)
- `Dia/DiaDebugServer/DebugServer.h` — extend `ServerStats` with `TopicStats[]`
- `Dia/DiaDebugServer/DebugServer.cpp` — per-topic counting in send paths, extended `get_server_stats`
- `Dia/DiaDebugServer/ObservationBridge.h` — add batch buffers, subscriber list reference
- `Dia/DiaDebugServer/ObservationBridge.cpp` — subscription-gated sends, batching logic, flush timer
- `Dia/DiaDebugProtocol/proto/debug_protocol.proto` — add `MESSAGE_TYPE_SUBSCRIBE_ACK` + `SubscribeAck`
- `Cluiche/CluicheGameBaseline/Modules/DebugServerHostModule.cpp` — re-enable ObservationBridge startup

### Editor UI (TypeScript/React)
- `Cluiche/CluicheEditor/UI/src/toolbar/ConnectionButton.tsx` — badge dot, summary line, health button
- `Cluiche/CluicheEditor/UI/src/toolbar/ConnectionHealthPopover.tsx` — new file, health popover component
- `Cluiche/CluicheEditor/UI/src/bridge/EditorBridge.ts` — subscribe ACK timeout tracking

### Editor C++ (DiaEditor)
- `Dia/DiaEditor/LiveConnection/GameConnectionController.cpp` — subscribe ACK validation, stats polling

## Binding Decisions

- **DDS-002** (broadcast core metrics always, specialized on subscription) — ObservationBridge data becomes subscription-gated, aligning with "specialized data on subscription" intent
- **DDS-005** (JSON over WebSocket) — all new messages use JSON
- **DDS-010** (JSON serialization) — per-topic stats and ACK use JSON

No binding constraints violated.

## Open Design Questions

1. **Batch flush on disconnect** — should pending batches flush immediately when a subscribed client disconnects, or just discard? Leaning discard (client is gone anyway).
2. **Per-topic stats reset** — should `get_server_stats` report lifetime totals or resettable windows? Starting with lifetime totals; can add a reset command later if needed.

## Status

`Done`
