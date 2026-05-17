# Feature Spec: AssetService Stage-Transition Watcher

## Parent System
@docs/specs/systems/cluichetest/async-asset-loading.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | Async Stage Asset Loading | @docs/specs/systems/cluichetest/async-asset-loading.md |
| Feature | AssetService Stage-Transition Watcher | (this file) |

## Problem Statement

The system makes per-stage asset loading **transition-driven** (system SD-006): when the framework transitions into a new stage, `AssetServiceModule` (MainPU) auto-issues `RequestStageLoad(newStageId)` to the runtime. Stage Modules only poll completion via a thread-safe static accessor.

This feature implements the engine-side enabler:
1. The transition watcher inside `AssetServiceModule::DoUpdate`
2. The thread-safe static accessor (`AssetServiceModule::GetStatic()`)
3. The completion/state queries (`IsStageLoadComplete`, `GetStageLoadState`)

Without this, the consumer feature (DummyStage Async Load Consumer) has nothing to poll.

## Acceptance Criteria

1. `AssetServiceModule::DoUpdate(float)`:
   - Reads current stage via `GetApplication()->GetCurrentStage()` (or equivalent — verify exact API in Discovery)
   - Compares to `mLastWatchedStage` member
   - On change: calls `mRuntime.RequestStageLoad(newStageId)`; updates `mLastWatchedStage`
   - Calls `mTextureHandler.Tick()` (per system SD-003) — the texture-handler feature defines this; this feature wires the call
2. **Static accessor:** `AssetServiceModule::GetStatic() → AssetServiceModule*`
   - Set in `DoStart`, cleared (set to `nullptr`) in `DoStop`
   - Internal storage is a `std::atomic<AssetServiceModule*>` (or equivalent; matches `KernelModule::sCanvas` / `sTextureHandler` pattern — verify whether those are atomic or plain pointers, follow precedent)
3. **`AssetServiceModule::IsStageLoadComplete(StringCRC stageId) → bool`** (public, thread-safe):
   - Returns `true` if the stage has no declared assets, or if all declared assets have loaded
   - Returns `false` while loads are in-flight
   - Returns `false` if the stage has never been requested
   - Internally reads atomic per-stage state; no mutex required for this query
4. **`AssetServiceModule::GetStageLoadState(StringCRC stageId) → StageLoadState`** (public, thread-safe):
   - Returns `enum class StageLoadState { kIdle, kLoading, kComplete, kFailed }`
   - `kIdle` = never requested
   - `kLoading` = `RequestStageLoad` issued, not all assets resolved
   - `kComplete` = all assets resolved successfully (matches `IsStageLoadComplete == true`)
   - `kFailed` = at least one asset's `OnLoadFailed` has fired and not been retried
5. **State storage**: a `Dia::Core::HashTable<StringCRC, std::atomic<StageLoadState>>` (or DiaCore equivalent) member on `AssetServiceModule`. Updated by:
   - `RequestStageLoad`: set entry to `kLoading`
   - `IAssetLoadCallback::OnLoadComplete`: when all assets in the stage have completed, transition entry to `kComplete`
   - `IAssetLoadCallback::OnLoadFailed`: transition entry to `kFailed` (any single failure)
6. **No new public manifest changes** — existing entry already declares `AssetService` with `"stages": ["all"]`
7. **Single-threaded write contract preserved**: all writes to the state table happen on MainPU (from `DoUpdate` and from runtime callbacks, both on owner thread). Reads are atomic.
8. Logs at first detection of a stage transition (`"AssetService: detected transition to <stage>, requesting load"`), and at terminal state per stage (`"AssetService: stage <stage> complete"` / `"failed"`)

## Implementation Sketch

