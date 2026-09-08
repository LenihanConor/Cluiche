# Plan: Connection Health

**Spec:** @docs/specs/applications/dia/systems/diadebugserver/connection-health.md
**Status:** Done

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `MESSAGE_TYPE_SUBSCRIBE_ACK` to proto + regenerate | Build passes, new enum value visible in generated code | Done | sonnet | Added enum value 17, `SubscribeAck` message, `kSubscribeAck=26` oneof case to both generated/ copies. 32 tests pass. |
| 2 | Add `TopicStats` to `ServerStats` and per-topic counting | `get_server_stats` returns `topics` array with at least 1 entry after a subscribe | Done | sonnet | TopicStats struct, RecordTopicSent helper, topics[] in get_server_stats. 32 tests pass. |
| 3 | Extend `get_server_stats` query to include `topics[]` | Unit test: query response contains topics array with sent/dropped/rate | Done | sonnet | Folded into Task 2 — topics[] serialized in same PR. |
| 4 | Send `SUBSCRIBE_ACK` from server on subscribe | Unit test: subscribe triggers ACK message back to client with correct data_type | Done | sonnet | ACK sent at end of HandleSubscribe covering all valid paths (real, virtual, no state provider). |
| 5 | Subscription-gate ObservationBridge sends | Observation data only reaches subscribed client, not all | Done | opus | SubscriberQueryFn callback, SendToSubscribers helper, BroadcastText replaced. 32 tests pass. |
| 6 | Add batching to ObservationBridge (logs, traces, metrics) | Logs batch ≤10/200ms, metrics ≤1/500ms, traces ≤5/200ms, health immediate | Done | opus | Batch buffers + Flush(dt) called from DebugServer::Tick. 32 tests pass. |
| 7 | Re-enable ObservationBridge in DebugServerHostModule | Bridge starts, observation topics available for subscribe | Done | sonnet | Uncommented startup code, updated comment. 32 tests pass. |
| 8 | Promote queue overflow to ERROR + fire notification | ERROR log on drop, `editor.notification` sent (rate-limited 1/5s) | Done | sonnet | DiaWebSocket::Server::SendText returns bool; SendProtoMessage/NotifySubscribers check return, fire DIA_LOG_ERROR + SendDropNotification (5s rate-limit). 32 tests pass. |
| 9 | Editor: subscribe ACK timeout validation | Toast fires if no ACK within 3s of subscribe | Done | sonnet | PendingSubscribe tracking in GameConnectionController; kSubscribeAck case clears pending; 3s timeout fires ERROR log + toast. Build passes. |
| 10 | Editor UI: poll `get_server_stats` + compute drop window | Stats state refreshes every 2s, 5s sliding window of drops computed | Done | sonnet | useConnectionHealth.ts hook; pipeline 3/3 pass. |
| 11 | Editor UI: badge dot on ConnectionButton (yellow/red) | Badge appears at ≥10 drops/5s (yellow), ≥100 drops/5s (red) | Done | sonnet | Overlay circle on connection dot; combined with Task 12. Pipeline pass. |
| 12 | Editor UI: summary line + "Connection Health" button in dropdown | Dropdown shows health summary text and button when connected | Done | sonnet | Combined with Task 11 in single ConnectionButton.tsx edit. |
| 13 | Editor UI: ConnectionHealthPopover component | Popover with per-topic table + subscription status table | Done | sonnet | ConnectionHealthPopover.tsx — per-topic table with colour coding, click-away/Escape dismiss. Pipeline 3/3. |
| 14 | HTML mockup for Connection Health UI | Visual acceptance gate for tasks 11–13 | Done | haiku | docs/research/editor_ai_chat/connection_health_mockup.html |

## Dependency Graph

```
[1] Proto ACK ──────────┐
                        ├──► [4] Server sends ACK ──► [9] Editor validates ACK
[2] TopicStats ─────────┤
                        ├──► [3] get_server_stats extended ──► [10] Editor polls stats
[5] Subscription gate ──┤                                          │
                        ├──► [7] Re-enable bridge                  ▼
[6] Batching ───────────┘                              [11] Badge dot
                                                       [12] Summary + button
[8] Queue overflow ERROR ──────────────────────────────► [13] Health popover

[14] HTML mockup (parallel, no deps — do before UI tasks)
```

## Execution Order

**Phase 1 — Protocol & C++ infrastructure (tasks 1–4, 8):**
Parallel: 1 + 2. Then 3 (depends on 2), 4 (depends on 1). Task 8 is independent.

**Phase 2 — ObservationBridge rework (tasks 5–7):**
Sequential: 5 → 6 → 7. Task 5 is the hardest (architecture change). Task 7 is the reward (re-enable).

**Phase 3 — Editor integration (tasks 9–13):**
Task 14 (mockup) first for visual alignment. Then 9 (ACK validation), 10 (polling hook). Then 11, 12, 13 in order (each builds on previous UI state).
