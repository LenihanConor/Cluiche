# Feature Spec: mailbox-traffic-monitor

**System:** DiaEntityInspector
**App:** Dia
**Status:** Draft
**Mockup:** @docs/research/diaentit_visual_debug/mockup_b_split.html

## Summary

Populate the `mailbox_log` array in the `entity.inspect` payload by maintaining a per-entity ring buffer of dispatched messages in `Domain`, and implement `MailboxMonitorController` to drive the Mailbox tab. The tab shows a scrollable log of the last 64 messages dispatched to or from the selected entity, a message-type histogram, and a pause/snapshot control. No new WebSocket topic is needed — the log is carried in the existing `entity.inspect` payload.

## Traceability

| Level | Spec |
|---|---|
| Platform | [Cluiche.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentityinspector.md](../../systems/dia/diaentityinspector.md) |
| Depends on feature | [entity-inspector-panel.md](entity-inspector-panel.md) |
| Depends on feature | [editor-inspection.md](../diaentity/editor-inspection.md) |
| Depends on system | [diamailbox.md](../../systems/dia/diamailbox.md) |

## Goals

- Developers can see which message types are flowing to/from the selected entity and at what rate without adding any print statements
- The mailbox log captures messages that have already been dispatched (drained), not just queued — requires a post-drain observation hook in `Domain`
- Pause/snapshot lets the developer freeze the log display to examine a moment in time without stopping the simulation

## Acceptance Criteria

### Domain — per-entity message dispatch log

- `Domain` maintains an internal `EntityMessageLog` ring buffer: last 64 messages dispatched to any entity
- Each log entry contains: `frame` (uint32 frame counter), `recipient_entity_index` (uint32), `msg_type_crc` (uint32), `msg_type_name` (const char*, 32-char buffer, from `ComponentTypeDesc` or type name lookup)
- The log is updated in `Domain::EndOfFrame()` during the mailbox drain pass — one entry appended per dispatched message per recipient entity
- `IEntityInspectable` gains one new method: `GetMessageLog(Entity entity, DynamicArrayC<MessageLogEntry, 64>& out) const` — fills `out` with the N most recent entries where recipient == entity, in dispatch order
- Log is a global ring across all entities (capacity 64 total entries, not 64 per entity) — simple, bounded, enough for debugging

### EntityInspectSerializer — mailbox_log array filled

- `SerializeEntityInspect` fills `mailbox_log` with entries for the selected entity by calling `GetMessageLog(entity, buf)`
- Each entry serializes as: `{ "frame": <uint32>, "msg_type_crc": <uint32>, "msg_type_name": "<string>" }`
- If no messages have been dispatched to the entity, `mailbox_log` is an empty array `[]`

### MailboxMonitorController (editor side)

- `MailboxMonitorController` class added to `Dia/DiaEntityInspector/`
- Owned by `DiaEntityInspectorPlugin`; receives the `entity.inspect` payload from `EntityInspectorController`
- Parses `mailbox_log` and maintains an editor-side append buffer (up to 256 entries total, discards oldest on overflow)
- When "paused", the controller stops updating the display buffer from incoming payloads but continues buffering internally
- On "snapshot", the controller exports the current display buffer as a JSON file to the local filesystem via a DiaAPI `entity.mailbox_snapshot` command

### Mailbox tab UI

- Mailbox tab renders a scrollable table of message log entries; columns: Frame, Message Type, (sender direction badge where applicable)
- Each row is colour-coded by message type (cycle through a fixed palette per distinct type CRC)
- Histogram section (collapsible) shows one bar per message type seen in the current display buffer: bar height = count, label = type name
- Pause button toggles display freeze; button text changes to "Paused — Resume" while frozen
- Snapshot button sends `entity.mailbox_snapshot` DiaAPI command; the game returns the current log as the command response payload (JSON); the editor saves it locally to a user-chosen path (browser download or file-save dialog)
- If no entity is selected, tab shows "Select an entity to monitor mailbox traffic"
- If entity is selected but log is empty, tab shows "No messages dispatched to this entity yet"

### IEntityInspectable amendment

`GetMessageLog` method signature:

```cpp
struct MessageLogEntry {
    uint32_t frame;
    uint32_t msgTypeCrc;
    char     msgTypeName[32];
};

virtual void GetMessageLog(
    Entity entity,
    Dia::Core::Containers::DynamicArrayC<MessageLogEntry, 64>& out) const = 0;
```

This method is added to `IEntityInspectable` in `Dia/DiaEntity/IEntityInspectable.h` and implemented in `Domain`.

## Data Model

### mailbox_log entries in entity.inspect payload

```json
"mailbox_log": [
  { "frame": 1021, "msg_type_crc": 555444333, "msg_type_name": "DamageMsg" },
  { "frame": 1022, "msg_type_crc": 555444333, "msg_type_name": "DamageMsg" },
  { "frame": 1023, "msg_type_crc": 777888999, "msg_type_name": "HealMsg" }
]
```

### EntityMessageLog internal structure (in Domain)

