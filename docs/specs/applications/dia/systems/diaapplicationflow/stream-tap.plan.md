# Implementation Plan: Stream Tap & Debug Iteration (remaining work)

**Spec:** [stream-tap.md](stream-tap.md)
**System Spec:** [diaapplicationflow.md](diaapplicationflow.md)
**Created:** 2026-05-18

---

## Session Notes

### What's already done
- `AttachTap` / `DetachTap` / `GetTapCount` on `IStreamStore` (default no-ops) and `EventStreamStore<T>` (full implementation with mutex, `mTaps` array, `mNextTapId`, re-entrance guard via `mInSend`).
- `TestStreamTap.cpp` — 10 tests covering AC10 (callback fires on Send), AC11 (tap count increments/decrements), and the IStreamStore virtual dispatch path.

### What remains
- **SerializeToJson / DIA_STREAM_TYPE_WITH_SERIALIZER** — `StreamTypeRegistry` has no serializer map; `SerializeToJson` and `RegisterSerializer` don't exist yet. `DIA_STREAM_TYPE_WITH_SERIALIZER` macro is not in `RegistrationMacrosV2.h`. (AC6, AC7)
- **`StreamInfo` extension** — `IApplicationInspectable::StreamInfo` struct is missing: `kind`, `payloadType`, `overflowPolicy`, `currentSequence`, `attachedReaderCount`, `attachedTapCount`. `Application::GetStreamInfo` needs to populate them. (AC5)
- **`IApplicationInspectable::FindStream`** — `Application::FindStreamStore` exists but is not on the inspectable interface. DebugServer needs this to attach taps at runtime.
- **DebugServer migration** — `SubscriptionManager` + `BroadcastStageTransition` still live; `HandleSubscribe`/`HandleUnsubscribe`/`HandleConnectionClosed` still route through it; `BroadcastStageTransition` still polls via `Tick`. All must be replaced with `AttachTap`/`DetachTap` on `Application`'s stores. (AC8, AC9)
- **`DebugLayerManager::NotifySubscribers` migration** — only external `NotifySubscribers` caller; needs to push via a declared `debug.layer.state` EventStream + tap instead. (AC8 cleanup)
- **`TestDebugServerTaps.cpp`** — new tests: HandleSubscribe attaches a tap; HandleUnsubscribe detaches; connection close detaches all. (AC from spec)
- **AC12** — lifecycle-via-tap test: tap on `$lifecycle` sees six event types; envelope sender = `$application`, sequence monotonic.
- **AC13** — stress test: 4 readers + 4 taps × 10k events, no deadlock, no corruption, sequences contiguous.
- **`TestSubscriptionManager.cpp` deleted** — 10 tests exercising the now-deleted class.
- **Module + DebugServer doc updates** — AC14, AC15.
- **Feature spec status** → Done.

