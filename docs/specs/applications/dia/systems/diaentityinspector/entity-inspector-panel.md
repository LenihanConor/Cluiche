# Feature Spec: entity-inspector-panel

**System:** DiaEntityInspector
**App:** Dia
**Status:** Draft
**Mockup:** @docs/research/diaentit_visual_debug/mockup_b_split.html

## Summary

Implement the `DiaEntityInspectorPlugin` scaffold, the entity list panel, the component field accordion (Fields tab), live field editing via `WriteField`, the `entity.inspect` WebSocket topic, and the game-side `EntityInspectSerializer`. This is the foundational feature of DiaEntityInspector — all subsequent features (query browser, mailbox monitor, watch list) build on the topic and plugin scaffold established here.

## Traceability

| Level | Spec |
|---|---|
| Platform | [Cluiche.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | [diaentityinspector.md](diaentityinspector.md) |
| Depends on feature | editor-inspection |
| Depends on feature | debug-entity-picking |
| Depends on system | [diadebugprotocol.md](../diadebugprotocol/diadebugprotocol.md) |
| Depends on system | [diadebugserver.md](../diadebugserver/diadebugserver.md) |
| Depends on system | diaeditor |

## Goals

- CluicheEditor can connect to a running game and display all live entities and their component fields without any per-type editor code
- Developers can edit component field values in real time while the game is running
- The `entity.inspect` WebSocket topic is established as the data channel for all four DiaEntityInspector tabs
- The plugin scaffold is in place so subsequent features (query browser, mailbox monitor, watch list) only need to add their controller and UI

## Acceptance Criteria

### Plugin scaffold
- `DiaEntityInspectorPlugin` class exists in `Dia/DiaEntityInspector/`, inherits `IEditorPlugin`
- Plugin registers via `REGISTER_EDITOR_PLUGIN(DiaEntityInspectorPlugin, "DiaEntityInspector")`
- `GetLayoutMode()` returns `LayoutMode::kDockable`
- `OnLoad` / `OnUnload` / `OnUpdate` lifecycle hooks work correctly

### Entity list (left panel)
- Left panel shows all live entities as a flat list with optional tree indentation for hierarchy (parent/child)
- Each row shows entity debug name (or `"[42]"` index fallback if no debug name)
- Each row shows component short tags (up to 4 tags, 4 chars each)
- Search bar filters by name substring (case-insensitive)
- Filter chips: one per registered component type; clicking a chip restricts list to entities that have that component
- Selecting an entity sends an `entity.inspect_request` to the server and triggers an immediate `entity.inspect` push
- If the selected entity is destroyed, the panel shows a "Entity no longer exists" banner and clears selection

### Fields tab (right panel)
- Right panel header (context strip) shows entity debug name, index/gen, and component tags
- Fields tab shows one accordion section per component
- Each section header shows component type name (human-readable string from `ComponentTypeDesc`)
- Each section body shows one row per declared field: name, type badge, current value
- Field widgets are type-aware:
  - `bool` → checkbox
  - `int` / `uint` → numeric input
  - `float` → numeric input with 3 decimal places
  - `vec2` → two float inputs (x / y)
  - `vec3` → three float inputs (x / y / z)
  - `StringCRC` → read-only text showing both hex CRC and string name (if available)
  - `EntityRef` → read-only text showing debug name of referenced entity
- Editing a field (changing a value and pressing Enter or defocusing) sends `entity.write_field` DiaAPI command
- If `WriteField` returns false (type mismatch or entity gone), the UI shows a transient error indicator on that field row and reverts to the last good value
- Tier (c) structural controls (Add Component, Remove Component, Destroy Entity) are rendered but disabled and tooltip-labelled "Not available in v1"

### entity.inspect WebSocket topic
- `diaentitytemplate/DebugDataTypes.h` defines `kEntityInspect`, `kEntityInspectRequest`, `kEntityFindByName`, `kEntityWriteField` as `Dia::Core::StringCRC` constants
- Game side registers a topic handler for `kEntityInspectRequest` in DiaDebugServer
- On receiving `kEntityInspectRequest` with an entity index+gen, server calls `SerializeEntityInspect` and pushes the result as a `kDataUpdate` with `dataType = kEntityInspect`
- `entity.write_field` DiaAPI command is registered in the game; on execution it calls `Domain::WriteField` (via `IEntityInspectable`) and returns success/failure
- `entity.find_by_name` DiaAPI command is registered in the game; returns entity index+gen for the named entity or `{index: -1}` if not found
- Slow poll: every 30 frames the server re-serializes and pushes `entity.inspect` for the currently-selected entity ID without waiting for a request
- `kProtocolVersion` bumped to 2 in `DiaDebugProtocol/Protocol.h`

### CluicheTest wiring
- CluicheTest `EntityModule` (or equivalent sim-side module) polls `DebugLayerManager::GetSelectedEntityId()` each frame
- When the selected ID changes, it maps the ID to a `Domain` entity handle and pushes an immediate `entity.inspect` update

### EntityInspectSerializer (game side)
- `Dia/DiaEntityInspector/EntityInspectSerializer.h` (and `.cpp`) implement `SerializeEntityInspect`
- Serializes: entity index/gen/debug_name, per-component type_id_crc/type_name/fields, hierarchy parent/child count, empty queries array (placeholder for query-browser-tab), empty mailbox_log array (placeholder for mailbox-traffic-monitor)
- Returns a valid `Json::Value` matching the payload schema in the system spec
- If the entity handle is invalid, returns `Json::Value::null`

## Data Model

### entity.inspect_request payload (Editor → Game)

```json
{
  "entity_index": 42,
  "entity_gen": 3
}
```

### entity.write_field payload (Editor → Game, via DiaAPI kCommandRequest)

```json
{
  "command": "entity.write_field",
  "payload": {
    "entity_index": 42,
    "entity_gen": 3,
    "component_type_crc": 1234567890,
    "field_name": "position",
    "value": [1.0, 2.0]
  }
}
```

### entity.find_by_name payload (Editor → Game, via DiaAPI kCommandRequest)

```json
{
  "command": "entity.find_by_name",
  "payload": { "debug_name": "Player" }
}
```

Response:
```json
{ "entity_index": 42, "entity_gen": 3 }
```
or
```json
{ "entity_index": -1 }
```

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj` | New — static library project |
| `Dia/DiaEntityInspector/DiaEntityInspector.vcxproj.filters` | New |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.h` | New — `IEditorPlugin` subclass |
| `Dia/DiaEntityInspector/DiaEntityInspectorPlugin.cpp` | New |
| `Dia/DiaEntityInspector/EntityInspectorController.h` | New |
| `Dia/DiaEntityInspector/EntityInspectorController.cpp` | New |
| `Dia/DiaEntityInspector/EntityInspectSerializer.h` | New — `SerializeEntityInspect` free function |
| `Dia/DiaEntityInspector/EntityInspectSerializer.cpp` | New |
| `Dia/diaentitytemplate/DebugDataTypes.h` | New — `kEntityInspect`, `kEntityInspectRequest`, `kEntityFindByName`, `kEntityWriteField` constants |
| `Dia/diaentitytemplate/diaentitytemplate.vcxproj` | Add `DebugDataTypes.h` |
| `Dia/diaentitytemplate/diaentitytemplate.vcxproj.filters` | Add `DebugDataTypes.h` |
| `Dia/DiaDebugProtocol/Protocol.h` | Bump `kProtocolVersion` to 2 |
| `Cluiche/CluicheTest/EntityModule.cpp` (or equivalent) | Add `GetSelectedEntityId()` poll + `entity.inspect` push |
| `Cluiche/Cluiche.sln` | Add `DiaEntityInspector.vcxproj` |
| `Tests/GoogleTests/DiaEntityInspector/EntityInspectSerializerTests.cpp` | New |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all entity/component IDs | All data type constants use `Dia::Core::StringCRC`. Component type IDs in JSON carry both CRC value and string name for human display. Compliant. |
| PD-002 | PU/Phase/Module architecture for app structure | `DiaEntityInspectorPlugin` is a plain `IEditorPlugin` subclass with no Module/Phase/PU inheritance. Application flow lives in CluicheEditor. Compliant per SED-015. |
| PD-004 | No STL containers in public APIs | `SerializeEntityInspect` takes `IEntityInspectable&` and `Entity` (DiaCore types); returns `Json::Value` (blessed). `IEditorPlugin` API uses `const char*` and `void*`. No STL in any public header. Compliant. |
| PD-005 | x64 only | No 32-bit code paths in new files. Compliant. |
| PD-006 | Visual Studio project files are source of truth | New `.vcxproj` and `.vcxproj.filters` follow existing project conventions. Compliant. |
| PD-007 | C++20 required | All new code compiled under `/std:c++20`. Compliant. |
| PD-008 | `Directory.Build.props` owns output paths | No per-project output path overrides in new `.vcxproj`. Compliant. |
| SED-ENT-001 | Hybrid push: immediate on selection + slow poll every 30 frames | `EntityInspectorController` triggers immediate push on `entity.inspect_request` and game side polls `GetSelectedEntityId()` each frame with a 30-frame throttle for background refresh. Compliant. |
| SED-ENT-003 | `EntityInspectSerializer` is a free function, not a Domain method | Implemented as `SerializeEntityInspect` in `Dia/DiaEntityInspector/EntityInspectSerializer.h`. Domain is not touched. Compliant. |
| SED-ENT-004 | Tier (c) structural edit controls present but disabled | Add Component, Remove Component, Destroy Entity buttons render in the Fields tab with `disabled` state and tooltip "Not available in v1". Compliant. |
| SED-ENT-005 | `entity.write_field` is DiaAPI-routed | Registered as a DiaAPI command; server calls `IEntityInspectable::WriteField`. No parallel write path. Compliant. |
| SED-ENT-009 | `kProtocolVersion` bumped to 2 | `DiaDebugProtocol/Protocol.h` updated. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Entity list rendering | The entity list could have hundreds of entities. Is a flat list with virtual scrolling sufficient, or does the React UI need a tree-view widget for hierarchy? | Flat list with indentation (padding-left per depth) is sufficient for v1. Virtual scrolling can be added later if needed. The tree structure is conveyed via indentation, not a full tree-view widget. |
| 2 | `GetSelectedEntityId` polling in CluicheTest | Which CluicheTest module owns the `DebugLayerManager` reference, and where does the polling code live? | `CluicheTest::EntityModule` (`Cluiche/CluicheTest/Modules/EntityModule.h`) already owns `Domain` and exposes `GetInspectable()`. The poll runs in `EntityModule::DoUpdate` after `Domain::Update`. A `DebugLayerManager*` pointer (or reference) is injected into `EntityModule` at construction time — same pattern used for other per-module dependencies. No new bridge module needed. |
| 3 | `SerializeEntityInspect` queries/mailbox_log placeholders | The payload schema has `queries` and `mailbox_log` arrays. Should v1 serialize them as empty arrays, or omit them entirely? | Empty arrays (`[]`). The editor-side controllers for query browser and mailbox monitor will be added by later features; they need the keys to be present (even empty) so they don't have to special-case missing keys. |
| 4 | `kProtocolVersion` bump migration | Bumping to version 2 will disconnect any editor running protocol v1. Is there a migration path for existing editors, or is this a hard cutover? | Hard cutover. The version bump is the right signal that the handshake payload has changed. Old editors will see `accepted: false` in the handshake response. No backward compatibility wrapper needed. |
| 5 | Component short tags in entity list rows | Entity list rows show "component short tags (up to 4 tags, 4 chars each)." Where do these tags come from — auto-truncated from type name, or manually registered like EntityLabelsDrawer? | Same as EntityLabelsDrawer: caller-registered via a `RegisterComponentTag(StringCRC typeId, const char* shortTag)` method on `DiaEntityInspectorPlugin` (or a shared tag registry). Auto-truncation causes collisions. Registration happens in CluicheTest wiring. |
| 6 | Field edit — when does the UI send the command? | Spec says "pressing Enter or defocusing." For a vec2 field with two inputs, does defocusing one sub-input (x) send immediately, or does the command wait until both x and y are confirmed? | Wait until the user leaves the entire field group (all sub-inputs for that field). Defocusing x while tabbing to y does not send. Defocusing the last sub-input (or pressing Enter from any sub-input) sends the full field value. |
| 7 | `DiaEntityInspector` project dependency on `diaentitytemplate` | `EntityInspectSerializer` calls `IEntityInspectable` methods — so `DiaEntityInspector.vcxproj` must reference `diaentitytemplate.vcxproj`. Is this the correct dependency direction? | Yes. `DiaEntityInspector` depends on `diaentitytemplate` (for `IEntityInspectable` and `Entity` types). `diaentitytemplate` has no dependency on `DiaEntityInspector`. The direction is correct and acyclic. |

## Open Questions

None.

## Status

`Approved`
