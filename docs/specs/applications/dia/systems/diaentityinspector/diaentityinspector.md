# System Spec: DiaEntityInspector

**Research:** @docs/research/diaentit_visual_debug/summary.md
**Mockup:** @docs/research/diaentit_visual_debug/mockup_b_split.html

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaEntityInspector is a CluicheEditor plugin that provides live runtime inspection and field editing of diaentitytemplate `Domain` state. It connects to a running game via WebSocket, subscribes to the `entity.inspect` data topic, and renders a split-panel UI showing the entity list on the left and a detail pane on the right.

The editor exposes four tabs in the detail pane:
- **Fields** — component accordion with live-editable field widgets (Tier b)
- **Queries** — registered query signatures and result counts
- **Mailbox** — ring-buffer log of dispatched messages for the selected entity
- **Watch** — persistent (entity, component, field) triples that survive reconnects

**Location:** `Dia/DiaEntityInspector/` — implements `IEditorPlugin` from DiaEditor framework.

**UI layout:** Option B split panel — 30% left (persistent entity list + search + filter chips) / 70% right (context strip + sub-tabs). Detailed in mockup_b_split.html.

## Responsibilities

- Implement `DiaEntityInspectorPlugin` — `IEditorPlugin` subclass, entry point for CluicheEditor
- Implement `EntityInspectorController` — subscribes to `entity.inspect` topic; drives Fields tab
- Implement `QueryBrowserController` — subscribes to `entity.inspect` topic; drives Queries tab
- Implement `MailboxMonitorController` — receives mailbox_log entries from the inspect payload; drives Mailbox tab
- Implement `EntityWatchListController` — stores persistent watch triples; stable re-bind on reconnect via `entity.find_by_name` command; drives Watch tab
- Implement `EntityInspectSerializer` — free function that serializes a `Domain`'s `IEntityInspectable` into the `entity.inspect` JSON payload on the game side
- Register `entity.inspect` data type constant in `diaentitytemplate/DebugDataTypes.h`
- Register `entity.find_by_name` and `entity.write_field` DiaAPI commands in the game side
- Wire CluicheTest EntityModule to poll `DebugLayerManager::GetSelectedEntityId()` each frame and push `entity.inspect` updates when the selection changes
- Bump `kProtocolVersion` to 2 in DiaDebugProtocol when this system ships

## Non-Responsibilities

- Rendering — UI is React/web served via CEF; this system provides data and command routing only
- Viewport entity picking — handled by DiaVisualDebugger `debug-entity-picking` feature; this system only consumes the selected entity ID
- Blueprint authoring — static file editing belongs in DiaSceneEditor
- Tier (c) structural edit (add/remove component, create/destroy entity) — deferred; controls rendered but disabled

## Public Interfaces

### Data Type Constants (new — `Dia/diaentitytemplate/DebugDataTypes.h`)

```cpp
namespace Dia::Entity::DebugDataType {
    static const Dia::Core::StringCRC kEntityInspect("entity.inspect");
    static const Dia::Core::StringCRC kEntityInspectRequest("entity.inspect_request");
    static const Dia::Core::StringCRC kEntityFindByName("entity.find_by_name");
    static const Dia::Core::StringCRC kEntityWriteField("entity.write_field");
}
```

### entity.inspect WebSocket Payload Schema

Pushed via `kDataUpdate` with `dataType = kEntityInspect`:

```json
{
  "entity": {
    "index": 42,
    "gen": 3,
    "debug_name": "Player"
  },
  "components": [
    {
      "type_id_crc": 1234567890,
      "type_name": "TransformComponent",
      "fields": [
        { "name": "position", "kind": "vec2", "value": [1.0, 2.0] },
        { "name": "rotation", "kind": "float", "value": 0.5 }
      ]
    }
  ],
  "hierarchy": {
    "parent_index": -1,
    "parent_gen": 0,
    "child_count": 2,
    "children": [
      { "index": 10, "gen": 1 },
      { "index": 11, "gen": 1 }
    ]
  },
  "queries": [
    { "index": 0, "entity_count": 15, "signature": [1234567890, 9876543210] }
  ],
  "mailbox_log": [
    { "frame": 1023, "msg_type_crc": 555, "msg_type_name": "DamageMsg" }
  ]
}
```

### EntityInspectSerializer (game side, in DiaEntityInspector)

```cpp
namespace Dia::EntityInspector {
    // Serializes the current state of a specific entity from a Domain into
    // the entity.inspect JSON payload. Called by the DiaDebugServer topic handler.
    Json::Value SerializeEntityInspect(
        const Dia::Entity::IEntityInspectable& domain,
        Dia::Entity::Entity entity);
}
```

### DiaEntityInspectorPlugin (editor side)

```cpp
namespace Dia::EntityInspector {
    class DiaEntityInspectorPlugin final : public Dia::Editor::IEditorPlugin {
    public:
        const char* GetName() const override;
        const char* GetVersion() const override;
        const char* GetDescription() const override;
        const char* GetUIPath() const override;
        Dia::Editor::LayoutMode GetLayoutMode() const override;
        void OnLoad(Dia::Editor::EditorModel* model) override;
        void OnUnload() override;
        void OnUpdate(float deltaTime) override;
        void* GetPluginData() override;
    private:
        EntityInspectorController  mInspectorController;
        QueryBrowserController     mQueryController;
        MailboxMonitorController   mMailboxController;
        EntityWatchListController  mWatchController;
    };
}
```

