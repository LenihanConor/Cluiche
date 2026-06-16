# LiveConnectionPluginBase

**Parent:** @docs/specs/applications/dia/systems/diaeditor/diaeditor.md
**Status:** Done
**Plan:** @docs/specs/applications/dia/systems/diaeditor/live-connection-plugin-base.plan.md

## Summary

Extract an intermediate base class `LiveConnectionPluginBase` (between `EditorPluginBase` and concrete plugins) that owns the entire game-connection lifecycle: acquiring `GameConnectionManager` from services, polling `IsConnected()` in `OnUpdate`, firing connect/disconnect callbacks, auto-registering a `get_connection_state` handler, and pushing `connectionStatus` to the UI. Three existing plugins duplicate this logic verbatim — this refactor eliminates the copy-paste and enforces consistent behaviour for all future live-connection plugins.

## Goals

- Eliminate duplicated connection-lifecycle boilerplate across plugins
- Enforce a single code path for connection state management — no ad-hoc implementations allowed
- Provide a `RegisterGameTopic` helper that auto-unsubscribes on disconnect
- Normalise the topic naming convention across all live-connection plugins
- Extract a shared JS hook (`useLiveConnection`) for the corresponding UI-side pattern

## Non-Goals

- Owning the connect/disconnect action (the toolbar owns that; base class is passive-observe only)
- Changing the `GameConnectionManager` API itself
- Adding reconnection or retry logic (that's the toolbar's responsibility)

## Acceptance Criteria

1. New `LiveConnectionPluginBase` class in `DiaEditor/Plugin/` deriving from `EditorPluginBase`
2. All 3 existing plugins (DiaEntityInspector, DiaAssetRuntimeInspector, DiaApplicationFlowInspector) refactored to derive from it — no functional change to end-user behaviour
3. Subclasses only implement `OnGameConnected()` / `OnGameDisconnected()` — no raw `mWasConnected` or polling code in subclasses
4. Base class auto-registers a `<prefix>.get_connection_state` request handler
5. Base class auto-pushes `<prefix>.connection_state` notification on transitions (normalised naming)
6. `RegisterGameTopic(StringCRC, DataCallback)` helper: subscribes immediately if connected, auto-unsubscribes on disconnect
7. All existing GoogleTests still pass unchanged
8. JS-side: shared `useLiveConnection(prefix)` hook extracts the mount-time status request + connection_state listener pattern — used by all 3 UIs
9. **Enforcement:** Any new plugin that needs game connection MUST derive from `LiveConnectionPluginBase`. Documented as a constraint in the DiaEditor module doc and the DiaEditor system spec decisions table.

## Design

### C++ Class Hierarchy

```
EditorPluginBase
  └── LiveConnectionPluginBase        ← NEW
        ├── DiaEntityInspectorPlugin
        ├── DiaAssetRuntimeInspectorPlugin
        └── DiaApplicationFlowInspectorPlugin
```

### C++ API

```cpp
class LiveConnectionPluginBase : public EditorPluginBase
{
public:
    explicit LiveConnectionPluginBase(const EditorPluginMetadata& meta, const char* connectionPrefix);

protected:
    // Subclass lifecycle — called by the base after its own setup/teardown
    virtual void OnLivePluginLoad() = 0;
    virtual void OnLivePluginUnload() = 0;

    // Called exactly once per connection state transition
    virtual void OnGameConnected() = 0;
    virtual void OnGameDisconnected() = 0;

    // Per-frame tick for subclass work (panel updates, timers, etc.)
    virtual void OnLiveUpdate(float deltaTime) {}

    // Declarative topic registration. Callable at any time.
    // - If disconnected: buffered, subscribed when connection arrives
    // - If connected: subscribed immediately
    // All registered topics auto-unsubscribe on disconnect.
    void RegisterGameTopic(const Dia::Core::StringCRC& topic,
                           GameConnectionManager::DataCallback callback);

    GameConnectionManager* GetGameConnection() const;
    bool IsGameConnected() const;

private:
    // OnPluginLoad/Unload/OnUpdate are final — subclass uses OnLive* variants instead
    void OnPluginLoad() override final;
    void OnPluginUnload() override final;
    void OnUpdate(float deltaTime) override final;

    GameConnectionManager* mGameConnection = nullptr;
    bool mWasConnected = false;
    const char* mPrefix;  // e.g. "entity_inspector", "asset_runtime_inspector", "app_flow_inspector"

    struct TopicRegistration { Dia::Core::StringCRC topic; GameConnectionManager::DataCallback callback; };
    DynamicArrayC<TopicRegistration, 16> mRegisteredTopics;

    void HandleConnectionStateChange(bool connected);
    void SubscribeAllTopics();
    void UnsubscribeAllTopics();
};
```

