---
module_id: dia.simtime
display_name: DiaSimTime
path: Dia/DiaSimTime
parent_module: dia.applicationflow
layer: foundation/application
version: "1.0"
status: Active
dependencies:
  - dia.core
  - dia.applicationflow
  - dia.streams
  - dia.observation
  - dia.aibudget  # ISimTimeBudgetedSystem extends Dia::AIBudget::IAIBudgetedSystem
public_api:
  headers:
    - DiaSimTime/DiaMainTime.h
    - DiaSimTime/DiaRenderTime.h
  namespaces:
    - Dia::SimTime::
  entry_points:
    - DiaMainTime
    - DiaRenderTime
responsibilities:
  - DiaMainTime: trivial MainPU presence module (MainTimeContext is computed directly by ProcessingUnit; no active publishing needed)
  - DiaRenderTime: sole reader of the sim-time FrameStream on RenderPU; pushes RenderTimeContext into the owning ProcessingUnit each tick
not_responsibilities:
  - Does not publish SimTimeContext itself — that is DiaSimTimeModule (a later phase, not yet built)
  - Does not drive vsync or swap-chain frame pacing
---
# DiaSimTime

Presence and cross-PU handoff modules for the Dia engine's sim/render/main time domains.

`DiaMainTime` is a trivial `MainModule` on the MainPU. `ProcessingUnit::Update()` already computes and caches `MainTimeContext` directly from wall-clock time for `kMain`/`kAny`-affinity PUs, so this module has nothing further to compute — it exists for manifest completeness and symmetry with `DiaRenderTime`.

`DiaRenderTime` is a `RenderModule` on the RenderPU. It is the sole reader of the `"SimTime"` FrameStream (published by `DiaSimTimeModule`, a later phase) and the sole producer of `RenderTimeContext`, which it pushes into the owning `ProcessingUnit` via `SetRenderTimeContext()`. Sibling `RenderModule`s read whatever was pushed last tick via `GetRenderTimeContext()` — one-tick-stale by design, with no manifest-order dependency. Until `DiaSimTimeModule` exists, `DiaRenderTime` tolerates the absence of a publisher gracefully (no crash, no pushed context) rather than treating it as an error.