```cpp
// AssetServiceModule.h additions
class AssetServiceModule : public Dia::ApplicationFlow::Module {
public:
    enum class StageLoadState { kIdle, kLoading, kComplete, kFailed };

    static AssetServiceModule* GetStatic();          // thread-safe, may return nullptr pre-DoStart / post-DoStop

    // Thread-safe queries (callable from any PU)
    bool           IsStageLoadComplete(const Dia::Core::StringCRC& stageId) const;
    StageLoadState GetStageLoadState (const Dia::Core::StringCRC& stageId) const;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    static std::atomic<AssetServiceModule*> sInstance;

    // Existing members (mRuntime, mTextureHandler, etc.)

    Dia::Core::StringCRC mLastWatchedStage;          // default-constructed = "no stage yet"

    // Per-stage atomic state. HashTable owned on Main; values are atomic for cross-PU reads.
    Dia::Core::HashTable<Dia::Core::StringCRC, std::atomic<StageLoadState>> mStageState;

    // Helper: compute current stage state from runtime asset states
    StageLoadState ComputeStageState(const Dia::Core::StringCRC& stageId) const;
};
```

```cpp
// AssetServiceModule.cpp additions
std::atomic<AssetServiceModule*> AssetServiceModule::sInstance{nullptr};

AssetServiceModule* AssetServiceModule::GetStatic() {
    return sInstance.load(std::memory_order_acquire);
}

bool AssetServiceModule::IsStageLoadComplete(const Dia::Core::StringCRC& stageId) const {
    return GetStageLoadState(stageId) == StageLoadState::kComplete;
}

AssetServiceModule::StageLoadState
AssetServiceModule::GetStageLoadState(const Dia::Core::StringCRC& stageId) const {
    if (auto* slot = mStageState.TryGetItemConst(stageId)) {
        return slot->load(std::memory_order_acquire);
    }
    // Special case: catalogue declares no assets for this stage -> kComplete.
    // (Verify with catalogue API. Conservative default if unknown: kIdle.)
    return StageLoadState::kIdle;
}

Dia::ApplicationFlow::StartResult AssetServiceModule::DoStart() {
    DIA_LOG_INFO("Application", "AssetServiceModule DoStart entry");
    // ... existing setup (LoadManifest, register handlers, RequestGlobalLoad) ...
    sInstance.store(this, std::memory_order_release);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetServiceModule::DoUpdate(float dt) {
    // 1. Stage-transition watch
    Dia::Core::StringCRC currentStage = GetApplication()->GetCurrentStage();
    if (currentStage != mLastWatchedStage) {
        DIA_LOG_INFO("Application",
            "AssetService: detected transition to '%u', requesting load",
            currentStage.Value());
        // Initialize/refresh state slot to kLoading
        auto& slot = mStageState[currentStage];  // creates if missing
        slot.store(StageLoadState::kLoading, std::memory_order_release);
        mRuntime.RequestStageLoad(currentStage);
        mLastWatchedStage = currentStage;
    }

    // 2. Pump per-handler main-thread completion work
    mTextureHandler.Tick();   // per system SD-003

    // 3. Recompute terminal states for any kLoading stage
    for (auto& [stageId, atomicState] : mStageState) {
        if (atomicState.load(std::memory_order_acquire) == StageLoadState::kLoading) {
            StageLoadState computed = ComputeStageState(stageId);
            if (computed != StageLoadState::kLoading) {
                atomicState.store(computed, std::memory_order_release);
                DIA_LOG_INFO("Application",
                    "AssetService: stage '%u' %s",
                    stageId.Value(),
                    computed == StageLoadState::kComplete ? "complete" : "failed");
            }
        }
    }
}

Dia::ApplicationFlow::StopResult AssetServiceModule::DoStop() {
    DIA_LOG_INFO("Application", "AssetServiceModule DoStop entry");
    sInstance.store(nullptr, std::memory_order_release);
    // ... existing teardown ...
    return Dia::ApplicationFlow::StopResult::kDone;
}
```

`ComputeStageState` queries the catalogue + runtime to determine if the stage's declared assets are all loaded, any failed, or still in flight. Implementation depends on existing `AssetCatalogue`/`AssetRuntime` APIs — Discovery Task 2 confirms exact calls.

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.h` | Update — add `StageLoadState` enum, static accessor, query API, state storage |
| `Cluiche/CluicheGameBaseline/Modules/AssetServiceModule.cpp` | Update — `DoUpdate` watcher, state machine, callbacks |
| `Cluiche/Tests/GoogleTests/CluicheGameBaseline/TestAssetServiceModule.cpp` | New — covers transition detection, state queries, kComplete/kFailed/kIdle |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` / `.filters` | Update — register new test file |

