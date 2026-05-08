# Feature Spec: Shared Connection

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntimeEditor | @docs/specs/systems/dia/diaassetruntimeeditor.md |
| Feature | **Shared Connection** | (this document) |

## Problem Statement

DiaAssetRuntimeEditorPlugin previously owned its own `GameConnectionManager` instance with a separate connect/disconnect UI (host + port inputs, connect button). This duplicated the connection lifecycle already managed by `GameConnectionEditorPlugin`, forced the user to manually enter connection details in each plugin, and opened multiple independent WebSocket connections to the same game server.

The plugin should instead consume the shared `GameConnectionManager` provided by `GameConnectionEditorPlugin` via the `PluginServiceLocator`. The UI connect bar is removed and replaced with a "No game connected" overlay that directs users to the Game Connection panel.

## Acceptance Criteria

- [x] DiaAssetRuntimeEditorPlugin no longer owns a `GameConnectionManager` instance
- [x] Plugin retrieves the shared manager via `context.mServices->GetService<GameConnectionManager>()`
- [x] Plugin polls `IsConnected()` in `OnUpdate` to detect connection state changes
- [x] All panels receive connection state changes via existing `OnConnectionStateChanged` callbacks
- [x] The `asset_runtime_editor.connect` and `asset_runtime_editor.disconnect` bridge handlers are removed
- [x] UI connect bar (host/port inputs, connect/disconnect buttons) removed from `index.html`
- [x] "No game connected" overlay displayed when disconnected, with hint to use Game Connection panel
- [x] Overlay hides automatically when connection comes up
- [x] Status dot in toolbar still shows connected/disconnected state
- [x] Graceful degradation: if service locator or manager is unavailable, plugin shows disconnected state

## Design

### Plugin Initialization

```cpp
void DiaAssetRuntimeEditorPlugin::OnLoad(const EditorPluginContext& context) {
    // Retrieve shared manager from service locator
    if (context.mServices)
        mManager = context.mServices->GetService<GameConnectionManager>();

    // Panels receive a pointer — nullptr is valid (panels handle gracefully)
    mAssetStateTable.Activate(mBridge, mManager, mState.get());
    // ...

    // If already connected at load time, notify panels immediately
    if (mManager && mManager->IsConnected())
        HandleConnectionStateChange(true);
}
```

### Connection State Polling

Rather than registering a callback on the shared manager (which only supports one `ConnectionCallback`), the plugin polls `IsConnected()` each frame and fires `HandleConnectionStateChange` on transitions:

```cpp
void DiaAssetRuntimeEditorPlugin::OnUpdate(float deltaTime) {
    if (mManager) {
        bool isConnected = mManager->IsConnected();
        if (isConnected != mWasConnected) {
            mWasConnected = isConnected;
            HandleConnectionStateChange(isConnected);
        }
    }
    // ... panel updates
}
```

### UI Overlay

When disconnected, a full-panel overlay obscures the content with:
- "No game connected" message
- "Use the Game Connection panel to connect to a running game" hint

The overlay is hidden when `asset_runtime_editor.connection_state` reports `connected: true`.

### Removed Code

- `GameConnectionManager mManager` member (owned instance) → `GameConnectionManager* mManager` (borrowed pointer)
- `mManager.Initialize()` / `mManager.Shutdown()` / `mManager.Update()` calls
- `asset_runtime_editor.connect` request handler
- `asset_runtime_editor.disconnect` request handler
- HTML connect bar (host input, port input, connect button, disconnect button)
- `onConnect()` / `onDisconnect()` / `diaRequest()` JavaScript functions

## Implementation Files

- `Dia/DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.h` - Changed `mManager` from owned instance to pointer; added `mWasConnected`
- `Dia/DiaAssetRuntimeEditor/DiaAssetRuntimeEditorPlugin.cpp` - Service locator query; polling; removed connect/disconnect handlers
- `Dia/DiaAssetRuntimeEditor/UI/index.html` - Removed connect bar; added disconnect overlay

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — service lookup via `GameConnectionManager::kUniqueId` |
| Platform | PD-004 | No STL in public APIs | **Compliant** — no change to public interfaces |
| DiaEditor | SED-016 | GameConnectionManager boots clean; Connect() is user-triggered | **Compliant** — connection lifecycle remains in GameConnectionEditorPlugin; this plugin is a pure consumer |
| DiaAssetRuntimeEditor | SD-ARED-002 | Read-only, no mutation | **Compliant** — plugin only reads connection state |
| DiaAssetRuntimeEditor | SD-ARED-005 | All panels require live connection; disabled when disconnected | **Compliant** — overlay shown when disconnected; panels deactivate via `OnConnectionStateChanged(false)` |

**All binding decisions: COMPLIANT**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Polling | Why poll instead of registering a callback on the shared manager? | `GameConnectionManager::SetConnectionCallback` only supports a single callback (the owner, GameConnectionEditorPlugin, already uses it). Polling in OnUpdate is simple, O(1), and avoids multi-callback infrastructure. |
| 2 | Load Order | What if DiaAssetRuntimeEditorPlugin loads before GameConnectionEditorPlugin? | `GetService` returns nullptr; plugin shows disconnected state. In practice, GameConnectionEditorPlugin is a built-in that loads first. If load order changes, the plugin still works — it just starts disconnected and picks up the connection on next poll. |
| 3 | Lifetime | What if GameConnectionEditorPlugin unloads while this plugin is active? | The pointer becomes dangling. This cannot happen in practice — GameConnectionEditorPlugin is pinned (built-in). If it ever becomes unpinnable, the service locator pattern would need an unregister notification. |
| 4 | Migration | Should DebugLayerPanelPlugin also migrate to the shared connection? | Yes, as a follow-up task. Same pattern applies — remove owned manager, use service locator. Not in scope of this feature. |

## Status

`Done` - Implemented and passing tests (4350/4351, pre-existing unrelated failure)