```cpp
// Dia/DiaEntity/Domain.h (private)
struct MessageLogEntry {
    uint32_t frame           = 0;
    uint32_t recipientIndex  = 0;
    uint32_t msgTypeCrc      = 0;
    char     msgTypeName[32] = {};
};

// Ring buffer: capacity 64 total across all entities
Dia::Core::Containers::DynamicArrayC<MessageLogEntry, 64> mMessageLog;
uint32_t mMessageLogHead = 0;   // oldest entry index (ring pointer)
uint32_t mFrameCounter   = 0;   // incremented each EndOfFrame()
```

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaMailbox/Mailbox.h` | Add internal name table (`StringCRC → const char*`); add `DIA_MAILBOX_REGISTER_TYPE` macro |
| `Dia/DiaMailbox/Mailbox.cpp` | Implement name table lookup |
| `Dia/DiaEntity/IEntityInspectable.h` | Add `MessageLogEntry` struct + `GetMessageLog` virtual method |
| `Dia/DiaEntity/Domain.h` | Add `mMessageLog`, `mMessageLogHead`, `mFrameCounter` members; add internal `AppendMessageLog` |
| `Dia/DiaEntity/Domain.cpp` | Implement ring buffer append in `EndOfFrame()` drain pass; implement `GetMessageLog` |
| `Dia/DiaEntityInspector/EntityInspectSerializer.cpp` | Modified — fill `mailbox_log` array (previously empty `[]`) |
| `Dia/DiaEntityInspector/MailboxMonitorController.h` | New |
| `Dia/DiaEntityInspector/MailboxMonitorController.cpp` | New — includes `entity.mailbox_snapshot` command handler (returns log as response payload) |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.h` | Modified — add `MailboxMonitorController mMailboxController` member |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.cpp` | Modified — forward payload to `mMailboxController` |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj` | Add `MailboxMonitorController.h/.cpp` |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj.filters` | Add `MailboxMonitorController.h/.cpp` |
| `Tests/GoogleTests/DiaEntityInspector/MailboxMonitorTests.cpp` | New — ring buffer fill, entity filter, pause semantics |
| `Tests/GoogleTests/DiaEntity/MessageLogTests.cpp` | New — ring overflow, frame counter, per-entity filter |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all entity/component IDs | `msgTypeCrc` in `MessageLogEntry` is the `StringCRC::GetValue()` of the message type. `msgTypeName` carries the human-readable string. No raw string comparisons at runtime. Compliant. |
| PD-002 | PU/Phase/Module architecture for app structure | `MailboxMonitorController` is a plain library class, no Module/Phase/PU inheritance. Compliant. |
| PD-004 | No STL containers in public APIs | `GetMessageLog` uses `DynamicArrayC<MessageLogEntry, 64>`. `MessageLogEntry` uses fixed-size `char[32]` buffer, not `std::string`. No STL in public headers. Compliant. |
| PD-005 | x64 only | No 32-bit code paths. Compliant. |
| PD-006 | Visual Studio project files are source of truth | All new files added to existing `.vcxproj` and `.vcxproj.filters`. Compliant. |
| PD-007 | C++20 required | All new code compiled under `/std:c++20`. Compliant. |
| PD-008 | `Directory.Build.props` owns output paths | No per-project output overrides. Compliant. |
| SED-ENT-007 | Mailbox monitor shows last N=64 messages in a ring buffer; no streaming | `mMessageLog` is a 64-entry ring in `Domain`. `GetMessageLog` filters by entity. Editor-side buffer extends to 256 for display history. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Log capture point | Messages are drained from the mailbox during `Domain::EndOfFrame()`. The ring buffer is appended during that drain pass — after delivery, not before. Does this matter (i.e. does the log show messages that were delivered, or messages that are pending delivery)? | Correct — the log records delivered messages. "Pending" messages that haven't been drained yet are not logged until `EndOfFrame()`. This is the right semantic for a traffic monitor: show what actually happened, not what is queued. |
| 2 | Message type name source | The `msgTypeName` field needs a string name for the message type CRC. DiaMailbox types are not registered in `ComponentRegistry` — they are arbitrary structs. Where does the name come from? | **Option B — Macro:** `DIA_MAILBOX_REGISTER_TYPE(mailbox, T, capacity, "TypeName")` wraps `RegisterType` and stores the name at the same call site. Follows the `DIA_COMPONENT`/`DIA_REGISTER_MODULE` macro pattern; name is mandatory and co-located with registration. `Mailbox` gains an internal name table (`StringCRC → const char*`) keyed by type CRC. `GetMessageLog` fills `msgTypeName` from that table; if the type was registered without the macro (or macro not used), falls back to empty string. |
| 3 | Global ring vs per-entity ring | The ring buffer is global (64 entries across all entities). In a Domain with many active entities, 64 entries could fill in one frame if multiple entities receive messages. Is 64 total enough? | 64 total is sufficient for debugging typical scenarios. The editor-side buffer accumulates across slow-poll ticks (up to 256) giving more history. If a user has 64 entities all receiving messages in a single frame, they will see partial data — acceptable for v1. The capacity is a named constant (`kMessageLogCapacity = 64`) that can be tuned. |
| 4 | Pause semantics | When paused, the controller "stops updating the display buffer but continues buffering internally." What happens to entries buffered during pause when the user resumes? | On resume, the buffered entries since pause are flushed into the display buffer all at once, up to display capacity (256). Entries that don't fit are discarded (oldest first). The user sees a "jump" to current state. This is simpler than replaying them chronologically and sufficient for debugging. |
| 5 | Snapshot file path | Snapshot writes to `Cluiche/out/CluicheTest/sessions/<id>/mailbox_snapshot_<entity>_<frame>.json`. The session ID comes from DiaObservation. Is the snapshot command responsible for resolving the session path, or does it receive it as a parameter? | **Option i — Return data in command response:** `entity.mailbox_snapshot` serializes the current log to JSON and returns it as the `CommandResponse` payload. The editor receives the data and saves it locally. No file I/O in the game process; no DiaObservation dependency in the command handler. ~256 entries ≈ 10–15 KB — fine for a WebSocket response. The snapshot output path (and whether to save at all) is entirely the editor's decision. |

## Open Questions

None.

## Status

`Approved`
