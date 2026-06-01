**Spec:** N/A (bug fix / infrastructure improvement)
**Status:** Done

# Render Fence — Fix Capture Timing for Fast-Completing Test Stages

## Problem

Test stages that complete on their first logic frame (e.g. Geometry2DTestStage) call `DIA_CAPTURE` immediately. The capture request arrives in CaptureManager's pending queue, but the render thread hasn't yet presented the frame containing the test's visual output — due to the double-buffered FrameStream and threading lag. The resulting screenshot is blank or shows the previous stage's content.

## Solution

Introduce a **RenderToSim feedback FrameStream** that the render thread writes after each `RenderFrame()` call. `TestStageModuleBase` reads this stream and defers `DIA_CAPTURE` until the fence confirms the render thread has presented the frame containing the test's draw data.

## Data Flow

```
Sim Thread (SimPU)                              Render Thread (RenderPU)
──────────────────                              ──────────────────────────
TestStage::OnUpdate()
  ├─ Logic passes → ReportPassed()
  │   └─ mResolved = true
  │   └─ mAwaitingCapture = true
  │   └─ mCaptureFrameTarget = renderFence.lastSeen + 1
  │   └─ Does NOT call DIA_CAPTURE
  │                                             RenderModule::DoUpdate()
  │                                               ├─ cm->RenderTick()
  │                                               ├─ FetchLatest("SimToRender") → frame data
  │                                               ├─ RenderFrame(frame)
  │                                               └─ mFenceOutput.Write({++mPresentCount})
  │
TestStage::DoUpdate() [next frame]
  ├─ Reads "RenderToSim" fence
  ├─ fence.presentedFrame >= mCaptureFrameTarget?
  │   └─ YES → DIA_CAPTURE(stageName, result)
  │   └─ mAwaitingCapture = false
```

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `RenderFence` struct in DiaGraphics | Compiles | Done | haiku | Tiny POD: `{ uint64_t presentedFrame; }` in `Dia/DiaGraphics/Frame/RenderFence.h` |
| 2 | Add `StreamWriter<RenderFence>` to RenderModule | Compiles | Done | sonnet | Write fence after `RenderFrame()` in `DoUpdate()`. Connect in `OnConnectStreams()`. Increment counter each frame. |
| 3 | Declare "RenderToSim" stream in `cluiche_main.diaapp` manifest | App loads | Done | haiku | `{ "id": "RenderToSim", "kind": "FrameStream", "payload_type": "RenderFence", "from": "RenderPU", "to": "SimPU" }` |
| 4 | Add channel binding for RenderModule in manifest | App loads | Done | haiku | `{ "id": "RenderToSim", "role": "writes" }` on RenderModule's channels |
| 5 | Add `StreamReader<RenderFence>` to TestStageModuleBase | Compiles | Done | sonnet | New member, connect in `OnConnectStreams()`. Add channel binding `{ "id": "RenderToSim", "role": "reads" }` to all test stage modules in their `.diaapp` manifests. |
| 6 | Implement deferred-capture logic in TestStageModuleBase | `dia run googletest` passes | Done | sonnet | New state: `mAwaitingCapture`, `mCaptureFrameTarget`, `mCaptureResult` (passed/failed). `ReportPassed/Failed` sets the state instead of calling `DIA_CAPTURE`. `DoUpdate()` checks fence after `OnUpdate()` — fires capture when fence >= target. `DoStop()` fires capture as fallback if still awaiting. |
| 7 | Verify Geometry2DTestStage capture timing | `dia run cluichetest` — capture PNG shows shapes | Done | sonnet | Run Geometry2DTestStage, confirm the capture file is non-blank. |
| 8 | Verify RigidBody2DTestStage still works | `dia run cluichetest` — passes | Done | haiku | Multi-frame test — should be unaffected but verify no regression. |

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| FrameStream (not EventStream) | Sim only needs "latest presented frame count" — no queue, no overflow, lock-free read |
| Fence lives in DiaGraphics | It's render-domain metadata; keeps DiaApplicationFlow payload-agnostic |
| TestStageModuleBase owns the reader | All test stages get the fix automatically |
| `mResolved = true` immediately | Budget timeout never fires; only the capture is deferred |
| Fallback capture in `DoStop()` | If stage is force-stopped (automation skip), capture what's available rather than losing it |
| Target = lastSeen + 1 | Ensures at least one full render pass after the test wrote its draw commands to SimToRender |

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaGraphics/Frame/RenderFence.h` | **NEW** — struct definition |
| `Cluiche/CluicheGameBaseline/Modules/RenderModule.h` | Add `StreamWriter<RenderFence>`, frame counter |
| `Cluiche/CluicheGameBaseline/Modules/RenderModule.cpp` | Write fence after `RenderFrame()`, connect stream |
| `Cluiche/CluicheTest/Modules/TestStages/TestStageModuleBase.h` | Add `StreamReader<RenderFence>`, deferred-capture state |
| `Cluiche/CluicheTest/Modules/TestStages/TestStageModuleBase.cpp` | Deferred-capture logic in DoUpdate/DoStop, refactored ReportPassed/Failed |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Add "RenderToSim" stream declaration + RenderModule channel binding |
| `Cluiche/Assets/Stages/Geometry2DTestStage/misc/ApplicationFlow/geometry2d_test_stage.diaapp` | Add channel binding for Geometry2DTestStageModule |
| `Cluiche/Assets/Stages/RigidBody2DTestStage/misc/ApplicationFlow/rigidbody2d_stage.diaapp` | Add channel binding (if test module reads fence) |
| `Cluiche/Assets/Stages/AssetRuntimeTestStage/misc/ApplicationFlow/asset_runtime_stage.diaapp` | Add channel binding |
| `Dia/DiaGraphics/DiaGraphics.vcxproj` + `.vcxproj.filters` | Add RenderFence.h |

## Open Questions

1. **Should non-test modules also use this fence?** (e.g. future game screenshots) — Probably yes eventually, but scope this to TestStageModuleBase for now.
2. **Should the fence carry a timestamp?** — Not needed for this use case (frame count is sufficient), but could be added later for interpolation.
