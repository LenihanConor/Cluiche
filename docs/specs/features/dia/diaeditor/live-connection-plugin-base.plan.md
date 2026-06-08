# LiveConnectionPluginBase — Implementation Plan

**Spec:** @docs/specs/features/dia/diaeditor/live-connection-plugin-base.md
**Status:** Not Started

## Implementation Patterns

### Phase 1: Base Class (Tasks 1–2)

**File:** `Dia/DiaEditor/Plugin/LiveConnectionPluginBase.h/.cpp`

**Class shape:**
```cpp
class LiveConnectionPluginBase : public EditorPluginBase
{
public:
    explicit LiveConnectionPluginBase(const EditorPluginMetadata& meta, const char* connectionPrefix);

protected:
    virtual void OnLivePluginLoad() = 0;
    virtual void OnLivePluginUnload() = 0;
    virtual void OnGameConnected() = 0;
    virtual void OnGameDisconnected() = 0;

    void RegisterGameTopic(const Dia::Core::StringCRC& topic,
                           GameConnectionManager::DataCallback callback);
    GameConnectionManager* GetGameConnection() const;
    bool IsGameConnected() const;

private:
    void OnPluginLoad() override final;
    void OnPluginUnload() override final;
    void OnUpdate(float deltaTime) override;

    void HandleConnectionStateChange(bool connected);
    void SubscribeAllTopics();
    void UnsubscribeAllTopics();

    GameConnectionManager* mGameConnection = nullptr;
    bool mWasConnected = false;
    const char* mPrefix;

    struct TopicRegistration {
        Dia::Core::StringCRC topic;
        GameConnectionManager::DataCallback callback;
    };
    static const unsigned int kMaxTopics = 16;
    DynamicArrayC<TopicRegistration, kMaxTopics> mRegisteredTopics;
};
```

**OnPluginLoad sequence:**
1. Acquire `GameConnectionManager` via `GetServices()->GetService<>()`
2. Register `"<prefix>.get_connection_state"` handler (returns `{connected: bool}`)
3. Call `OnLivePluginLoad()` — subclass registers topics + does own init
4. If `mGameConnection->IsConnected()`: set `mWasConnected = true`, call `SubscribeAllTopics()`, push `connection_state`, call `OnGameConnected()`

**OnUpdate sequence:**
1. Poll `mGameConnection->IsConnected()`
2. If changed from `mWasConnected`: call `HandleConnectionStateChange()`

**HandleConnectionStateChange(true):**
1. `SubscribeAllTopics()` — iterate `mRegisteredTopics`, call `mGameConnection->Subscribe()` for each
2. Push `"<prefix>.connection_state"` with `{connected: true}` to bridge
3. Call `OnGameConnected()`

**HandleConnectionStateChange(false):**
1. `UnsubscribeAllTopics()` — iterate `mRegisteredTopics`, call `mGameConnection->Unsubscribe()` for each
2. Push `"<prefix>.connection_state"` with `{connected: false}` to bridge
3. Call `OnGameDisconnected()`

**RegisterGameTopic:**
1. Append to `mRegisteredTopics`
2. If `mWasConnected`: subscribe immediately via `mGameConnection->Subscribe()`

**OnPluginUnload:**
1. If connected: `UnsubscribeAllTopics()`
2. Call `OnLivePluginUnload()`
3. Null `mGameConnection`, reset `mWasConnected`

### Phase 2: Refactor Existing Plugins (Tasks 3–5)

**Pattern per plugin:**
- Change base class: `EditorPluginBase` → `LiveConnectionPluginBase`
- Constructor: pass `connectionPrefix` (e.g. `"entity_inspector"`)
- Rename `OnPluginLoad()` → `OnLivePluginLoad()`; remove: manager acquisition, `get_connection_state` handler, `mWasConnected` field, `IsConnected()` poll in `OnUpdate`
- Rename `OnPluginUnload()` → `OnLivePluginUnload()`; remove: unsubscribe-on-teardown code
- Rename `HandleConnectionStateChange()` → split into `OnGameConnected()` / `OnGameDisconnected()`
- Replace direct `mGameConnection->Subscribe()` with `RegisterGameTopic()` calls in `OnLivePluginLoad()`
- Remove `mManager`/`mGameConnection` pointer — use `GetGameConnection()` for `SendCommand`/`SendCommandWithResponse`