### Key constraints
- `DebugServer` is NOT a Module and does not hold an `IApplicationInspectable*` yet — the host (`DebugServerHostModule`) passes a `IDebugStateProvider*`. To attach taps, DebugServer needs access to `IApplicationInspectable`. **Decision:** extend `IDebugStateProvider` with `FindStream(StringCRC) → IStreamStore*`, implemented by `DebugServerHostModule` which already holds the `Application*`.
- `DebugLayerManager::NotifySubscribers` sends `debug.layer.state` JSON. Post-migration, DebugLayerManager must push a typed event to a `debug.layer.state` EventStream — but that stream must be declared in the app manifest. **Decision:** keep `NotifySubscribers` on DebugServer as a thin shim (JSON→data_update broadcast) and deprecate-but-not-delete in this feature. The full migration (DebugLayerManager → stream) is a follow-on. This keeps scope tight and unblocks the DebugServer migration without requiring manifest changes in DiaVisualDebugger.
- `std::function` for `TapCallback` is already in use (F4 tap API); PD-004 exception already documented in the spec.
- No STL containers in public API — `DynamicArrayC` for all new arrays.
- SubscriptionManager deletion removes `TestSubscriptionManager.cpp` (10 tests). These tests are exercising a class being deleted, not regressions — replace with `TestDebugServerTaps.cpp`.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `SerializeToJson` + `RegisterSerializer` to `StreamTypeRegistry`; add `DIA_STREAM_TYPE_WITH_SERIALIZER` macro to `RegistrationMacrosV2.h` | `TestStreamTypeRegistry.cpp` (new, 3 tests: registered type serializes, unregistered returns nullValue + warns once, macro registers both type and serializer) | Todo | sonnet | Add `JsonSerializer = Json::Value(*)(const void*, size_t)` typedef; `static Entry s_serializerEntries[kMaxTypes]` in StreamTypeRegistry; `SerializeToJson(StringCRC, const void*, size_t)`; `RegisterSerializer(StringCRC, JsonSerializer)`. Macro in RegistrationMacrosV2.h: `DIA_STREAM_TYPE_WITH_SERIALIZER(T, fn)` calls `DIA_STREAM_TYPE(T)` then `StreamTypeSerializerRegistration<T>{StringCRC(#T), fn}`. Add to DiaApplicationFlow.vcxproj if any new .cpp needed. |
| 2 | Extend `StreamInfo` in `IApplicationInspectable.h`; populate in `Application::GetStreamInfo` | `dia run googletest --filter="Inspectable*"` (existing tests still pass; new fields populated) | Todo | sonnet | Add to `StreamInfo`: `StreamKind kind`, `Dia::Core::StringCRC payloadType`, `OverflowPolicy overflowPolicy`, `unsigned long long currentSequence`, `unsigned int attachedReaderCount`, `unsigned int attachedTapCount`. `Application::GetStreamInfo` iterates `mStreamStores`, casts to `EventStreamStore<T>` via `IStreamStore::GetTapCount()` and new `GetReaderCount()` (already exists as `GetRegisteredReaderCount` — verify). Populate `currentSequence` from `IStreamStore::GetLastSequence()` — add if missing. |
| 3 | Add `FindStream(StringCRC) → IStreamStore*` to `IApplicationInspectable`; implement in `Application` (delegates to `FindStreamStore`) | Build | Todo | haiku | Single virtual method addition. `Application::FindStream` returns `FindStreamStore(id)`. This is the tap-attachment path for DebugServer. |
| 4 | Extend `IDebugStateProvider` with `FindStream(StringCRC) → IStreamStore*`; implement in `DebugServerHostModule` (delegates to `mApplication.FindStream(id)`) | Build | Todo | haiku | `IDebugStateProvider.h` adds pure virtual `virtual IStreamStore* FindStream(const Dia::Core::StringCRC& id) = 0;`. `DebugServerHostModule` holds `Application&` ref — implement as `return mApplication.FindStream(id);`. Include `<DiaApplicationFlow/IApplicationInspectable.h>` in IDebugStateProvider.h for `IStreamStore*` fwd-decl (or use forward decl). |
| 5 | Replace `SubscriptionManager` in `DebugServer` with `mClientTaps`; rewrite `HandleSubscribe` / `HandleUnsubscribe` / `HandleConnectionClosed` as tap operations; delete `SubscriptionManager.h/.cpp` | Build (no crash) | Todo | sonnet | Add to `DebugServer`: `struct ClientTap { int connId; Dia::Core::StringCRC streamId; TapHandle handle; };` `DynamicArrayC<ClientTap, 64> mClientTaps;`. `HandleSubscribe`: call `mStateProvider->FindStream(dataType)` → `AttachTap(...)` → store handle. `HandleUnsubscribe`: linear scan, `DetachTap`, erase. `HandleConnectionClosed`: scan all matching connId, `DetachTap` each. Remove `mSubscriptionManager` member; remove `BroadcastStageTransition` (stage changes come via `$lifecycle` tap); remove stage-polling from `Tick`. Update `mStats.subscriptionCount` to `mClientTaps.Size()`. Remove `GetSubscriptionManager()` from public API. Keep `NotifySubscribers` as a shim (see session notes). |
| 6 | Add `$lifecycle` tap in `DebugServer::Start`; on `kStageTransitionCommitted` events forward to subscribed clients via legacy `stage_transition` topic alias | Build + manual smoke (`dia run cluicheeditor`, connect editor, observe stage transitions) | Todo | sonnet | In `DebugServer::Start`: after attaching log sink, call `mStateProvider->FindStream(Dia::Core::StringCRC("$lifecycle"))` → `AttachTap(...)`. Callback checks `event.type == kStageTransitionCommitted`, deserializes `LifecycleEvent` from `evt.bytes`, builds the same JSON payload as `BroadcastStageTransition` did, sends to all clients subscribed to `stage_transition` topic via the new `mClientTaps` scan (clients who subscribed to `$lifecycle` directly also get it). Store tap handle in `mLifecycleTapHandle`. In `Stop()`: `DetachTap(mLifecycleTapHandle)`. Remove `BroadcastStageTransition` and the `mLastObservedStage` polling entirely from `Tick`. |
| 7 | Delete `TestSubscriptionManager.cpp` from GoogleTests; delete from `.vcxproj` + `.vcxproj.filters` | `dia run googletest` still green | Todo | haiku | `git rm Cluiche/Tests/GoogleTests/DebugServer/TestSubscriptionManager.cpp`. Remove ClCompile entry from vcxproj + filters. |
| 8 | Write `TestDebugServerTaps.cpp`: mock `IDebugStateProvider` + mock `IStreamStore`; verify HandleSubscribe attaches tap, HandleUnsubscribe detaches, connection close detaches all | `dia run googletest --filter="DebugServerTaps*"` — 3+ tests pass | Todo | sonnet | Add to GoogleTests.vcxproj. Tests are unit-level (no real WebSocket) — use mock provider that returns a fake stream store; call HandleSubscribe/Unsubscribe/ConnectionClosed directly on DebugServer or via a thin test harness. If DebugServer is hard to unit-test (requires real WebSocket), test at the `mClientTaps` management level by exposing a test-only accessor or testing via the public Subscribe/Unsubscribe path. |
| 9 | Write AC12 lifecycle-via-tap test in `TestLifecycleEvents.cpp` (extend existing) | `dia run googletest --filter="LifecycleEvents*"` — existing + new tests pass | Todo | sonnet | Tap `$lifecycle` store directly (not via DebugServer). Fire a stage transition. Assert: callback fires with `payloadType == StringCRC("LifecycleEvent")`, `sender == StringCRC("$application")`, sequence monotonic across multiple transitions. Six event types covered: ModuleStateChanged, StageTransitionRequested, StageTransitionStarted, StageTransitionCommitted, RollbackAttempted, ShutdownRequested. |
| 10 | Write AC13 stress test in `TestStreamTap.cpp` (extend existing): 4 readers + 4 taps × 10k events | `dia run googletest --filter="StreamTap*"` — all pass | Todo | sonnet | Push 10k events from one thread; 4 readers + 4 taps registered. Assert: no deadlock (timeout-guarded); no envelope corruption (sequence values correct); sequence numbers from each consumer perspective are contiguous or show expected drops; tap count correct throughout. |
| 11 | Update `dia.application.architecture.module.md` — list tap API in `public_api`, add `FindStream` to inspectable responsibilities, remove SubscriptionManager mention | File updated | Todo | haiku | AC14. |
| 12 | Update `Dia/DiaDebugServer/dia.dia.diadebugserver.architecture.module.md` — remove SubscriptionManager from public_api; describe tap-based subscription model | File updated | Todo | haiku | AC15. |
| 13 | **Integration gate** — `dia pipeline --target googletest` + `dia pipeline --target cluichetest` + `dia pipeline --target cluicheeditor` all green | All ACs verified | Todo | sonnet | Spot-check: `grep -r SubscriptionManager Dia/ Cluiche/ --include="*.h" --include="*.cpp"` → 0. Manually connect editor to running game, subscribe to `$lifecycle`, observe stage transitions forwarded. |
| 14 | Flip feature spec to Done; update system spec; update 7 previously-Approved feature specs to Done; update backlog | Docs updated | Todo | haiku | System spec: all 8 live features Done → system spec status Done. Backlog: DiaApplicationFlow moves from In Progress to history. |

