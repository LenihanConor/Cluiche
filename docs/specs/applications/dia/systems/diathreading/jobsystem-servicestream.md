# Feature Spec: JobSystem ServiceStream Provider

**Parent:** @docs/specs/applications/dia/systems/diathreading/diathreading.md

---

## Summary

Enable SimPU modules to safely submit jobs to MainPU's `JobSystem` by publishing it as a ServiceStream. Add `TrySubmit` to `JobSystem` as a non-asserting variant of `Submit` that returns an invalid handle after shutdown, preventing cross-PU crashes when MainPU stops before SimPU completes its frame.

---

## Problem

Cross-PU `ModuleRef` isn't provided by design, and MainPU stops first during application shutdown. SimPU modules that need parallel execution (like A4's weekly weather update) cannot safely access `JobSystem` because the pool may shut down mid-frame, causing assertions in `Submit`.

---

## Goals

1. **Safe cross-PU job submission** — SimPU modules can submit jobs to MainPU's pool without risk of shutdown assertions
2. **Graceful degradation** — `TrySubmit` returns an invalid handle after shutdown, allowing callers to fall back to serial execution
3. **Established pattern** — Uses `ServiceStreamWriter<T>` pattern (like `KernelModule::mTextureHandlerService`)

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `JobSystemModule` publishes `JobSystem&` as ServiceStream with id "JobSystemService" | Inspect `JobSystemModule.h` — `ServiceStreamWriter<Dia::Threading::JobSystem>` member exists |
| AC2 | Uses `ServiceStreamWriter<JobSystem>` pattern: `Connect` in `OnConnectStreams`, `Register` in `DoStart`, `Deregister` in `DoStop` | Inspect `JobSystemModule.cpp` — all three lifecycle hooks wired |
| AC3 | `JobSystem::TrySubmit(JobFn fn)` added — non-asserting, returns invalid `JobHandle` after `Shutdown()` | Unit test: call `TrySubmit` after `Shutdown()` → `IsValid()` returns false, no assert |
| AC4 | `cow_main.diaapp` adds ServiceStream entry for "JobSystemService" | Inspect manifest — stream entry present with `kind: "ServiceStream"`, `payload_type: "JobSystem"`, `from: "MainPU"` |
| AC5 | `registeredtypes.diaschema` includes `JobSystem` type | Inspect schema — `JobSystem` entry present |
| AC6 | A4's parallel weather update can safely call `TrySubmit` from SimPU, falls back to serial if pool unavailable | Integration test: SimPU module calls `TrySubmit` via ServiceStreamReader, handles invalid handle gracefully |

---

## Data Model

### ServiceStream Entry (cow_main.diaapp)

```json
{
    "id": "JobSystemService",
    "kind": "ServiceStream",
    "payload_type": "JobSystem",
    "from": "MainPU"
}
```

### API Addition (JobSystem.h)

```cpp
// Non-asserting variant of Submit.
// Returns an invalid JobHandle (IsValid() == false) if called before Initialize
// or after Shutdown, instead of asserting.
// Callers should check IsValid() and fall back to serial execution if needed.
JobHandle TrySubmit(JobFn fn);
```

---

## Implementation Notes

### Pattern: ServiceStreamWriter Lifecycle

Follows the established `KernelModule` pattern for publishing stable handles cross-PU:

1. **Declare member:** `ServiceStreamWriter<Dia::Threading::JobSystem> mJobSystemService{this, "JobSystemService"};`
2. **OnConnectStreams:** `mJobSystemService.Connect(app);`
3. **DoStart:** `mJobSystemService.Register(mJobSystem);` (after `mJobSystem.Initialize()`)
4. **DoStop:** `mJobSystemService.Deregister();` (before `mJobSystem.Shutdown()`)

### TrySubmit Semantics

- Returns invalid handle if `mThreadPool == nullptr` (before `Initialize` or after `Shutdown`)
- Otherwise identical to `Submit` — enqueues job, increments counters, returns valid handle
- **No assert** — caller is responsible for checking `IsValid()` on returned handle