**DiaEntityInspectorPlugin specifics:**
- Topics: `kEntityInspect` — registered in `OnLivePluginLoad()`
- `OnGameConnected()`: activates controllers
- `OnGameDisconnected()`: deactivates controllers
- Still overrides `OnUpdate` for its own per-frame work (watch pushes) — call `LiveConnectionPluginBase::OnUpdate(deltaTime)` first... wait, `OnUpdate` is not final. Actually looking again at the spec — `OnUpdate` needs to remain overridable since AssetRuntimeInspector has panel update ticks. Make `OnUpdate` non-final but call base in subclasses? No — that's the call-super problem we rejected. Better: make `OnUpdate` final in base, add `virtual void OnLiveUpdate(float dt) {}` that subclasses override.

**Correction to spec:** `OnUpdate` should also use the narrowing-virtual pattern:
- `OnUpdate` is `final` in `LiveConnectionPluginBase`
- Base does: poll connection, then call `virtual void OnLiveUpdate(float deltaTime) {}`
- Subclasses override `OnLiveUpdate` for panel ticks, etc.

**DiaAssetRuntimeInspectorPlugin specifics:**
- No direct topic subscriptions at plugin level — panels subscribe via their own `mManager` pointer passed in `Activate()`. This is the tricky one: panels currently receive `GameConnectionManager*` directly. After refactor, pass `GetGameConnection()` result to panel `Activate()` calls in `OnLivePluginLoad()`. Panels still manage their own subscriptions internally (they're not going through `RegisterGameTopic`). That's acceptable — the base class handles the lifecycle, panels handle their domain subscriptions within the connected window.
- `OnGameConnected()`: calls `panel.OnConnectionStateChanged(true)` on all 4 panels
- `OnGameDisconnected()`: calls `panel.OnConnectionStateChanged(false)` on all 4 panels
- `OnLiveUpdate()`: calls `panel.Update(dt)` on all 4 panels

**DiaApplicationFlowInspectorPlugin specifics:**
- Topics: `kTopicAppState`, `kTopicAppModules`, `kTopicAppStreams`, `kTopicAppTimings`, `kTopicAppEvent` — all registered in `OnLivePluginLoad()`
- `HandleLiveConnect()` handler remains but becomes simpler — just calls `GetGameConnection()->Connect(host, port)` and sets the callback. The callback now only needs to sync `mWasConnected` (or can be removed entirely if we rely on `OnUpdate` polling).
- Actually, with the base class polling every frame, the `SetConnectionCallback` in `HandleLiveConnect` is redundant. The base's `OnUpdate` will detect the state change. Remove the callback entirely — just call `Connect()`.

### Phase 3: Topic Normalisation (Task 6)

**AppFlowInspector rename:**
- C++: `connectionPrefix` = `"app_flow_inspector"` → topics become `"app_flow_inspector.connection_state"`, `"app_flow_inspector.get_connection_state"`
- JS (`AppInspector.tsx`): update `bridgeRequest('live.getStatus')` → `bridgeRequest('app_flow_inspector.get_connection_state')`
- JS: update `'live.connectionStatus'` listener → `'app_flow_inspector.connection_state'`

### Phase 4: Shared JS Hook (Tasks 7–8)

**Location decision:** No shared UI package exists. Options:
- A) Create `Dia/DiaEditorUI/` as a shared npm workspace package — overkill for one hook
- B) Place in each plugin's `src/` as a copy — defeats the purpose
- C) Create a minimal `Dia/DiaEditor/UI/shared/` directory with the hook as a buildable package that plugins reference via relative path workspace dep

**Recommendation: C** — lightweight, single source of truth, no publish step.

**File:** `Dia/DiaEditor/UI/shared/useLiveConnection.ts`

```typescript
import { useState, useEffect } from 'react';

export type LiveConnectionState = 'disconnected' | 'connecting' | 'connected';

export function useLiveConnection(
    prefix: string,
    bridgeRequest: (type: string, data?: object) => Promise<unknown>,
): LiveConnectionState {
    const [state, setState] = useState<LiveConnectionState>('disconnected');

    // Request current state on mount
    useEffect(() => {
        bridgeRequest(`${prefix}.get_connection_state`, {}).then((result: unknown) => {
            const r = result as { connected?: boolean } | null;
            if (r?.connected === true) setState('connected');
        });
    }, [prefix, bridgeRequest]);

    // Listen for state pushes
    useEffect(() => {
        const handler = (topic: string, data: unknown) => {
            if (topic === `${prefix}.connection_state`) {
                const d = data as { connected?: boolean } | null;
                setState(d?.connected === true ? 'connected' : 'disconnected');
            }
        };
        // Register with the plugin's dispatch mechanism
        (window as any).__diaLiveConnectionListeners =
            (window as any).__diaLiveConnectionListeners || [];
        (window as any).__diaLiveConnectionListeners.push(handler);
        return () => {
            const list = (window as any).__diaLiveConnectionListeners;
            const idx = list.indexOf(handler);
            if (idx >= 0) list.splice(idx, 1);
        };
    }, [prefix]);

    return state;
}
```

