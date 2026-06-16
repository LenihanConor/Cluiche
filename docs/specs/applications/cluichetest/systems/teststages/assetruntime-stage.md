# Feature Spec: AssetRuntime Test Stage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-010 |
| Application | @docs/specs/applications/cluichetest/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/applications/cluichetest/systems/teststages/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/applications/cluichetest/systems/teststages/test-stage-infrastructure.md | Depends on Tasks 1-2 (manifest loader + transitions field) |

## Problem Statement

Unit tests for DiaAssetRuntime mock file IO or load single assets synchronously. They cannot catch: concurrent multi-type loading races, handle lifetime bugs across stage transitions, or leaked handles after unload. This stage validates the full asset pipeline under real conditions — concurrent loads of multiple asset types, correct cleanup on stage exit, and clean reload on re-entry.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature exercised | DiaAssetRuntime: concurrent multi-type async loading, handle lifecycle, stage-transition cleanup |
| T2 | Scene setup in DoStart | Request 4 assets concurrently (texture, clip, rig, json config). Wait for all handles to report ready. Record handle state snapshot. |
| T3 | Checkpoint(s) and success conditions | `asset_runtime.all_loaded` → true when all 4 handles are ready. `asset_runtime.clean_reload` → true on second stage entry when handle count and state match first entry's snapshot. |
| T4 | Metrics emitted | `cluichetest.asset_runtime.load_count` (total assets loaded), `cluichetest.asset_runtime.active_handles` (handles alive at checkpoint time), `cluichetest.asset_runtime.load_time_ms` (wall time from request to all-ready) |
| T5 | Processing Unit | MainPU (standard asset request origin) |
| T6 | Assets needed | 4 test assets: 1 texture (.diatex), 1 clip (.diaclip), 1 rig (.diarig), 1 json config (.json) |
| T7 | Gap vs unit tests | Unit tests mock IO, load one asset at a time, never test stage-transition cleanup or concurrent load races |
| T8 | Determinism constraints | Load order may vary (async), but final state must be identical. Checkpoint validates state, not ordering. |
| T9 | Expected frame budget | ~60 frames (2s at 30Hz) for initial load. Second entry (after round-trip) may be faster due to caching — still within 60 frames. Total stage budget: 120 frames per entry. |
| T10 | Dependencies on other modules | TimeServer, AutomationModule, DiaAssetRuntime (AssetManager, handles) |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-AR1 | DoStart requests 4 assets of different types concurrently | Code review — all requests issued before any wait |
| AC-AR2 | `asset_runtime.all_loaded` returns true only when all 4 handles report ready | Checkpoint poll → eventually true |
| AC-AR3 | DoStop releases all handles; active handle count returns to zero | Metric `active_handles` = 0 after stop (verified on re-entry) |
| AC-AR4 | After Boot → Stage → Boot → Stage round-trip, second entry's loaded state matches first | `clean_reload` checkpoint passes on second entry |
| AC-AR5 | No leaked handles after DoStop (AssetManager's active count matches pre-stage count) | Metric comparison: pre-entry vs post-stop active count |
| AC-AR6 | Stage loads complete within 120 frames per entry (4s at 30Hz) | Orchestrator timeout per entry |
| AC-AR7 | `load_time_ms` metric emitted and > 0 | Metric query returns positive value |

## Design

**Visual mockup:** [@docs/specs/applications/cluichetest/systems/teststages/assetruntime-stage.mockup.html](assetruntime-stage.mockup.html) — open in a browser for visual acceptance gate reference.

### Two-Phase Validation

The orchestrator drives this stage through two entries to validate the reload:

```
Phase 1: Boot → AssetRuntimeStage (load 4 assets, snapshot state, checkpoint all_loaded)
         AssetRuntimeStage → Boot (DoStop releases handles)
Phase 2: Boot → AssetRuntimeStage (reload same 4 assets, compare to snapshot, checkpoint clean_reload)
         AssetRuntimeStage → Boot (DoStop releases handles)
```

The module tracks which entry this is (first vs subsequent) using a persistent counter that resets only on module destruction (not on DoStop — DoStop runs between entries within the same application session).

### Asset Manifest

| Asset | Type | Path | Purpose |
|-------|------|------|---------|
| test_texture | Texture | `stages/AssetRuntimeStage/assets/test_texture.diatex` | Validates texture pipeline |
| test_clip | AnimClip | `stages/AssetRuntimeStage/assets/test_clip.diaclip` | Validates clip pipeline |
| test_rig | Rig | `stages/AssetRuntimeStage/assets/test_rig.diarig` | Validates rig pipeline |
| test_config | JSON | `stages/AssetRuntimeStage/assets/test_config.json` | Validates generic asset pipeline |

### State Snapshot

On first entry, after all loads complete, the module captures:
- Number of active handles (should be 4)
- Handle IDs (for identity comparison on reload)
- Load success flags (all true)

On second entry, after loads complete, it compares against the snapshot.

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/AssetRuntimeStageModule.h
namespace CluicheTest {

class AssetRuntimeStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"AssetRuntimeStageModule"};
    explicit AssetRuntimeStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void RequestAssets();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    bool AllHandlesReady() const;
    void CaptureSnapshot();
    bool SnapshotMatches() const;

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};

    Dia::AssetRuntime::AssetHandle mTextureHandle;
    Dia::AssetRuntime::AssetHandle mClipHandle;
    Dia::AssetRuntime::AssetHandle mRigHandle;
    Dia::AssetRuntime::AssetHandle mConfigHandle;

    struct LoadSnapshot {
        unsigned int activeHandleCount = 0;
        bool allSucceeded = false;
    };

    LoadSnapshot mFirstEntrySnapshot;
    unsigned int mEntryCount = 0;
    unsigned int mFrameCount = 0;
    unsigned int mLoadStartFrame = 0;
    bool mAllLoaded = false;
    bool mCleanReload = false;
};

} // namespace CluicheTest
DIA_MODULE(AssetRuntimeStageModule);
```

### Checkpoint Logic

```cpp
// asset_runtime.all_loaded — all 4 handles ready
automation->RegisterCheckpoint(this, StringCRC("asset_runtime.all_loaded"),
    [this]() -> CheckpointResult {
        return { mAllLoaded, mAllLoaded ? "4/4 assets ready" : "loading in progress", 0.0f };
    });

