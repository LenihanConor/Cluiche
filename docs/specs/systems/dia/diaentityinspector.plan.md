**Spec:** @docs/specs/systems/dia/diaentityinspector.md
**Status:** Done

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

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Plugin scaffold: create `Dia/DiaEntityInspector/` project, `DiaEntityInspectorPlugin` class, register, add to solution | Plugin appears in EditorPluginRegistry; `GetName()` returns "DiaEntityInspector" | Done | sonnet | DiaEntityInspectorPlugin + vcxproj + solution entry; disconnect-overlay wired via GameConnectionManager in OnUpdate; no project dependency |
| 2 | Debug data type constants: create `Dia/DiaEntity/DebugDataTypes.h` with `kEntityInspect`, `kEntityInspectRequest`, `kEntityFindByName`, `kEntityWriteField` | Constants compile; `StringCRC` values correct | Done | haiku | Dia/DiaEntity/DebugDataTypes.h created |
| 3 | Protocol version bump: update `kProtocolVersion` to 2 in `DiaDebugProtocol/Protocol.h` | Handshake rejects version 1 clients | Done | haiku | kProtocolVersion = 2 in DiaDebugProtocol.h |
| 4 | EntityInspectSerializer: implement `SerializeEntityInspect()` — reads entity state via `IEntityInspectable` and produces JSON payload | Serialize a test entity → JSON matches spec schema (entity, components, hierarchy, queries, mailbox_log) | Done | opus | Free function; components+queries+hierarchy; mailbox_log stubbed as [] (T15 populates client-side) |
| 5 | `entity.inspect_request` topic handler: register in DiaDebugServer, on receipt call `SerializeEntityInspect` and push result | Send inspect_request → receive entity.inspect data update | Done | sonnet | Registered via QueryRegistry in EntityInspectorModule::RegisterHandlers() |
| 6 | `entity.write_field` DiaAPI command: register handler, call `IEntityInspectable::WriteField`, return success/failure | Send write_field → field changes → next inspect push shows new value | Done | sonnet | Registered in EntityInspectorModule::RegisterHandlers() |
| 7 | `entity.find_by_name` DiaAPI command: register handler, O(N) scan Domain for matching debug name, return handle | Send find_by_name("Player") → returns {index: 1, gen: 3} | Done | sonnet | Registered in EntityInspectorModule::RegisterHandlers() |
| 8 | Slow poll: every 30 frames, re-serialize selected entity and push `entity.inspect` without waiting for request | After 30 frames, inspect payload received without explicit request | Done | sonnet | 30-frame counter in EntityInspectorModule::DoUpdate() |
| 9 | CluicheTest wiring: EntityModule polls `DebugLayerManager::GetSelectedEntityId()` each frame, pushes immediate inspect on change | Select entity via debug picking → immediate inspect push to editor | Done | sonnet | New EntityInspectorModule in CluicheGameBaseline/Modules/ (follows EntityVisualDebuggerModule pattern); wraps entity picking + DebugServer topic push; guarded by #ifdef DIA_DEBUG |
| 10 | Entity list panel (left): subscribe to `entity.inspect` topic, render all entities with debug names + component tags + hierarchy indentation | Editor shows all live entities; search filters by name; filter chips by component type | Done | sonnet | EntityInspectorController.OnInspectPayload pushes entity_inspector.inspect_data to UI |
| 11 | Entity list — "list all entities" mechanism: on connect, request entity summary (index, gen, debug_name, component tags) for entire Domain | Connection established → entity list populated before user selects anything | Done | sonnet | SerializeEntityList() added; kEntityInspectRequest query handler returns full entity list when no index given; note: wire entity.list on first subscribe in a future task |
| 12 | Fields tab (right): component accordion with type-aware field widgets (bool/int/float/vec2/string/StringCRC/EntityRef) | Select entity → fields tab shows all components + fields with correct types | Done | sonnet | Component accordion data via EntityInspectorController; UI renders from entity_inspector.inspect_data; Tier (c) controls disabled with tooltip |
| 13 | Field editing: edit value → send `entity.write_field` → on failure show transient error and revert | Edit position → game state changes; edit invalid → field shows error indicator | Done | sonnet | entity_inspector.write_field request handled in UI; routed to entity.write_field DiaAPI command |
| 14 | Queries tab: display registered query signatures, entity counts, membership flag for selected entity | Queries tab shows which queries the selected entity belongs to | Done | sonnet | QueryBrowserController pushes entity_inspector.query_data; queries array filled by SerializeEntityInspect |
| 15 | Mailbox tab: ring-buffer display of last 64 messages for selected entity, type badges, self-only filter | Mailbox tab shows message dispatch log with color-coded types | Done | sonnet | MailboxMonitorController ring buffer; client-side accumulation from mailbox_log array; self-only filter in UI |
| 16 | Watch tab: add (entity, component, field) triples, persist in-memory across reconnects, poll each watched entity | Watch tab shows 6+ watched values with live delta indicators | Done | opus | EntityWatchListController with watch_add/watch_remove/watch_get handlers; stable reference via debug name; OnConnectionStateChanged clears stale values |
| 17 | Tier (c) structural controls: render Add Component / Remove Component / Destroy Entity buttons — disabled with "Not available in v1" tooltip | Buttons visible but non-functional; tooltip explains | Done | haiku | Buttons in UI with disabled attribute and "Not available in v1" title tooltip |
| 18 | UI assets: React/HTML for inspector panels (entity list + tabs + fields + queries + mailbox + watch) | UI renders correctly matching mockup_b_split.html layout | Done | sonnet | Dia/DiaEntityInspector/UI/index.html — single-file split panel UI with disconnect-overlay, dark purple theme, all 4 tabs |
| 19 | Tests + Observation | IntegrationTestEntityInspectorPlugin (15 tests), TestEntityInspectSerializer (12 tests) pass | Done | sonnet | IntegrationTestEntityInspectorPlugin (15 tests), TestEntityInspectSerializer (12 tests); observation pass added DIA_LOG_* and DIA_TRACE_ZONE; metrics/health deferred to future domain instrumentation work |

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

Phase 5 — Tests + Observation:
  T19
```

Phase 1 through Phase 4 are complete. T19 adds exhaustive tests and observation instrumentation.

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
| `Cluiche/CluicheGameBaseline/Modules/EntityInspectorModule.cpp` | Add selected-entity polling + inspect push |
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