**Note:** The exact dispatch mechanism depends on how each plugin's bridge works. The AppFlowInspector uses `postMessage` + `DiaEditor_onDataChanged`; the EntityInspector uses the same. The hook should integrate with whichever dispatch the hosting panel uses — likely by accepting a subscribe/unsubscribe function rather than hard-coding `window.__diaLiveConnectionListeners`. Refine during implementation.

### Phase 5: Documentation (Tasks 9–10)

- Add `SED-023` to `docs/specs/systems/dia/diaeditor.md` decision table: "Plugins requiring game connection must derive from LiveConnectionPluginBase"
- Update `Dia/DiaEditor/dia.editor.architecture.module.md` to list `LiveConnectionPluginBase` and the enforcement rule

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `LiveConnectionPluginBase.h/.cpp` | Compiles; GoogleTest for state machine (connect/disconnect/reconnect transitions, RegisterGameTopic buffering + immediate subscribe) | Not Started | sonnet | Core deliverable |
| 2 | Add to DiaEditor.vcxproj + filters | `dia docs vcxproj-add DiaEditor "Plugin\\LiveConnectionPluginBase.h" --filter Plugin` + `.cpp` | Not Started | haiku | |
| 3 | Refactor DiaEntityInspectorPlugin → LiveConnectionPluginBase | All existing EntityInspector GoogleTests pass; connection works in editor | Not Started | sonnet | Simplest plugin — good first validation |
| 4 | Refactor DiaAssetRuntimeInspectorPlugin → LiveConnectionPluginBase | All existing tests pass; panels still receive connection state | Not Started | sonnet | Panels keep their own subscriptions; base handles lifecycle only |
| 5 | Refactor DiaApplicationFlowInspectorPlugin → LiveConnectionPluginBase | All existing tests pass; remove SetConnectionCallback, rely on OnUpdate polling | Not Started | sonnet | Remove HandleLiveConnect's callback; base handles detection |
| 6 | Normalise AppFlowInspector topic names: `live.*` → `app_flow_inspector.*` | JS tests updated; C++ uses `connectionPrefix` from constructor | Not Started | sonnet | Breaking change to JS — do with task 5 |
| 7 | Create shared `useLiveConnection` hook + unit test | Hook returns correct state for mount-request and push scenarios | Not Started | sonnet | Location: `Dia/DiaEditor/UI/shared/` or inline in each plugin if workspace setup is too heavy |
| 8 | Refactor 3 UI panels to use `useLiveConnection` | Existing UI tests pass; remove duplicated bridge request + listener code | Not Started | sonnet | EntityInspector is vanilla JS (index.html) — hook only applies to React plugins (AppFlowInspector). EntityInspector stays as-is. |
| 9 | Add SED-023 to DiaEditor system spec decisions table | — | Not Started | haiku | "Plugins requiring game connection MUST derive from LiveConnectionPluginBase" |
| 10 | Update DiaEditor module doc with enforcement rule | — | Not Started | haiku | |

## Dependencies

```
1 → 2 (need vcxproj before compiling)
2 → 3, 4, 5 (base class must compile before refactoring consumers)
3, 4, 5 are independent of each other (parallel-safe — different plugins, different vcxprojs)
5 → 6 (normalise topics as part of AppFlowInspector refactor)
7 is independent of 1–6 (JS work)
7 → 8 (hook must exist before refactoring UIs)
9, 10 are independent (docs)
```

## Risks

- **AssetRuntimeInspector panels manage their own subscriptions:** The base class handles plugin-level lifecycle, but panels call `mManager->Subscribe()` directly in their `Activate()`. This is intentional — panels have their own poll intervals and topic sets. The base just ensures the manager pointer is valid during the connected window.
- **EntityInspector UI is vanilla JS (not React):** The `useLiveConnection` hook only applies to React-based plugins. EntityInspector's `index.html` already has its own inline connection handling that works. Leave it as-is — the C++ side gets the benefit; the JS side is a React-only improvement.
- **OnUpdate override for subclass tick work:** Added `OnLiveUpdate(float)` virtual to avoid the call-super problem. Must update the spec to reflect this addition.