// asset_runtime.clean_reload — second entry matches first entry's state
automation->RegisterCheckpoint(this, StringCRC("asset_runtime.clean_reload"),
    [this]() -> CheckpointResult {
        if (mEntryCount < 2)
            return { false, "awaiting second entry", 0.0f };
        return { mCleanReload, mCleanReload ? "reload state matches" : "state mismatch after reload", 0.0f };
    });
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/assetruntime/test_asset_lifecycle.py

def test_asset_concurrent_load_and_clean_reload(dia_client):
    """Two-entry test: load assets, exit, re-enter, verify clean state."""
    # Phase 1: first entry
    dia_client.navigate_to("AssetRuntimeStage")
    result = dia_client.poll_checkpoint("asset_runtime.all_loaded", timeout_s=5.0)
    assert result["passed"], f"First load failed: {result['message']}"

    # Return to Boot (triggers DoStop → handle release)
    dia_client.navigate_to("Boot")

    # Phase 2: second entry
    dia_client.navigate_to("AssetRuntimeStage")
    result = dia_client.poll_checkpoint("asset_runtime.all_loaded", timeout_s=5.0)
    assert result["passed"], f"Reload failed: {result['message']}"

    result = dia_client.poll_checkpoint("asset_runtime.clean_reload", timeout_s=2.0)
    assert result["passed"], f"Clean reload failed: {result['message']}"

    metrics = dia_client.query_metrics("cluichetest.asset_runtime.*")
    assert metrics["cluichetest.asset_runtime.load_count"] == 4
    assert metrics["cluichetest.asset_runtime.active_handles"] == 4
    assert metrics["cluichetest.asset_runtime.load_time_ms"] > 0

    dia_client.navigate_to("Boot")
