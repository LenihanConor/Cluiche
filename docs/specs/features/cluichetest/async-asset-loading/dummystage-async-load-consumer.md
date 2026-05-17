# Feature Spec: DummyStage Async Load Consumer

## Parent System
@docs/specs/systems/cluichetest/async-asset-loading.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | Async Stage Asset Loading | @docs/specs/systems/cluichetest/async-asset-loading.md |
| Feature | DummyStage Async Load Consumer | (this file) |

## Problem Statement

`DummyLevelModule::DoStart` (`Cluiche/Stages/DummyStage/DummyLevelModule.cpp:29–40`) currently flips a placeholder `mStartLoadRequested` flag and returns `kLoading → kReady` over two frames without doing any actual loading.

With the JobSystem Module, two-phase TextureHandler, and AssetService Stage-Transition Watcher in place, DummyStage becomes the canonical reference implementation. Per system SD-006 and SD-011, the load is **transition-driven** — `AssetServiceModule` (MainPU) auto-issues the load when the framework transitions into DummyStage. `DummyLevelModule` (SimPU) is purely a **reader**: it polls completion via the thread-safe static accessor.

This is the visible end of the system — the user-facing pattern that every future stage will copy. Stage code is mechanical and minimal.

## Acceptance Criteria

1. `DummyLevelModule.h`: no new `ModuleRef` is added (cross-PU `ModuleRef` doesn't work — see system SD-011)
2. `mStartLoadRequested` (bool placeholder) is **removed** entirely; no replacement state is needed because the polling `DoStart` is stateless
3. `DoStart()`:
   - Calls `AssetServiceModule::GetStatic()->IsStageLoadComplete(StringCRC("DummyStage"))`
   - If `true` → returns `StartResult::kReady`
   - If load state is `Failed` (via `GetStageLoadState`) → returns `StartResult::kFailed`, log diagnostic
   - Otherwise → returns `StartResult::kLoading`
4. `DoStop()`: unchanged from current — log entry, return `StopResult::kDone`
5. `DoUpdate` (lines 102–131): texture lookup unchanged; `KernelModule::GetStaticTextureHandler()` still resolves IDs at draw time; sprites render identically once loaded
6. `dummy_stage.diaapp`: **no manifest dependency change** — `DummyLevel` does NOT add `"AssetService"` to its `dependencies` array (cross-PU dependencies aren't supported and aren't needed; the framework guarantees `AssetService` (`"stages": ["all"]`) is started before any stage-scoped module)
7. Boot path through `cluichetest`: DummyStage's `DoStart` returns `kLoading` for at least 1 frame in normal conditions and at least 5+ frames when a 100ms artificial sleep is injected into the decode worker (proves the async path is real, not a stub)
8. Logs at `DoStart` first call (no entry log on every poll — that would spam) and `DoStop` entry/exit per SD-007 (CT AppFlow)
9. No regression in DummyStage visuals: red/blue/green sprites render at the same positions/scales/rotations/tints as before

## Implementation Sketch

```cpp
// DummyLevelModule.h diff — no ModuleRef added; remove the placeholder state
class DummyLevelModule : public Dia::ApplicationFlow::Module {
    // ...
private:
    // remove: bool mStartLoadRequested = false;
    // unchanged: float mSpriteX, mSpriteY, mScore;
    bool mLoadEntryLogged = false;  // log only the first DoStart call
};
```

```cpp
// DummyLevelModule.cpp DoStart replacement (lines 29–40)
Dia::ApplicationFlow::StartResult DummyLevelModule::DoStart() {
    AssetServiceModule* assets = AssetServiceModule::GetStatic();
    const Dia::Core::StringCRC kStageId("DummyStage");

    if (!mLoadEntryLogged) {
        DIA_LOG_INFO("Application", "DummyLevelModule DoStart entry — polling for stage load");
        mLoadEntryLogged = true;
    }

    using LoadState = AssetServiceModule::StageLoadState;
    LoadState state = assets->GetStageLoadState(kStageId);

    if (state == LoadState::kFailed) {
        DIA_LOG_ERROR("Application", "DummyLevelModule DoStart — stage load failed");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }
    if (state == LoadState::kComplete) {
        DIA_LOG_INFO("Application", "DummyLevelModule DoStart ready");
        return Dia::ApplicationFlow::StartResult::kReady;
    }
    return Dia::ApplicationFlow::StartResult::kLoading;
}

Dia::ApplicationFlow::StopResult DummyLevelModule::DoStop() {
    DIA_LOG_INFO("Application", "DummyLevelModule DoStop entry");
    mLoadEntryLogged = false;  // reset for potential re-entry
    return Dia::ApplicationFlow::StopResult::kDone;
}
```

```jsonc
// dummy_stage.diaapp — DummyLevel entry unchanged from existing applicationflow spec
{
    "instance_id": "DummyLevel",
    "type_id": "DummyLevelModule",
    "stages": ["DummyStage"],
    "dependencies": ["TimeServer", "InputStream"],
    "reads": ["InputToSim"],
    "writes": ["SimToRender", "SimToUI"],
    "start_timeout_ms": 5000,
    "stop_timeout_ms": 2000
}
```

The trigger flow (no code in DummyLevelModule):

```
[Main thread]                              [Sim thread]
Application::TransitionTo("DummyStage")
AssetServiceModule::DoUpdate
   ├ stage = App->GetCurrentStage()
   ├ stage changed -> Runtime.RequestStageLoad("DummyStage")
   └ TextureHandler::Tick()
                                          DummyLevelModule::DoStart  (polls each frame)
                                            ├ AssetSvc::GetStatic()
                                            │   ->GetStageLoadState("DummyStage")
                                            └ kComplete -> kReady
```

## Files Touched

| File | Action |
|------|--------|
| `Cluiche/Stages/DummyStage/DummyLevelModule.h` | Update — remove `mStartLoadRequested`, add `mLoadEntryLogged` |
| `Cluiche/Stages/DummyStage/DummyLevelModule.cpp` | Update — replace `DoStart`/`DoStop` bodies |
| `Cluiche/Assets/Stages/DummyStage/misc/ApplicationFlow/dummy_stage.diaapp` | No change (the asset-deployment feature creates this file with these contents) |
| `Cluiche/Stages/DummyStage/DummyStage.vcxproj` (if separate) | No change |

## Dependencies

- **JobSystem Module** feature — must land first (or in same plan)
- **TextureHandler Two-Phase Async Load** feature — must land first
- **AssetService Stage-Transition Watcher** feature — **must land first**; provides `GetStatic()`, `IsStageLoadComplete`, `GetStageLoadState`
- **DummyStage Asset Deployment** feature — must land first or same execution
- **DiaApplicationFlow** — `Module`, `StartResult`, `StopResult`
- **DiaCore/CRC** — `StringCRC`
- **DiaLogger** — `DIA_LOG_INFO`, `DIA_LOG_ERROR`

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — `kStageId` is `StringCRC("DummyStage")` |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | App | Three ProcessingUnits | Compliant — DummyLevel still on SimPU |
| SD-005 (CT AppFlow) | System | Stage-specific from `.diastage` | Compliant — module declared in `dummy_stage.diaapp` |
| SD-007 (CT AppFlow) | System | Log at DoStart/DoStop | Compliant (DoStart entry logged once; DoStop logged) |
| SD-013 (DiaAppFlow) | System | DoStart returns StartResult | Compliant — `kLoading` is now real, not a placeholder; `kFailed` surfaced from load failures |
| SD-006 (Async) | System | Stage loads are transition-driven; stage Modules only poll | This feature **is** that pattern's reference implementation |
| SD-011 (Async) | System | Cross-PU access via thread-safe static accessor; no cross-PU method invocations | Compliant — uses `GetStatic()`, no `ModuleRef<AssetServiceModule>` |
| SD-012 (Async) | System | `IsStageLoadComplete` + `GetStageLoadState` distinguish in-flight from failed | Compliant — uses `GetStageLoadState` for the failure path |
| SD-002 (Async) | System | Two-phase texture load | Compliant by transitivity — sibling features handle this |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Pattern | Why no `ModuleRef<AssetServiceModule>`? | `ModuleRef::Get()` searches only the owner's PU (`ModuleRefV2.h:85–89`). DummyLevel on SimPU + AssetService on MainPU = lookup returns `nullptr`. The `GetStatic()` accessor is the established cross-PU read pattern — same shape as `KernelModule::GetStaticTextureHandler` already used in this file at line 102. |
| 2 | Pattern | Why doesn't DummyLevel call `RequestStageLoad`? | `AssetRuntime::AssertOwnerThread()` (`AssetRuntime.cpp:691`) forbids non-Main threads from mutating runtime state. `RequestStageLoad` from SimPU would assert. Per system SD-006, the load is auto-triggered by the AssetService stage-transition watcher (sibling feature) — DummyLevel is purely a reader. |
| 3 | Visibility | Will the user actually see a "loading" state for 3 tiny PNGs? | Probably not — decode + upload is microseconds. Acceptance criterion 7 (with injected 100ms sleep) is the deterministic check that the async path is real. Without artificial latency, `DoStart` may return `kReady` on the first or second poll. That's correct behaviour. |
| 4 | Re-entry | If DummyStage is exited and re-entered, does the polling work correctly? | Yes. `mLoadEntryLogged` is reset in `DoStop`. On re-entry, `DoStart` polls again. The AssetService stage-transition watcher detects the transition and re-issues `RequestStageLoad` (which the runtime dedups via cache hits — already-loaded textures fire `OnLoadComplete` synchronously per SD-009). |
| 5 | Logging | Why log only on the first `DoStart` call instead of every poll? | `DoStart` is called every frame while `kLoading`. Logging every call would spam. Single entry log + the eventual "ready" / "failed" terminal log gives the same trace coverage with no noise. |
| 6 | Coupling | DummyLevel directly references `AssetServiceModule::GetStatic()` and `AssetServiceModule::StageLoadState` — does this couple every stage to AssetServiceModule's class type? | Yes, but this is the existing precedent (`KernelModule::GetStaticTextureHandler` is referenced from this same file at line 102). Stages that consume assets need an asset accessor; that's `AssetServiceModule`. Acceptable coupling — and contained to one symbol per stage. |
| 7 | Texture lookup | Do we still need `KernelModule::GetStaticTextureHandler()` at draw time? | Yes — out of scope for this feature. The draw-time lookup is a separate cross-PU concern. This feature only changes load-time behaviour. |
| 8 | Manifest | Why is `AssetService` NOT added to `DummyLevel`'s `dependencies` array? | The `dependencies` field controls module-init ordering within a PU. Cross-PU dependencies aren't supported by the manifest dependency system (verified during system spec design). They're also not needed: `AssetService` is `"stages": ["all"]` so it's already started before any stage-scoped module begins polling. |
| 9 | Failure visibility | What does the user see when `kFailed` is returned? | The framework treats `kFailed` per `start_timeout_ms` / framework policy (likely a hard stop or fallback to a previous stage). UX is out of scope for this feature — covered in the AssetService Stage-Transition Watcher's failure-state semantics. |
| 10 | Pattern documentation | Where does the canonical pattern (other stage authors will copy) get documented? | The system spec (SD-006) documents the rule. This feature's "Implementation Sketch" section is the working reference. Future stages copy `DummyLevelModule::DoStart` verbatim, swapping the `StringCRC` for their own stage ID. |

## Open Questions

None — Option A pattern locked at system-spec level (SD-006 / SD-011 / SD-012). Implementation details all resolve to the AssetService Stage-Transition Watcher feature.

## Status

`Approved` — 2026-05-17
