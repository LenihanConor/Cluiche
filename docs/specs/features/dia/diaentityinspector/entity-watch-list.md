# Feature Spec: entity-watch-list

**System:** DiaEntityInspector
**App:** Dia
**Status:** Draft
**Mockup:** @docs/research/diaentit_visual_debug/mockup_b_split.html

## Summary

Implement the Watch tab: a persistent, ordered list of (entity debug name, component type, field name) triples that shows live field values across multiple entities simultaneously. Watch entries survive editor reconnects by re-binding via `entity.find_by_name`; they are in-memory only for the editor session. Values are refreshed by polling each watched entity individually using the existing `entity.inspect_request` / `entity.inspect` mechanism.

## Traceability

| Level | Spec |
|---|---|
| Platform | [Cluiche.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentityinspector.md](../../systems/dia/diaentityinspector.md) |
| Depends on feature | [entity-inspector-panel.md](entity-inspector-panel.md) |
| Depends on feature | [editor-inspection.md](../diaentity/editor-inspection.md) |

## Goals

- Developers can pin specific (entity, component, field) triples and monitor their values across entities without having to re-select the entity in the list each time
- Watch entries survive a game reconnect — they re-bind by entity debug name so the developer doesn't need to re-add them manually after restarting the game
- The Watch tab reuses the existing `entity.inspect_request` + `entity.inspect` data path — no new WebSocket topic or DiaAPI commands required

## Acceptance Criteria

### Adding watch entries

- Right-clicking any field row in the Fields tab shows a context menu with "Add to Watch List"
- Clicking "Add to Watch List" adds a triple `(entity_debug_name, component_type_crc, field_name)` to the watch list
- If the identical triple already exists in the watch list, the add is a no-op (no duplicates)
- Watch entries can also be added manually via a text input row at the bottom of the Watch tab: three fields (entity name, component name, field name) + "Add" button
- Maximum 32 watch entries; attempting to add a 33rd shows a toast "Watch list full (max 32)"

### Watch tab display

- Watch tab renders a table with columns: Entity, Component, Field, Value, Status
- Each row shows the current live value for that triple in the Value column
- Status column shows one of: `live` (green — entity found and field readable), `stale` (yellow — last value shown but entity not updated this tick), `not found` (red — `entity.find_by_name` returned no match), `error` (red — field or component not found on the entity)
- Rows can be reordered via drag-and-drop
- Each row has a × remove button
- If the watch list is empty, the tab shows "No fields watched — right-click any field in the Fields tab to add"

### Value refresh

- `EntityWatchListController` maintains the list of watch triples
- Each slow-poll tick (every 30 frames), the controller sends one `entity.inspect_request` per unique entity referenced in the watch list
- When the `entity.inspect` response arrives, the controller extracts the relevant field values and updates the display
- Fields for entities not currently selected are fetched this way — the watch list is independent of the selection

### Reconnect re-bind

- On WebSocket reconnect (detected by `GameConnectionManager` reconnect callback), `EntityWatchListController` iterates all watch entries
- For each unique entity debug name, it sends `entity.find_by_name` and updates the stored entity handle (index + gen)
- Entries whose debug name returns `{ entity_index: -1 }` are marked with Status `not found` until a future rebind succeeds
- The watch list contents (triples) are preserved across reconnect; only the handles are re-resolved

### Persistence (v1 scope)

- Watch list is in-memory only — not written to disk; closing and reopening the editor resets it
- No import/export in v1

## Data Model

### Watch entry (editor side, in EntityWatchListController)

```cpp
struct WatchEntry {
    char        entityDebugName[64];   // stable identity across reconnects
    uint32_t    componentTypeCrc;      // StringCRC value
    char        componentTypeName[64]; // human-readable, for display
    char        fieldName[32];

    // Runtime-resolved (cleared on reconnect, re-bound via entity.find_by_name)
    int32_t     entityIndex = -1;      // -1 = unresolved
    uint32_t    entityGen   = 0;

    // Display state
    Json::Value lastValue;             // most recent field value received
    enum class Status { Live, Stale, NotFound, Error } status = Status::NotFound;
};
```

### entity.inspect_request for non-selected entity (same schema, new use)

The watch list reuses the existing request/response schema. The controller sends:

```json
{ "entity_index": 42, "entity_gen": 3 }
```

And reads back the `entity.inspect` payload, extracting only the fields it needs. No new message types required.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntityInspector/EntityWatchListController.h` | New |
| `Dia/DiaEntityInspector/EntityWatchListController.cpp` | New — watch entry store, reconnect re-bind, per-entity poll, value extraction |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.h` | Modified — add `EntityWatchListController mWatchController` member |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.cpp` | Modified — forward payload + reconnect callback to `mWatchController` |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj` | Add `EntityWatchListController.h/.cpp` |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj.filters` | Add `EntityWatchListController.h/.cpp` |
| `Tests/GoogleTests/DiaEntityInspector/EntityWatchListTests.cpp` | New — add/remove/deduplicate, reconnect re-bind, not-found status, max capacity |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all entity/component IDs | `componentTypeCrc` in `WatchEntry` is the `StringCRC::GetValue()` of the component type. All `entity.find_by_name` and `entity.inspect_request` messages use CRC values for component identity. Compliant. |
| PD-002 | PU/Phase/Module architecture for app structure | `EntityWatchListController` is a plain library class, no Module/Phase/PU inheritance. Compliant. |
| PD-004 | No STL containers in public APIs | `WatchEntry` uses fixed-size `char[]` buffers and `Json::Value` (blessed). No STL in public headers. Compliant. |
| PD-005 | x64 only | No 32-bit code paths. Compliant. |
| PD-006 | Visual Studio project files are source of truth | New files added to existing `.vcxproj` and `.vcxproj.filters`. Compliant. |
| PD-007 | C++20 required | All new code compiled under `/std:c++20`. Compliant. |
| PD-008 | `Directory.Build.props` owns output paths | No per-project output overrides. Compliant. |
| SED-ENT-002 | Watch list stable reference = entity debug name | `WatchEntry::entityDebugName` is the stable key. Handles are re-resolved via `entity.find_by_name` on reconnect. Compliant. |
| SED-ENT-006 | `entity.find_by_name` returns entity handle for watch list rebind | Used exactly as specified — one call per unique entity name on reconnect. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Poll rate for non-selected entities | The watch list polls each watched entity every 30 frames (same slow-poll rate as the selected entity). If 10 entities are watched, this is 10 `entity.inspect_request` messages per 30 frames. Is that acceptable traffic? | 10 requests per 30 frames at 60 FPS = one request every 3 frames = ~20 requests/second total. Each response is ~1–2 KB. ~40 KB/s total — well within WebSocket capacity for a local debug connection. Acceptable for v1. If the watch list grows large, a dedicated `entity.watch_values` topic (game-side aggregated push) is a future optimisation. |
| 2 | Value extraction from inspect payload | The watch list needs only specific (component, field) pairs from the `entity.inspect` payload. Should the controller parse the full payload or send a filtered request? | Parse the full payload — the inspect payload is small and already structured. Sending a filtered request would require a new protocol message type. Full parse is simpler and sufficient. |
| 3 | Reconnect callback source | `EntityWatchListController` needs to be notified on reconnect to trigger re-bind. Which object owns the reconnect callback, and how is the controller registered? | `DiaEntityInspectorPlugin::OnUpdate` polls `GameConnectionManager`'s connection state each tick. On transition from disconnected → connected, it calls `mWatchController.OnReconnect()`. No separate callback registration mechanism needed — the plugin already has access to both. |
| 4 | "Stale" status timing | A row is marked `stale` when "entity not updated this tick." How many missed ticks before a `live` entry becomes `stale`? | After 2 consecutive slow-poll ticks with no response for that entity (i.e. 60 frames / ~1 second), the entry transitions to `stale`. This prevents flicker on a single dropped poll while still alerting the developer that data is no longer flowing. |
| 5 | Manual add input validation | The manual-add row requires entity name, component name, and field name as free-text strings. What validation happens on "Add"? | On Add: (1) send `entity.find_by_name` — if not found, add entry immediately with Status `not found` (user may have typed it for an entity not yet spawned); (2) if found, send `entity.inspect_request` and validate component + field exist before setting Status `live`. Entry is added to the list immediately regardless of validation outcome — the Status column communicates resolution. |

## Open Questions

None.

## Status

`Approved`
