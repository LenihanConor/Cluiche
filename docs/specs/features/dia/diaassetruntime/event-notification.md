# Feature Spec: Event Notification

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |
| Feature | Event Notification | (this document) |

## Summary

`IAssetStateListener` is the callback interface that consuming systems (DiaGraphics, DiaAudio, etc.) implement to respond to asset state transitions. `RegisterListener` and `UnregisterListener` manage the listener list. `OnAssetReady` is dispatched when an asset transitions from `Registered` to `Staged` (load requested, path resolved). `OnAssetUnloading` is dispatched when an asset transitions from `Loaded` or `Staged` to `Unloading` (ref count dropped to zero, content should be released).

Listeners are called synchronously on the calling thread. Unregistering during dispatch is safe via deferred removal.

## Problem

Without event notification, consuming systems would need to poll DiaAssetRuntime to detect state changes. This is both wasteful and error-prone -- a system might miss a transition between polls, or react to a stale state. A listener-based push model ensures consumers are notified exactly when they need to act, with the resolved path readily available.

## Acceptance Criteria

1. `IAssetStateListener` is an abstract interface with three virtual methods: `OnAssetReady(assetId, resolvedPath)`, `OnAssetUnloading(assetId)`, and `OnAssetLoadFailed(assetId)` (dispatched when a consumer calls `AcknowledgeAssetLoadFailed` while the asset is in Staged state)
2. `RegisterListener(IAssetStateListener*)` adds a listener to the notification list
3. `UnregisterListener(IAssetStateListener*)` removes a listener from the notification list
4. `OnAssetReady` is dispatched when an asset transitions `Registered->Staged` (during `RequestStageLoad`)
5. `OnAssetReady` provides both the asset ID and the resolved absolute `FilePath` so the consumer can immediately begin loading content
6. `OnAssetUnloading` is dispatched when an asset transitions to `Unloading` (during `RequestStageUnload`, when ref count drops to 0)
7. Listeners are called synchronously on the thread that calls `RequestStageLoad`/`RequestStageUnload`
8. Multiple listeners can be registered simultaneously; all are notified for each event
9. Unregistering a listener during a dispatch callback is safe -- removal is deferred until the current dispatch completes
10. Registering the same listener pointer twice is a no-op with a DiaLogger warning
11. The listener list is a `DynamicArrayC` with a fixed capacity; exceeding capacity logs a warning and the registration fails
12. Listener ordering matches registration order (first registered, first notified)
13. Late-joining listeners are NOT replayed missed events. Systems that register after a `RequestStageLoad` must call `GetStagedAssets()` (Feature 5) to discover assets already in `Staged` state and self-serve. This is the documented pattern for late registration.
14. Listener registration should happen before any `RequestStageLoad` call. Late registration is supported but the caller is responsible for catching up via the query API.
15. Acknowledgement is single-caller — one `AcknowledgeAssetLoaded` call per asset advances state. In the current architecture each asset type is consumed by exactly one system (1:1). Per-listener acknowledgement is a documented future path if multi-consumer assets arise (would add listener ID to the ack call).

## API Design

### Core Types

```cpp
namespace Dia::AssetRuntime
{
    class IAssetStateListener
    {
    public:
        virtual ~IAssetStateListener() = default;
        virtual void OnAssetReady(const Dia::Core::StringCRC& assetId,
                                  const Dia::Core::Containers::String512& resolvedPath) = 0;
        virtual void OnAssetUnloading(const Dia::Core::StringCRC& assetId) = 0;
        virtual void OnAssetLoadFailed(const Dia::Core::StringCRC& assetId) = 0;
    };
}
```

### API on AssetRuntime

```cpp
namespace Dia::AssetRuntime
{
    class AssetRuntime
    {
    public:
        void RegisterListener(IAssetStateListener* listener);
        void UnregisterListener(IAssetStateListener* listener);
    };
}
```

### Usage Pattern

```cpp
class MyGraphicsSystem : public Dia::AssetRuntime::IAssetStateListener
{
public:
    void OnAssetReady(const StringCRC& assetId, const FilePath& path) override
    {
        if (IsTextureAsset(assetId))
            mTextureManager.BeginLoad(assetId, path);
    }

    void OnAssetUnloading(const StringCRC& assetId) override
    {
        if (IsTextureAsset(assetId))
        {
            mTextureManager.Release(assetId);
            mRuntime->AcknowledgeAssetUnloaded(assetId);
        }
    }
};

// Registration
runtime.RegisterListener(&myGraphicsSystem);

// Later -- triggers OnAssetReady for all newly-staged assets
runtime.RequestStageLoad(StringCRC("stage.gameplay"));

// Cleanup
runtime.UnregisterListener(&myGraphicsSystem);
```

### Deferred Removal Design

