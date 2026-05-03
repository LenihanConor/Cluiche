# Feature Spec: debug-editor-panel

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Adds a `DebugLayerPanelPlugin` to CluicheEditor — a dockable panel that shows all registered debug layers as a checkbox tree, badges the panel icon when primitives are being dropped, and lets the user toggle individual layers. All communication uses the existing `GameConnectionManager` / `DiaDebugProtocol` protobuf-over-WebSocket path that the rest of the editor uses. A new `DebugLayerState` protobuf message type is added to `debug_protocol.proto`; `DebugLayerManager::BroadcastLayerState()` sends it via `DebugServerModule`; the plugin handles it on the editor side and pushes state to the CEF UI via `WebUIBridge::NotifyUIDataChanged`.

**Problem solved:** Developers have no editor-side visibility into which debug layers are active and no warning when the primitive budget overflows. This panel makes the layer stack visible and controllable from CluicheEditor using the same comms infrastructure as every other editor panel.

---

## Acceptance Criteria

1. `DebugLayerPanelPlugin` implements `IEditorPlugin` and lives in `DiaVisualDebugger.vcxproj`
2. `GetUIPath()` returns `"dia://plugins/debug-layers/index.html"` — served from `Dia/DiaEditor/ui/debug-layers/`
3. New `DebugLayerState` message added to `debug_protocol.proto`; `debug_protocol.pb.h` regenerated
4. `DebugLayerManager::BroadcastLayerState(DebugServerModule*)` serialises layer state to `DebugMessage` with `DebugLayerState` payload and calls `DebugServerModule::NotifySubscribers()`
5. On `OnLoad()`: plugin registers a raw message callback on the shared `GameConnectionManager` to handle `kDebugLayerState` messages
6. On receiving `kDebugLayerState`: deserialise payload, call `mBridge->NotifyUIDataChanged("debug.layer.state", data)` to push to CEF front-end
7. Checking/unchecking a layer checkbox in the UI calls `GameConnectionManager::SendCommand("debug.layer.enable", args)` or `"debug.layer.disable"` — which the game dispatches through `Dia::API::CommandRegistry::Execute()`
8. Panel displays an overflow badge when `droppedCount > 0` in the `DebugLayerState` payload; clears when it returns to 0
9. `GetLayoutMode()` returns `LayoutMode::kDockable`
10. Plugin registered with `CluicheEditor`'s plugin registry at startup
11. Build with no warnings; all tests pass (C++ unit tests only — no browser automation)

---

## Protocol Design

### New protobuf message — `debug_protocol.proto`

```protobuf
message DebugLayerEntry {
  string name     = 1;
  bool   enabled  = 2;
  int32  priority = 3;
}

message DebugLayerState {
  repeated DebugLayerEntry layers = 1;
  uint32                   dropped_count = 2;
}
```

Added to `MessageType` enum:
```protobuf
MESSAGE_TYPE_DEBUG_LAYER_STATE = 16;
```

Added to `DebugMessage.payload` oneof:
```protobuf
DebugLayerState debug_layer_state = 25;
```

### Game side — BroadcastLayerState

```cpp
// DebugLayerManager.cpp
void DebugLayerManager::BroadcastLayerState(Dia::Debug::DebugServerModule* server)
{
    if (server == nullptr) return;

    dia::debug::DebugMessage msg;
    msg.set_type(dia::debug::MESSAGE_TYPE_DEBUG_LAYER_STATE);

    auto* layerState = msg.mutable_debug_layer_state();
    layerState->set_dropped_count(mLastDroppedCount);

    for (unsigned int i = 0; i < mLayers.Size(); ++i)
    {
        auto* entry = layerState->add_layers();
        entry->set_name(mLayers[i].debugger->GetLayerName().AsString());
        entry->set_enabled(mLayers[i].enabled);
        entry->set_priority(mLayers[i].priority);
    }

    char jsonBuf[4096];
    if (Dia::Proto::ToJson(msg, jsonBuf, sizeof(jsonBuf)))
        server->NotifySubscribers(Dia::Core::StringCRC("debug.layer.state"), jsonBuf);
}
```