---

## Dependency Graph

```
Task 1 (SerializeToJson)         Task 2 (StreamInfo extension)   Task 3 (FindStream on inspectable)
       │                                  │                                │
       │                          Task 4 (IDebugStateProvider)            │
       │                                  │                                │
       └─────────────┬────────────────────┘────────────────────────────────┘
                     ▼
              Task 5 (SubscriptionManager → mClientTaps)
                     │
              Task 6 ($lifecycle tap in DebugServer::Start)
                     │
              Task 7 (delete TestSubscriptionManager)    Task 8 (TestDebugServerTaps)
              Task 9 (AC12 lifecycle test)                Task 10 (AC13 stress test)
                     │                                          │
                     └─────────────────┬────────────────────────┘
                                       ▼
                               Task 11 + 12 (module docs)
                                       │
                                       ▼
                               Task 13 (integration gate)
                                       │
                                       ▼
                               Task 14 (spec/backlog flip)
```

Tasks 1, 2, 3 are independent — run in parallel.  
Tasks 7, 9, 10 can run in parallel after Task 6.  
Tasks 8 depends on Task 5.

---

## Key Decisions

- **`NotifySubscribers` kept as shim** — `DebugLayerManager` is the only external caller and migrating it requires a manifest change + new stream type. Deferred. `NotifySubscribers` becomes a direct `BroadcastJson` call (no subscription list) — it broadcasts to *all* connected clients, which is safe for debug layer state (low-frequency, editor-only). The shim is documented as deprecated-pending-DebugLayerManager-stream-migration.
- **`IDebugStateProvider::FindStream`** — avoids giving DebugServer a direct `Application*` dependency. Clean interface boundary preserved.
- **`$lifecycle` tap lifetime** — attached in `Start()`, detached in `Stop()`. Handle stored in `mLifecycleTapHandle` (TapHandle member). If `FindStream("$lifecycle")` returns null (no manifest-declared stream), log a warning and skip — DebugServer degrades gracefully.
