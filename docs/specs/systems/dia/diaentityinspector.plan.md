**Spec:** @docs/specs/systems/dia/diaentityinspector.md
**Status:** In Progress

## Implementation Plan

### Prerequisites

All dependencies exist:
- `IEditorPlugin` interface + `REGISTER_EDITOR_PLUGIN` — `Dia/DiaEditor/Plugin/`
- `WebUIBridge` for C++↔UI — `Dia/DiaEditor/UI/`
- `IEntityInspectable` interface (Tier a + b) — `Dia/DiaEntity/IEntityInspectable.h`
- `DiaDebugServer` WebSocket infrastructure — `Dia/DiaDebugServer/`
- `DiaDebugProtocol` message types — `Dia/DiaDebugProtocol/`
- `DiaAPI` command dispatch — `Dia/DiaAPI/`
- `DebugLayerManager::GetSelectedEntityId()` — `Dia/DiaVisualDebugger/`
- `ComponentRegistry` + `ComponentTypeDesc` for field introspection — `Dia/DiaEntity/`

**Blocked on:** DiaReflect Phase 3 (T11-T13 in DiaEntity plan) for `IEntityInspectable::ReadField`/`WriteField` to work with full reflection. However, scaffold + protocol work can proceed independently.

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Plugin scaffold: create `Dia/DiaEntityInspector/` project, `DiaEntityInspectorPlugin` class, register, add to solution | Plugin appears in EditorPluginRegistry; `GetName()` returns "DiaEntityInspector" | Not Started | sonnet | Follow existing editor plugin patterns |
| 2 | Debug data type constants: create `Dia/DiaEntity/DebugDataTypes.h` with `kEntityInspect`, `kEntityInspectRequest`, `kEntityFindByName`, `kEntityWriteField` | Constants compile; `StringCRC` values correct | Not Started | haiku | 4 `static const StringCRC` declarations in a header |
| 3 | Protocol version bump: update `kProtocolVersion` to 2 in `DiaDebugProtocol/Protocol.h` | Handshake rejects version 1 clients | Not Started | haiku | One-line change + document breaking change |
| 4 | EntityInspectSerializer: implement `SerializeEntityInspect()` — reads entity state via `IEntityInspectable` and produces JSON payload | Serialize a test entity → JSON matches spec schema (entity, components, hierarchy, queries, mailbox_log) | Not Started | opus | Core complexity: iterate components via `GetComponentTypeIds()`, read each field via `ReadField()`, build JSON |
| 5 | `entity.inspect_request` topic handler: register in DiaDebugServer, on receipt call `SerializeEntityInspect` and push result | Send inspect_request → receive entity.inspect data update | Not Started | sonnet | Wire into DiaDebugServer's `QueryRegistry` |
| 6 | `entity.write_field` DiaAPI command: register handler, call `IEntityInspectable::WriteField`, return success/failure | Send write_field → field changes → next inspect push shows new value | Not Started | sonnet | DiaAPI command handler pattern; validate entity handle + component type + field name |
| 7 | `entity.find_by_name` DiaAPI command: register handler, O(N) scan Domain for matching debug name, return handle | Send find_by_name("Player") → returns {index: 1, gen: 3} | Not Started | sonnet | Simple linear scan; return {index: -1} if not found |
| 8 | Slow poll: every 30 frames, re-serialize selected entity and push `entity.inspect` without waiting for request | After 30 frames, inspect payload received without explicit request | Not Started | sonnet | Frame counter in DiaDebugServer topic handler; reset on selection change |
| 9 | CluicheTest wiring: EntityModule polls `DebugLayerManager::GetSelectedEntityId()` each frame, pushes immediate inspect on change | Select entity via debug picking → immediate inspect push to editor | Not Started | sonnet | Add to existing sim-side module; check if selected ID changed since last frame |
| 10 | Entity list panel (left): subscribe to `entity.inspect` topic, render all entities with debug names + component tags + hierarchy indentation | Editor shows all live entities; search filters by name; filter chips by component type | Not Started | sonnet | WebUIBridge populates entity list from inspection data; needs initial "list all entities" mechanism |
| 11 | Entity list — "list all entities" mechanism: on connect, request entity summary (index, gen, debug_name, component tags) for entire Domain | Connection established → entity list populated before user selects anything | Not Started | sonnet | New lightweight `entity.list` data type or initial dump on subscribe; avoid serializing full state for all entities |
| 12 | Fields tab (right): component accordion with type-aware field widgets (bool/int/float/vec2/string/StringCRC/EntityRef) | Select entity → fields tab shows all components + fields with correct types | Not Started | sonnet | Use `ComponentTypeDesc::fields` for metadata; `FieldDesc::kind` maps to widget type |
| 13 | Field editing: edit value → send `entity.write_field` → on failure show transient error and revert | Edit position → game state changes; edit invalid → field shows error indicator | Not Started | sonnet | Debounce edits (Enter or defocus); optimistic UI with revert on failure |
| 14 | Queries tab: display registered query signatures, entity counts, membership flag for selected entity | Queries tab shows which queries the selected entity belongs to | Not Started | sonnet | Use `IEntityInspectable::GetQueryCount/GetQuerySignature/GetQueryEntityCount` |
| 15 | Mailbox tab: ring-buffer display of last 64 messages for selected entity, type badges, self-only filter | Mailbox tab shows message dispatch log with color-coded types | Not Started | sonnet | Parse `mailbox_log` array from inspect payload; client-side ring buffer display |
| 16 | Watch tab: add (entity, component, field) triples, persist in-memory across reconnects, poll each watched entity | Watch tab shows 6+ watched values with live delta indicators | Not Started | opus | Stable reference via debug name (not handle); on reconnect use `entity.find_by_name` to rebind |
| 17 | Tier (c) structural controls: render Add Component / Remove Component / Destroy Entity buttons — disabled with "Not available in v1" tooltip | Buttons visible but non-functional; tooltip explains | Not Started | haiku | UI-only: disabled buttons with tooltips |
| 18 | UI assets: React/HTML for inspector panels (entity list + tabs + fields + queries + mailbox + watch) | UI renders correctly matching mockup_b_split.html layout | Not Started | sonnet | Dark purple theme; reuse mockup as reference implementation |