`mLastDroppedCount` is cached at the end of `Draw()` from `frameData.GetDebugFrameData().DroppedCount()`.

### Editor side — DebugLayerPanelPlugin

```cpp
// Dia/DiaVisualDebugger/DebugLayerPanelPlugin.h
namespace Dia::Debug {

class DebugLayerPanelPlugin : public Dia::Editor::IEditorPlugin
{
public:
    const char* GetName()        const override { return "Debug Layers"; }
    const char* GetVersion()     const override { return "1.0"; }
    const char* GetDescription() const override { return "Toggle debug draw layers and monitor primitive budget"; }
    const char* GetUIPath()      const override { return "dia://plugins/debug-layers/index.html"; }
    Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

    Dia::Editor::EditorToolbarItem GetToolbarItem() const override;

    void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
    void OnUnload() override;
    void OnUpdate(float deltaTime) override;  // no-op

private:
    void HandleRawMessage(const char* rawText, unsigned int rawLength,
                          const Json::Value& envelope);
    void PushLayerStateToUI(const dia::debug::DebugLayerState& state);

    Dia::Editor::WebUIBridge*          mBridge  = nullptr;
    Dia::Editor::GameConnectionManager* mManager = nullptr;
};

} // namespace Dia::Debug
```

### OnLoad

```cpp
void DebugLayerPanelPlugin::OnLoad(const EditorPluginContext& context)
{
    mBridge  = context.mBridge;
    mManager = context.GetSharedGameConnectionManager();  // see AI Q1

    if (mManager != nullptr)
    {
        mManager->SetRawMessageCallback(
            [this](const char* raw, unsigned int len, const Json::Value& envelope) {
                HandleRawMessage(raw, len, envelope);
            });
    }
}
```

### HandleRawMessage

```cpp
void DebugLayerPanelPlugin::HandleRawMessage(const char* rawText,
                                              unsigned int /*len*/,
                                              const Json::Value& /*envelope*/)
{
    dia::debug::DebugMessage msg;
    if (!Dia::Proto::FromJson(rawText, &msg)) return;
    if (msg.payload_case() != dia::debug::DebugMessage::kDebugLayerState) return;

    PushLayerStateToUI(msg.debug_layer_state());
}
```

### PushLayerStateToUI

```cpp
void DebugLayerPanelPlugin::PushLayerStateToUI(const dia::debug::DebugLayerState& state)
{
    Json::Value data;
    data["droppedCount"] = state.dropped_count();

    Json::Value layers(Json::arrayValue);
    for (int i = 0; i < state.layers_size(); ++i)
    {
        const auto& entry = state.layers(i);
        Json::Value layer;
        layer["name"]     = entry.name();
        layer["enabled"]  = entry.enabled();
        layer["priority"] = entry.priority();
        layers.append(layer);
    }
    data["layers"] = layers;

    mBridge->NotifyUIDataChanged("debug.layer.state", data);
}
```

### Toggle commands from UI

The JavaScript front-end calls:
```js
// On checkbox toggle:
gameConnection.sendCommand("debug.layer.enable",  { name: "physics.shapes" });
// or
gameConnection.sendCommand("debug.layer.disable", { name: "physics.shapes" });
```

These reach the game via `MESSAGE_TYPE_COMMAND_REQUEST`. The game's `CommandRegistry::Execute("debug.layer.enable physics.shapes")` routes to the callback registered by `DebugLayerManager::RegisterDiaAPICommands()`.

### Game side registration (game code)

```cpp
// Each frame, after PhysicsWorld::Step() and all sim updates:
manager.Draw(frameData);
manager.BroadcastLayerState(debugServer);  // explicit — game code owns both
```

---

## UI behaviour (CEF front-end — `DiaEditor/ui/debug-layers/`)