## System-Level Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| SED-ENT-001 | `entity.inspect` push is hybrid: immediate on selection change, slow poll every 30 frames | Responsive to selection; keeps channel quiet when nothing changes |
| SED-ENT-002 | Watch list stable reference = entity debug name (string), not entity handle | Handles change across reconnects; names are user-controlled identifiers |
| SED-ENT-003 | `EntityInspectSerializer` is a free function in DiaEntityInspector, not a method on Domain | Domain stays clean; serialization concern belongs in the editor layer |
| SED-ENT-004 | Tier (c) structural edit controls (add/remove component, create/destroy entity) rendered but disabled in v1 | Signals future capability; avoids user confusion about missing controls |
| SED-ENT-005 | `entity.write_field` command is DiaAPI-routed; the server calls `IEntityInspectable::WriteField` | Consistent command routing; no parallel write path |
| SED-ENT-006 | `entity.find_by_name` command returns entity handle (index + gen) for watch list rebind | O(N) scan acceptable for editor use per AI Review Q4 in editor-inspection spec |
| SED-ENT-007 | Mailbox monitor shows last N=64 messages in a ring buffer; no streaming | Bounded memory; enough for debugging without overwhelming the channel |
| SED-ENT-008 | Query browser tab driven by `GetQueryCount` / `GetQuerySignature` / `GetQueryEntityCount` on `IEntityInspectable` | No additional query state needed beyond what IEntityInspectable already exposes |
| SED-ENT-009 | `kProtocolVersion` bumped to 2 in DiaDebugProtocol when `entity.inspect` topic ships | Prevents stale editors from misinterpreting new payloads |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| entity-inspector-panel | Plugin scaffold + entity list + component field accordion + Tier (b) live field edit + `entity.inspect` WebSocket topic + `EntityInspectSerializer` | [entity-inspector-panel.md](entity-inspector-panel.md) | Approved |
| query-browser-tab | Query signatures + result counts + entity membership display in Queries tab | [query-browser-tab.md](query-browser-tab.md) | Approved |
| mailbox-traffic-monitor | Ring-buffer log of dispatched messages, histogram, pause/snapshot in Mailbox tab | [mailbox-traffic-monitor.md](mailbox-traffic-monitor.md) | Approved |
| entity-watch-list | Persistent (entity, component, field) triple watch list; stable re-bind on reconnect | [entity-watch-list.md](entity-watch-list.md) | Approved |
| hierarchy-navigation | Live parent/child hierarchy display with clickable navigation links in Fields tab | — | Done |

## Inherited Binding Decisions

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | All data type constants (`kEntityInspect`, etc.) are `StringCRC`. Component type IDs in the JSON payload carry both CRC value and string name. Compliant. |
| PD-002 | ProcessingUnit/Phase/Module architecture for app structure | DiaEntityInspector is a pure library (`IEditorPlugin` subclass). No Module/Phase/PU. Application flow lives in CluicheEditor. Compliant with SED-015 (DiaEditor library rule). |
| PD-004 | No STL containers in public APIs | `EntityInspectSerializer` returns `Json::Value` (blessed). Controller internals may use STL privately. No STL in the `IEditorPlugin` interface or public headers. Compliant. |
| PD-005 | x64 only | No 32-bit code paths. Compliant. |
| PD-006 | Visual Studio project files are source of truth | New `DiaEntityInspector.vcxproj` follows existing project file conventions. Compliant. |
| PD-007 | C++20 required | All new code compiled under `/std:c++20`. Compliant. |
| PD-008 | `Directory.Build.props` owns output paths | No per-project output overrides. Compliant. |
| ED-REACT | React + Vite + `@dia/editor-ui` required for all `LiveConnectionPluginBase` UIs | This spec compliant — panel is React/Vite. Applies equally to all `LiveConnectionPluginBase` inspector plugins. Disconnected overlay is a React component, not the host frame. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | entity.inspect push rate | Slow poll is "every 30 frames" — at 60 FPS this is 500ms. Is that responsive enough for live field editing? | Acceptable for debugging. Field edit confirmation is immediate (write_field round-trip); the poll is only for background drift detection. |
| 2 | Thread safety of EntityInspectSerializer | `SerializeEntityInspect` calls `IEntityInspectable` methods. If this is called from the DiaDebugServer broadcast handler (network thread), is Domain access thread-safe? | DiaDebugServer calls this from its update tick which runs in the sim thread, not a separate network thread. Caller must ensure this is called from the same thread that owns the Domain. Document as a contract. |
| 3 | Protocol version bump | Bumping `kProtocolVersion` to 2 will reject older editors. How should the server handle version mismatch? | Existing handshake response already carries `accepted: false` with reason. No protocol change needed; just update the constant and document the breaking change. |
| 4 | Watch list persistence | Watch list triples persist "across reconnects." Where are they persisted? | In-memory within the editor session only. Not written to disk in v1. If the editor is closed and reopened, the watch list resets. Acceptable for v1. |

## Status

`Done` — [Plan](diaentityinspector.plan.md)