### Consumer Pattern (A4 Weather Example)

```cpp
ServiceStreamReader<Dia::Threading::JobSystem> mJobSystemReader{this, "JobSystemService"};

// In OnConnectStreams
mJobSystemReader.Connect(app);

// In DoUpdate
if (mJobSystemReader.IsAvailable()) {
    Dia::Threading::JobSystem& jobSystem = mJobSystemReader.Get();
    JobHandle h = jobSystem.TrySubmit([this]{ RunParallelWeatherUpdate(); });
    if (h.IsValid()) {
        jobSystem.Wait(h);  // Parallel execution
    } else {
        RunSerialWeatherUpdate();  // Fallback
    }
} else {
    RunSerialWeatherUpdate();  // Fallback if service not available
}
```

---

## Files Modified

| File | Change |
|------|--------|
| `Dia/DiaThreading/JobSystem.h` | Add `TrySubmit` declaration |
| `Dia/DiaThreading/JobSystem.cpp` | Implement `TrySubmit` — check `mThreadPool != nullptr`, call `Submit` or return invalid handle |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.h` | Add `ServiceStreamWriter<Dia::Threading::JobSystem> mJobSystemService{this, "JobSystemService"};` |
| `Cluiche/CluicheGameBaseline/Modules/JobSystemModule.cpp` | Wire `Connect`, `Register`, `Deregister` in lifecycle hooks |
| `Cluiche/Assets/CoW/cow_main.diaapp` | Add ServiceStream entry for "JobSystemService" |
| `Cluiche/Assets/CluicheTest/registeredtypes.diaschema` | Add `JobSystem` type registration |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|-------------------|------------|
| PD-001 | Use StringCRC for all entity/component IDs | **COMPLIES.** ServiceStream id `"JobSystemService"` is a `StringCRC` value per `ServiceStreamWriter` constructor signature. Manifest entry uses string literal, resolved to StringCRC at load time by `ManifestValidator`. |
| PD-004 | No STL containers in public APIs | **COMPLIES.** `TrySubmit` returns `JobHandle` (existing type), which uses `std::shared_ptr` internally but exposes value-type copy/move semantics. No new STL types added to public API. |
| AD-003 | Namespace convention: `Dia::<Module>::` | **COMPLIES.** `TrySubmit` added to existing `Dia::Threading::JobSystem` class. No new namespace introduced. |

No binding constraints conflict with this feature's design.

---

## Open Design Questions

None. The `ServiceStreamWriter<T>` pattern is well-established (see `KernelModule::mTextureHandlerService` precedent), shutdown semantics are clear (MainPU stops first, `Deregister()` before `Shutdown()`), and the `TrySubmit` fallback behavior is unambiguous (return invalid handle, caller checks `IsValid()`).

---

## Test Plan

### Unit Tests (GoogleTests)

1. **TrySubmit after shutdown** — `js.Shutdown(); JobHandle h = js.TrySubmit([]{}); EXPECT_FALSE(h.IsValid());`
2. **TrySubmit before initialize** — `JobSystem js; JobHandle h = js.TrySubmit([]{}); EXPECT_FALSE(h.IsValid());`
3. **TrySubmit normal case** — `js.Initialize(); JobHandle h = js.TrySubmit([]{ x = 42; }); EXPECT_TRUE(h.IsValid()); js.Wait(h); EXPECT_EQ(42, x);`

### Integration Tests

1. **ServiceStream wiring** — Launch CoW, check `JobSystemModule` registers "JobSystemService" successfully (no validation errors)
2. **Cross-PU access** — SimPU test module reads "JobSystemService", calls `TrySubmit`, verifies job executes
3. **Shutdown race** — Trigger shutdown while SimPU frame is mid-flight, verify `TrySubmit` returns invalid handle (no crash)

---

## Status

`Done` — 2026-10-15