- Subscribes to `"debug.layer.state"` topic via the existing `window.DiaEditor_onDataChanged` JavaScript event
- Renders a checkbox list sorted by priority, then alphabetically within priority tier
- Toggle fires `sendCommand("debug.layer.enable"/"debug.layer.disable", { name })` back via the existing JS game connection channel
- Overflow badge: when `droppedCount > 0` the panel toolbar icon shows a red dot with the count

Follows the same JS pattern as the `core_metrics` panel which uses `NotifyUIDataChanged`.

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaDebugProtocol/proto/debug_protocol.proto` | Add `DebugLayerEntry`, `DebugLayerState` messages; `MESSAGE_TYPE_DEBUG_LAYER_STATE = 16`; add to `DebugMessage` oneof |
| `Dia/DiaDebugProtocol/generated/debug_protocol.pb.h/.cc` | Regenerated from proto |
| `Dia/DiaVisualDebugger/DebugLayerPanelPlugin.h` | New |
| `Dia/DiaVisualDebugger/DebugLayerPanelPlugin.cpp` | New |
| `Dia/DiaVisualDebugger/DebugLayerManager.h` | Add `BroadcastLayerState(DebugServerModule*)` declaration; add `mLastDroppedCount` member |
| `Dia/DiaVisualDebugger/DebugLayerManager.cpp` | Implement `BroadcastLayerState()`; cache `mLastDroppedCount` in `Draw()` |
| `Dia/DiaVisualDebugger/DiaVisualDebugger.vcxproj` | Add `DebugLayerPanelPlugin.h/.cpp`; add `DiaDebugProtocol` project reference |
| `Dia/DiaVisualDebugger/DiaVisualDebugger.vcxproj.filters` | Update filters |
| `Dia/DiaEditor/ui/debug-layers/index.html` | New — CEF panel UI |
| `Dia/DiaEditor/ui/debug-layers/debug-layers.js` | New — checkbox list + `DiaEditor_onDataChanged` handler |
| `CluicheEditor` startup | Register `DebugLayerPanelPlugin` |

**Dependencies added to DiaVisualDebugger.vcxproj:** `DiaDebugProtocol.lib`, `DiaEditor.lib` (for `IEditorPlugin`, `WebUIBridge`)

**Prerequisites:** `debug-layer-manager` implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `DebugLayerEntry` + `DebugLayerState` to `debug_protocol.proto`; regenerate `.pb.h/.cc` | New message type 16 |
| 2 | Implement `BroadcastLayerState(DebugServerModule*)` in `DebugLayerManager.cpp` | Cache `mLastDroppedCount` in `Draw()`; serialise via `Dia::Proto::ToJson` |
| 3 | Write `DebugLayerPanelPlugin.h/.cpp` — `OnLoad` registers raw callback; `HandleRawMessage` dispatches `kDebugLayerState` | |
| 4 | Update `DiaVisualDebugger.vcxproj` — add plugin files + proto reference | |
| 5 | Write `DiaEditor/ui/debug-layers/index.html` + `debug-layers.js` | Same JS event pattern as `core_metrics` |
| 6 | Register plugin in `CluicheEditor` startup | Follow `GameConnectionEditorPlugin` registration pattern |
| 7 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 8 | Write tests | `TestDebugLayerPanelPlugin.cpp` |
| 9 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaVisualDebugger/TestDebugLayerPanelPlugin.cpp`