### Implementation Order

```
Phase 1 — Scaffold + Protocol:
  T1 → T2 → T3

Phase 2 — Game-Side Serialization:
  T4 → T5 → T6 → T7 → T8 → T9

Phase 3 — Editor-Side UI:
  T10 → T11 → T12 → T13 → T14 → T15 → T16 → T17

Phase 4 — UI Assets:
  T18 (runs in parallel with Phase 3)
```

Phase 1 and Phase 2 can run before DiaReflect Phase 3 if `IEntityInspectable` methods are stubbed. Phase 3 (editor UI) needs real data from the game.

### Key Files to Create

| File | Purpose |
|------|---------|
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj` | MSBuild project |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj.filters` | IDE filters |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.h/cpp` | IEditorPlugin subclass |
| `Dia/DiaEntityInspector/EntityInspectorController.h/cpp` | Fields tab logic |
| `Dia/DiaEntityInspector/QueryBrowserController.h/cpp` | Queries tab logic |
| `Dia/DiaEntityInspector/MailboxMonitorController.h/cpp` | Mailbox tab logic |
| `Dia/DiaEntityInspector/EntityWatchListController.h/cpp` | Watch tab logic |
| `Dia/DiaEntityInspector/EntityInspectSerializer.h/cpp` | Game-side JSON serialization |
| `Dia/DiaEntity/DebugDataTypes.h` | Data type StringCRC constants |
| `Cluiche/CluicheTest/Modules/EntityModule.cpp` | Add selected-entity polling + inspect push |
| `Tests/GoogleTests/DiaEntityInspector/EntityInspectSerializerTests.cpp` | Serializer unit tests |
| `Cluiche/CluicheEditor/UI/src/plugins/entity-inspector/` | React UI components |

### Key Patterns to Reuse

- **Plugin scaffold:** Same as other editor plugins
- **WebSocket topic handler:** `DiaDebugServer::RegisterTopicHandler(dataType, handler)` pattern
- **DiaAPI commands:** `CommandDispatcher::Register(commandName, handler)` pattern
- **Component introspection:** `ComponentRegistry::Find(typeId)` → `ComponentTypeDesc` → `FieldDesc`
- **Entity list:** Similar to `EntityStatsDrawer` pattern (enumerate Domain entities)
- **Field serialization:** `IEntityInspectable::ReadField(entity, componentTypeId, fieldName)` returns `Json::Value`

### Verification

- `dia run cluicheeditor` → DiaEntityInspector plugin loads
- `dia run cluichetest` → editor connects → entity list populates
- Select entity in game (via debug picking) → inspector updates immediately
- Edit a field → game state changes in real-time
- Watch tab: add entry → survives editor reconnect (rebinds via debug name)
- Mailbox tab: trigger messages in game → log appears in editor
- Queries tab: shows correct membership for selected entity