## Discovery Tasks (before editing)

| # | Task |
|---|------|
| 1 | Verify the exact API for "current stage" on `Application` — likely `GetCurrentStage()` returning `StringCRC` (per CT AppFlow spec); confirm in `Dia/DiaApplicationFlow/Application.h` |
| 2 | Verify how to query per-asset load state from `AssetRuntime` to compute stage-level state — there may already be a per-stage rollup; if not, walk the catalogue's stage entries and check `IsLoadComplete` / failure tracking |
| 3 | Verify whether `KernelModule::sCanvas` / `sTextureHandler` are `std::atomic` or plain pointers, and follow that precedent for `sInstance` |
| 4 | Verify `Dia::Core::HashTable` supports `std::atomic<T>` values (may need to use a wrapper struct if it doesn't permit non-trivially-movable values) |

## Dependencies

- **DiaApplicationFlow** — `Application::GetCurrentStage` (or equivalent), `Module` base
- **DiaAssetRuntime** — `AssetRuntime::RequestStageLoad`, per-asset load state queries
- **DiaAssetCatalogue** — to walk declared assets per stage when computing rollup state
- **DiaCore/CRC** — `StringCRC`
- **DiaCore/Containers** — `HashTable`
- **DiaLogger** — `DIA_LOG_INFO`
- **TextureHandler Two-Phase Load** feature — `DoUpdate` calls `mTextureHandler.Tick()` (cross-feature wire-up)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — stage/asset IDs are `StringCRC` |
| PD-004 | Platform | No STL in public APIs | Compliant — public API uses `StringCRC` and an enum; no STL types in signatures. Internal `std::atomic` is impl detail |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | App | Three ProcessingUnits | Compliant — module remains on MainPU |
| SD-001 (CT AppFlow) | System | Three PUs: Main/Sim/Render | Compliant — watcher and writes on Main; only reads cross to Sim |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop and at transitions | Compliant |
| SD-003 (Async) | System | `AssetServiceModule::DoUpdate` calls `TextureHandler::Tick()` | This feature wires that call |
| SD-004 (Async) | System | AssetRuntime stays single-thread; callbacks on owner thread | Compliant — runtime calls and callbacks all on Main |
| SD-006 (Async) | System | Stage loads transition-driven; stage Modules only poll | This feature **is** the transition-driver |
| SD-007 (Async) | System | DoStart no longer pre-loads DummyStage; only `stage.global` | Compliant — handled by Asset Deployment feature; this feature does the per-stage load on transition |
| SD-011 (Async) | System | Cross-PU read via thread-safe static accessor | This feature **is** that accessor |
| SD-012 (Async) | System | `IsStageLoadComplete` + `GetStageLoadState` distinguish empty/loading/complete/failed | This feature implements both queries |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Are atomic loads sufficient for `IsStageLoadComplete` reads from SimPU, or does the HashTable lookup itself need synchronization? | The `mStageState` HashTable is mutated only on Main (entries added during `DoUpdate` on transition). If `Dia::Core::HashTable` mutation can race with reads from other threads, we need either a mutex around the lookup or a different data structure (e.g., a fixed-size array indexed by stage). **Discovery Task 4 resolves this.** Worst case: small `std::shared_mutex` around the HashTable; readers take shared lock, writer takes unique. |
| 2 | Threading | What if a Sim-thread reader queries `IsStageLoadComplete` *before* `DoStart` has set `sInstance`? | `GetStatic()` returns `nullptr`. Callers must null-check. In practice: `DummyLevelModule::DoStart` only runs after the framework has started all `"stages": ["all"]` modules, so by definition `AssetService::DoStart` has completed and `sInstance` is set. Document the contract: callers must handle `nullptr` defensively only if they call before module bringup is complete. |
| 3 | Semantics | If the stage has zero assets in the catalogue, what does `GetStageLoadState` return? | `kComplete` — empty manifests must not block. Implementation: if `ComputeStageState` finds zero assets declared, returns `kComplete` directly. The state slot is still populated on transition for consistency. |
| 4 | Semantics | When `GetStageLoadState` is called with a stage ID that's never been transitioned to, return `kIdle`. Is that right, or should it auto-trigger a load? | `kIdle`. Auto-triggering on query would mean side-effects on a const accessor — wrong. The transition watcher is the only loader. If a stage's assets are needed without transitioning to it (preload), that's a separate API (out of scope per system Out of Scope). |
| 5 | Failure | One asset failing makes the whole stage `kFailed`. Is that too aggressive? | For now, yes — it's the simple, safe default. Partial-success semantics ("stage is mostly loaded, render with placeholders") is a real game requirement but a separate design problem. Out of scope here; revisit when a real game needs it. |
| 6 | Lifecycle | If the stage transitions from `DummyStage` → `BootStage` → `DummyStage`, does the second entry re-issue the load? | Yes. The watcher detects the transition (new stage != `mLastWatchedStage`). The runtime dedups via the texture-handler cache hit path (per SD-009) so already-loaded textures fire `OnLoadComplete` immediately. State transitions kLoading → kComplete in the same frame for cached assets. |
| 7 | Lifecycle | What happens during the very first `DoUpdate` when `mLastWatchedStage` is default-constructed (empty `StringCRC`)? | The Application's initial stage (Boot) compares `!=` to the default. The watcher fires for Boot too. Boot may have empty stage assets (`kComplete` immediately) — that's fine. |
| 8 | Lifecycle | What if the Application is in the middle of a transition when `DoUpdate` reads `GetCurrentStage()`? | Per CT AppFlow SD-005 (DiaAppFlow), transitions are queued and applied at start-of-frame. By the time `DoUpdate` runs, the stage value is stable for the frame. No mid-frame race. |
| 9 | Compatibility | The existing `AssetServiceModule::DoStart` already pre-loads `stage.global`. Does this feature change that? | No. `stage.global` is loaded explicitly in `DoStart` (preserved). The transition watcher is additional, for per-stage loads. The DummyStage Asset Deployment feature removes DummyStage assets from the global pre-load path; this feature picks them up via transition. |
| 10 | Performance | The watcher iterates `mStageState` every frame to recompute states. Cost? | At most one entry per stage; for a game with 100 stages that's a 100-entry iteration with one atomic load per entry = negligible. If `kComplete` and `kFailed` are terminal (which they are), only `kLoading` entries need recomputation — optimize the loop to skip terminal entries if needed. Not necessary for stage counts < 1000. |
| 11 | Test isolation | How do GoogleTests verify the watcher without spinning up a full Application? | Test fixture constructs `AssetServiceModule` directly, sets a fake "current stage" via a test-only seam, calls `DoUpdate` repeatedly, asserts state transitions. May need a small `SetCurrentStageForTest(StringCRC)` test seam since real `Application::GetCurrentStage` requires full app bringup. Document the test seam clearly (`#ifdef DIA_TESTING` or a `friend` declaration). |
| 12 | API surface | Should `GetStageLoadState` go on `AssetServiceModule`, or be added to `AssetRuntime` itself (as part of DiaAssetRuntime)? | On `AssetServiceModule` for now — it's the consumer-facing API and the stage-state aggregation is a service-level concern, not runtime. If a second consumer of stage-state aggregation appears, lift it to AssetRuntime. Current rule: don't expand DiaAssetRuntime's API for one caller. |

## Open Questions

- **Discovery Task 1**: exact `Application::GetCurrentStage` signature — verify before coding.
- **Discovery Task 2**: per-stage asset rollup — does `AssetRuntime`/`AssetCatalogue` already provide a "list assets in stage X" iterator, or do we need to walk the catalogue manually?
- **Discovery Task 4**: `Dia::Core::HashTable` interaction with `std::atomic<T>` values — may force a wrapper.

## Status

`Approved` — 2026-05-17