Tests cover C++ plugin class and `BroadcastLayerState` serialisation only — no browser automation.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| Plugin metadata | `GetName_ReturnsDebugLayers` | `GetName() == "Debug Layers"` |
| Plugin metadata | `GetUIPath_ReturnsCorrectPath` | `GetUIPath() == "dia://plugins/debug-layers/index.html"` |
| Plugin metadata | `GetLayoutMode_IsDockable` | `GetLayoutMode() == LayoutMode::kDockable` |
| BroadcastLayerState | `Serialises_LayerName` | After `Register()`, proto payload contains layer name |
| BroadcastLayerState | `Serialises_EnabledFlag` | Disabled layer → `enabled == false` in proto |
| BroadcastLayerState | `Serialises_Priority` | Priority value round-trips through proto |
| BroadcastLayerState | `Serialises_DroppedCount` | Overflow frame → `dropped_count > 0` in proto |
| BroadcastLayerState | `NullServer_NoOp` | `BroadcastLayerState(nullptr)` → no crash |
| HandleRawMessage | `IgnoresNonLayerStateMessages` | `kCoreMetrics` message → `PushLayerStateToUI` not called |
| HandleRawMessage | `HandlesLayerStateMessage` | Valid `kDebugLayerState` → `NotifyUIDataChanged` called with `"debug.layer.state"` |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — `StringCRC` used for layer names; proto stores `string` representation |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — plugin lifecycle via `IEditorPlugin` |
| PD-003 | Component-based entities | Compliant — no entity concerns |
| PD-004 | No STL in public APIs | Compliant — `IEditorPlugin` uses `const char*`; plugin public API has no STL |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — `DiaVisualDebugger.vcxproj` updated |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — proto generated files are source-controlled, not built output |
| AD-001 | Module YAML frontmatter | Compliant — `dia.debug.architecture.module.md` updated |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — `Dia::Debug::DebugLayerPanelPlugin` |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — plugin only registered in Debug builds |
| SD-DBG-008 | `debug.pick` no-op stub | Compliant — panel does not add pick functionality |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Shared `GameConnectionManager` | `DebugLayerPanelPlugin::OnLoad` needs to access the shared `GameConnectionManager` that `GameConnectionEditorPlugin` owns. Does `EditorPluginContext` expose it, or does the plugin registry have a lookup? | Must be resolved at implementation time. Options: (a) `EditorPluginContext` gains a `GetSharedManager()` accessor; (b) a plugin can query the registry for another plugin by name and cast. Follow whichever pattern `GameConnectionEditorPlugin` itself uses for sharing state between plugins. |
| 2 | Multiple raw message callbacks | `GameConnectionManager::SetRawMessageCallback()` takes a single callback. If `DebugLayerPanelPlugin` sets it, it may displace the callback that `GameConnectionController` already set. Should `GameConnectionManager` support multiple raw callbacks? | Either add `AddRawMessageCallback()` (supports N callbacks, stored in `DynamicArrayC`) or use the topic-envelope path instead — `BroadcastLayerState()` sends `{ "topic": "debug.layer.state", "data": { ... } }` which routes via `Subscribe()` without touching the raw callback. Recommend the topic-envelope path as the lower-risk option; reserve the raw callback for proto messages that have no topic envelope. |
| 3 | Proto regeneration workflow | `debug_protocol.proto` is compiled to `.pb.h/.cc` — where does this happen and who runs it? | Check existing proto generation in `DiaDebugProtocol/`. Likely a pre-build step or a manual script. Task 1 must follow the same process as any prior proto additions (e.g. the existing `LogEntry` / `CoreMetrics` additions). |
| 4 | `BroadcastLayerState` call frequency | The spec says "called after each `Draw()`" — potentially 60 Hz. Is broadcasting the full layer list at 60 Hz appropriate? | Layer state rarely changes; broadcasting at 60 Hz is wasteful but harmless (the list is small). For correctness, broadcast only when `mLayersDirty` is true (set on `Register`, `Enable`, `Disable`, `UnregisterLayer`). Clear the flag after broadcast. This reduces traffic to zero when nothing changes. |
| 5 | `DebugLayerManager` dependency on `DiaDebugProtocol` | `BroadcastLayerState` uses `dia::debug::DebugMessage` — this adds `DiaDebugProtocol` as a dependency of `DiaVisualDebugger.vcxproj`. Is this acceptable? | Acceptable — `DiaVisualDebugger` already depends on `DiaDebugServer` for broadcasting (from the system spec). `DiaDebugProtocol` is the wire format for that server. The dependency chain `DiaVisualDebugger → DiaDebugServer → DiaDebugProtocol` is directional and acyclic. |

---

## Status

`Approved`