```

### Manifest Entry

```json
{
    "name": "AssetRuntimeStage",
    "manifest": "stages/AssetRuntimeStage/misc/ApplicationFlow/asset_runtime_stage.diaapp",
    "config": {
        "path_aliases": { "stage_root": "." }
    },
    "transitions": ["Boot"],
    "auto_advance": false
}
```

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/features/cluichetest/teststages/assetruntime-stage.mockup.html` | New — visual acceptance gate |
| `Cluiche/CluicheTest/Modules/TestStages/AssetRuntimeStageModule.h` | New — module header |
| `Cluiche/CluicheTest/Modules/TestStages/AssetRuntimeStageModule.cpp` | New — module implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/Stages/AssetRuntimeStage/misc/ApplicationFlow/asset_runtime_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/Stages/AssetRuntimeStage/asset_runtime_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/Stages/AssetRuntimeStage/assets/test_texture.diatex` | New — test texture asset |
| `Cluiche/Assets/Stages/AssetRuntimeStage/assets/test_clip.diaclip` | New — test clip asset |
| `Cluiche/Assets/Stages/AssetRuntimeStage/assets/test_rig.diarig` | New — test rig asset |
| `Cluiche/Assets/Stages/AssetRuntimeStage/assets/test_config.json` | New — test json asset |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for AssetRuntime stage |
| `Tools/orchestrator/scenarios/cluichetest/assetruntime/test_asset_lifecycle.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Create `assetruntime-stage.mockup.html` — 3 texture quads (red/green/blue) + JSON data card in world space, asset panel, reload snapshot, checkpoint panel, HUD | Visual sign-off before C++ | Done | sonnet | |
| 1 | Add `JsonPassthroughHandler.h/.cpp` to `Dia/DiaAssetRuntime/Handlers/`; register with prefix `"json"` in `AssetServiceModule::EnsureHandlersRegistered()`; add to `DiaAssetRuntime.vcxproj` | `dia run googletest` passes (no regressions) | Todo | sonnet | Prerequisite for JSON asset in T-2 |
| 2 | Create 4 test asset files (3×1×1 PNG textures + 1 JSON config) under `Cluiche/Assets/Stages/AssetRuntimeStage/assets/`; register all 4 in `assets.runtime.json` under stage `stage.asset_runtime_stage` | Assets resolve via `runtime.ResolveAssetPath()` | Todo | haiku | Minimal valid content |
| 3 | Create `AssetRuntimeStageModule.h/.cpp` skeleton — `kTypeId`, `DIA_MODULE` macro, empty stubs | Compiles, module registered | Todo | haiku | |
| 4 | Implement DoStart: increment `mEntryCount`; register 2 checkpoints; return `kReady` | Checkpoints registered | Todo | sonnet | No explicit RequestStageLoad — AssetServiceModule fires it |
| 5 | Implement DoUpdate: poll `IsStageLoadComplete`; on ready set `mAllLoaded`, capture/compare snapshot; emit 3 metrics per-frame | `all_loaded` passes entry 1; `clean_reload` passes entry 2 | Todo | sonnet | |
| 6 | Implement DoStop: reset `mAllLoaded`, `mCleanReload`, `mFrameCount`; do NOT reset `mEntryCount` or snapshot | Second-entry comparison correct | Todo | haiku | |
| 7 | Render loaded textures: in DoUpdate, after all-loaded, submit one bgfx quad per texture using `TextureHandler::LookupTexture()` | 3 coloured quads visible per mockup | Todo | sonnet | Visual acceptance gate against mockup |
| 8 | Create `asset_runtime_stage.diastage` + `asset_runtime_stage.diaapp`; add import to `cluichetest.diagame`; add to `assets.catalogue.json` | Stage visible in Boot menu | Todo | haiku | |
| 9 | Add all new source files to `CluicheTest.vcxproj` + `.vcxproj.filters` | Clean build | Todo | haiku | |
| 10 | Write pytest scenario `assetruntime/test_asset_lifecycle.py` — two-entry pattern | Both checkpoints pass + metrics | Todo | sonnet | |
| 11 | Add scenario to `plans/cluichetest/default.json` | `--list` shows scenario | Todo | haiku | |
| 12 | Verify: full E2E pass (two-entry round-trip); visual check against mockup | Orchestrator green + quads visible | Todo | sonnet | Requires Infrastructure complete |