```cpp
// Internal: during dispatch, removal requests are queued
// After dispatch loop completes, deferred removals are applied
// This prevents iterator invalidation when a listener unregisters itself
// or another listener during a callback
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement IAssetStateListener interface | Abstract class with virtual destructor, OnAssetReady, OnAssetUnloading. Header-only. |
| 2 | Implement listener registration and unregistration | DynamicArrayC<IAssetStateListener*, 16> for listener storage. RegisterListener adds (rejects duplicates). UnregisterListener removes (or defers if dispatching). |
| 3 | Implement dispatch from state machine transitions | Wire into Feature 2's state transitions: dispatch OnAssetReady on Registered->Staged, dispatch OnAssetUnloading on transition to Unloading. |
| 4 | Implement safe removal during dispatch | Boolean flag `mIsDispatching`. During dispatch, removal requests are added to a pending-removal list. After dispatch loop, apply deferred removals. |
| 5 | Add GoogleTest coverage | Tests for: single listener receives OnAssetReady, single listener receives OnAssetUnloading, multiple listeners notified in order, unregister during dispatch, duplicate registration rejected, resolved path passed correctly, no events for already-loaded assets on re-stage |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetRuntime -- Manifest Load & Path Resolution (Feature 1) | Resolved FilePath passed to OnAssetReady |
| DiaAssetRuntime -- Asset State Machine (Feature 2) | State transitions that trigger event dispatch |
| DiaAssetRuntime -- Stage Lifecycle & Reference Counting (Feature 3) | RequestStageLoad/Unload drive state transitions that fire events |
| DiaCore CRC/StringCRC | Asset IDs in listener callbacks |
| DiaCore FilePath | Resolved path in OnAssetReady |
| DiaCore Containers | DynamicArrayC for listener list and deferred removal list |
| DiaLogger | Warnings for duplicate registration, capacity exceeded |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntime/IAssetStateListener.h` | Create -- listener interface |
| `Dia/DiaAssetRuntime/AssetRuntime.h` | Modify -- add RegisterListener, UnregisterListener, listener storage, dispatch flag |
| `Dia/DiaAssetRuntime/AssetRuntime.cpp` | Modify -- implement listener management and event dispatch |
| `Dia/DiaAssetRuntime/DiaAssetRuntime.vcxproj` | Modify -- add IAssetStateListener.h |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** OnAssetReady and OnAssetUnloading pass StringCRC asset IDs. |
| PD-004 | No STL in public APIs | **Compliant.** IAssetStateListener uses StringCRC and FilePath parameters. Listener list is DynamicArrayC. No std::function, std::vector, or std::string. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj build setting overrides. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** No generated output. |
| AD-001 | YAML frontmatter module docs | **Compliant.** Module doc created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::AssetRuntime:: | **Compliant.** All code under `Dia::AssetRuntime::`. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** Listener callbacks receive type.name StringCRC IDs. |
| SD-ARUN-001 | No DiaApplicationFlow dependency | **Compliant.** Listeners are raw interface pointers registered by game code. No ProcessingUnit or Module awareness. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | **Compliant.** No imports from DiaAssetCatalogue. Events carry only runtime data (asset ID, deploy path). |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Listeners are called synchronously. What if a listener blocks for a long time (e.g., synchronous file I/O)? | That is the listener's problem. DiaAssetRuntime is a library with no thread model (SD-ARUN-001). If the game needs async loading, the listener should queue work and return immediately. The Module wrapper above DiaAssetRuntime can enforce this. |
| 2 | Dispatch | Should OnAssetReady fire on Registered->Staged (load requested) or Staged->Loaded (load confirmed)? | On Registered->Staged. This is when the consumer should begin loading content. Staged->Loaded is the consumer's acknowledgement that it finished loading -- firing an event there would be circular. |
| 3 | Capacity | What is a reasonable fixed capacity for the listener list? | 16 listeners. A game typically has a handful of content-loading systems (graphics, audio, physics, UI). 16 is generous. If a project needs more, the template parameter can be increased. |
| 4 | Safety | What if a listener calls RequestStageLoad or RequestStageUnload from within a callback? | This is re-entrant dispatch. The current implementation dispatches synchronously, so nested RequestStageLoad would dispatch more events to all listeners mid-callback. This is acceptable for now -- the alternative (queuing transitions) adds complexity. Document that listeners should avoid triggering stage transitions from callbacks. |
| 5 | Lifecycle | Who owns the listener pointer? Can DiaAssetRuntime call delete on it? | No. DiaAssetRuntime stores raw pointers. The caller owns the listener's lifetime and must call UnregisterListener before destroying the listener. Virtual destructor on IAssetStateListener is for correct cleanup by the owning code, not by DiaAssetRuntime. |
| 6 | Edge case | What if all listeners are unregistered and a stage transition fires events? | No-op. The dispatch loop iterates an empty list. No error, no warning. This is valid -- a runtime with no listeners is legal (e.g., headless server that only needs path resolution). |
| 7 | Late join | What if a system registers after a Stage is already loaded? | Late-joining listeners are not replayed missed events. The system must call `GetStagedAssets()` (Feature 5) to discover assets in `Staged` state, load them, and call `AcknowledgeAssetLoaded`. This keeps the event system simple (fire-and-forget). In practice, all listeners register at startup before any `RequestStageLoad`. |
| 8 | Multi-consumer | What if two systems both need the same asset (e.g. a config used by gameplay and UI)? | Current design: single `AcknowledgeAssetLoaded` call advances state. The first system to ack wins. This works because each asset type is consumed by exactly one system today (1:1). If multi-consumer assets become a real use case, per-listener acknowledgement (listener ID in the ack call) is the documented migration path — the API signature is forward-compatible. |

## Status

`Approved`