### Topic Naming Normalisation

All plugins will use a consistent pattern:
- Status push: `"<prefix>.connection_state"` (e.g. `"app_flow_inspector.connection_state"`)
- Status request: `"<prefix>.get_connection_state"`

The AppFlowInspector's current `"live.connectionStatus"` / `"live.getStatus"` will be normalised to match.

### JS Hook

```typescript
// Shared hook in a common location (e.g. DiaEditor shared UI utils)
function useLiveConnection(prefix: string): {
    connectionState: 'disconnected' | 'connecting' | 'connected';
}
```

On mount: fires `bridgeRequest('<prefix>.get_connection_state')`.
Listens for: `<prefix>.connection_state` via postMessage dispatch.

### Base Class Lifecycle

1. **`OnPluginLoad()` [final]** — acquires `GameConnectionManager` from services, registers `<prefix>.get_connection_state` handler, calls `OnLivePluginLoad()`, then if already connected: subscribes all registered topics + calls `OnGameConnected()`
2. **`OnUpdate()` [final]** — polls `IsConnected()`, fires `HandleConnectionStateChange()` on transition, then calls `OnLiveUpdate(deltaTime)`
3. **`HandleConnectionStateChange(true)`** — subscribes all registered topics, pushes `<prefix>.connection_state`, calls `OnGameConnected()`
4. **`HandleConnectionStateChange(false)`** — unsubscribes all registered topics, pushes `<prefix>.connection_state`, calls `OnGameDisconnected()`
5. **`OnPluginUnload()` [final]** — unsubscribes if connected, calls `OnLivePluginUnload()`, nulls manager

## Binding Decisions

| ID | Decision | Compliance |
|----|----------|-----------|
| SED-001 | Plugin interface is minimal and stable | `LiveConnectionPluginBase` sits below `IEditorPlugin` — no interface change |
| SED-015 | DiaEditor is pure C++ library, no DiaApplicationFlow dependency | Base class lives in DiaEditor, depends only on DiaCore + GameConnectionManager |
| SED-016 | GameConnectionManager boots clean; Connect() is explicit | Base class is passive-observe only — does not call Connect() |
| SED-022 | PluginServiceLocator for inter-plugin service sharing | Base class acquires GameConnectionManager via `GetServices()->GetService<T>()` |

## Design Decisions

1. **Separate virtual (not call-super):** Base class makes `OnPluginLoad`/`OnPluginUnload` `final`, does framework setup, then calls `virtual OnLivePluginLoad() = 0` / `virtual OnLivePluginUnload() = 0`. Matches the `EditorPluginBase` precedent (`OnLoad` → `OnPluginLoad`) — impossible to misuse.

2. **RegisterGameTopic: buffer always, subscribe on connect, immediate if already connected.** Subclass calls `RegisterGameTopic` at any time (typically in `OnLivePluginLoad`). Base class buffers the registration. On connect: subscribes all buffered topics, then calls `OnGameConnected()`. On disconnect: unsubscribes all. If called while already connected: subscribes immediately. Mental model — "register whenever, it'll work."

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `LiveConnectionPluginBase.h/.cpp` in `DiaEditor/Plugin/` | Compiles; unit test for state machine | Not Started | sonnet | |
| 2 | Add to DiaEditor.vcxproj + filters | `dia docs vcxproj-add` | Not Started | haiku | |
| 3 | Refactor DiaEntityInspectorPlugin to derive from new base | Existing tests pass | Not Started | sonnet | |
| 4 | Refactor DiaAssetRuntimeInspectorPlugin to derive from new base | Existing tests pass | Not Started | sonnet | |
| 5 | Refactor DiaApplicationFlowInspectorPlugin to derive from new base | Existing tests pass | Not Started | sonnet | |
| 6 | Normalise AppFlowInspector topic names (`live.*` → `app_flow_inspector.*`) | Inspector UI still works | Not Started | sonnet | |
| 7 | Create shared `useLiveConnection` JS hook | Unit test | Not Started | sonnet | |
| 8 | Refactor 3 UI panels to use `useLiveConnection` | Existing UI tests pass | Not Started | sonnet | |
| 9 | Add SED-023 constraint to DiaEditor system spec | — | Not Started | haiku | |
| 10 | Update DiaEditor module doc with enforcement rule | — | Not Started | haiku | |