## Dependencies

- T-1 (JsonPassthroughHandler) must complete before T-2 (JSON asset registration)
- T-2 and T-3 can run in parallel after T-1
- T-4, T-5, T-6, T-7 depend on T-3 (sequential — each builds on previous)
- T-8, T-9, T-10, T-11 can run in parallel after T-7
- T-12 requires T-8 through T-11 complete, plus **Infrastructure spec Tasks 1-6**

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names all StringCRC. Asset paths stored as strings (loaded by AssetManager). |
| PD-004 | No STL in public APIs | Module interface uses Dia types. Handles are Dia asset handles, not STL smart pointers. |
| PD-006 | VS project files source of truth | Task 9 adds files to CluicheTest.vcxproj. |
| PD-007 | C++20 required | constexpr StringCRC, standard features. |
| PD-010 | .diastage for stages | Stage declared in `.diastage`. Test assets stored under stage directory. |
| AD-001 (CT) | Three PUs | Module lives on MainPU (standard asset request origin). |
| AD-004 (CT) | Test levels included | This IS a test level. |
| AD-005 (CT) | App is testbed not product | Stage exists purely for asset pipeline validation. |
| SD-TS-001 | One manifest stage per feature | One stage: AssetRuntimeStage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | RegisterCheckpoint called after asset requests issued. Auto-clear on stop. |
| SD-TS-003 | Metrics for threshold assertions | `load_count`, `active_handles`, `load_time_ms` emitted. Pytest asserts expected values. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Entry Counter | `mEntryCount` must persist across DoStop/DoStart. Is that safe given module lifecycle rules? | Yes — plain CPU-side scalars survive across DoStop/DoStart within the same application session. The module object lives for the full app lifetime; only DoStart/DoStop mark active periods. The counter resets only on destruction (which only happens at app shutdown). |
| 2 | Concurrent Load | What if one asset type doesn't support async loading? | The spec requires all 4 to be async. If a type (e.g., JSON) is loaded synchronously today, it still works — it just completes immediately. The "concurrent" requirement means requests are issued together, not that they must all be in-flight simultaneously. |
| 3 | Handle Identity | Should the reload check compare handle IDs or just counts? | Counts + success flags. Handle IDs may differ between loads (new allocation). The invariant is: same number of handles, all succeeded, no leaked handles from the previous entry. Identity comparison would be too strict. |
| 4 | Cache Effects | Second load may be faster due to OS file cache. Does that affect the test? | No — the test validates correctness (handles ready, state clean), not performance. `load_time_ms` is emitted for observability but not asserted against a threshold. Caching is expected and harmless. |
| 5 | Asset Formats | Do all 4 asset types (.diatex, .diaclip, .diarig, .json) have loaders today? | Verify before implementation. If a loader doesn't exist (e.g., .diatex), either add it as a dependency or substitute a type that does have a loader. The test cares about concurrency and lifecycle, not the specific asset type. |
| 6 | Leaked Handle Detection | How does the module detect leaked handles from a previous entry? | On second entry's DoStart, before requesting new assets, query AssetManager's total active handle count. Compare against expected baseline (should be 0 from this module — other modules may hold handles, so compare delta, not absolute). If delta > 0, previous stop leaked. |

## Status

`Approved` — 2026-05-22
