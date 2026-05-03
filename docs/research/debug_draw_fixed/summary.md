# Research Summary — Debug Draw for Fixed/Registered Objects

**Session folder:** docs/research/debug_draw_fixed/  
**Date:** 2026-05-03

---

## One-Line Answer

Register fixed-topology objects once at init; the render thread owns the resulting primitive buffer permanently and draws directly from it each frame — never touching FrameData — while dynamic overlays (selection highlighting, etc.) continue through the existing IVisualDebugger / FrameData path unchanged.

---

## Journey

1. **Explored:** Mapped the full problem space — redundant per-frame primitive emission for static structures (grids, quadtrees), the FrameData overflow problem for large structures (100×100 grid = 40 000 line primitives), renderer variety (default vs selected-cell vs heatmap), thread boundary ownership, and the future non-debug promotion path the user flagged as likely.

2. **Ideated:** 9 candidates generated initially, spanning caching strategies, renderer strategies, parallel registries, CRTP mixins, and a full command-stream model. Design discussion revealed the original axis (caching strategy) was wrong — the real question is whether large primitive sets ever enter FrameData at all. Candidates collapsed to 3 composable pieces (A, B, C).

3. **Evaluated:** A+B scored highest (4.20). A alone is a valid V1. C (DebugDrawGroup for multi-instance naming) deferred — problem not yet felt. Full mid-session thread handoff ruled out as over-engineered for init-time-only registration.

4. **Chose:** A+B confirmed. Init-time registration enforced by assert. Thread handoff via double-buffer pointer swap at frame boundary. Dynamic overlays unchanged. DebugDrawGroup deferred.

---

## Chosen Work Item

**Name:** fixed-draw-layer  
**Home module:** DiaVisualDebugger (new feature alongside existing IVisualDebugger stack)  
**Suggested spec type:** Feature  
**Estimated size:** M

---

## Key Insights from Exploration

- **FrameData is the wrong target for large static structures.** A 100×100 grid produces 40 000 line primitives — 40× the default DebugFrameData capacity. The fixed-draw path bypasses FrameData entirely; the render thread draws directly from its owned buffer.
- **The layer toggle IS the stream.** No per-frame token needed from sim to render. `EnableLayer()` / `DisableLayer()` on the existing system is sufficient. The sim thread has zero per-frame involvement after registration.
- **Topology and state are always separate layers.** Fixed buffer holds cell edges / bone lines / mesh wireframe (never changes). Dynamic overlay holds selection highlight, current-frame state (7 primitives, FrameData, per frame). They compose by running simultaneously, neither aware of the other.
- **Init-time registration eliminates thread complexity.** Restricting `RegisterFixed()` to before the render thread starts removes the need for any synchronisation on the hot path. Enforced by `DIA_ASSERT(!mRegistrationLocked)`. Mid-session registration deferred.
- **Two concrete consumers validate the design.** `SpatialStructureDrawer` (geometry2d) and `RigRestPoseDrawer` (rig2d) are both ideal fixed-draw candidates — topology never changes. Both are already Approved in the DiaVisualDebugger system; they will be amended to use the fixed-draw path once this feature ships.
- **Non-debug promotion is structural, not a retrofit.** `IObjectRenderer::BuildPrimitives()` has no inherent debug coupling — same interface works whether output goes to a debug primitive buffer or a vertex buffer for real static geometry. The path is reserved by design.

---

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Re-emit with large DebugFrameData capacity | Doesn't eliminate the problem, just raises the ceiling. Still 40 000 writes per frame |
| CachedVisualDebugger CRTP (C4) | Recomputation saved but primitives still flow through FrameData. Good as internal impl detail only |
| IRegisteredDrawObject with FrameData blit (C2 original) | Same overflow problem; render thread still holds sim-side reference — dangling reference risk |
| Full command-stream with per-frame tokens (C7) | Correct but over-engineered for init-time-only registration; revisit if mid-session registration needed |
| DebugDrawGroup (C8/Candidate C) | Good idea, deferred — multiple-instance naming not yet a felt problem |
| DebugDrawRegistrar parallel system (C3) | Fights existing layer stack; absorbed by Candidate A |

---

## Backlog Actions Added

- `docs/BACKLOG.md` — fixed-draw-layer feature spec (new work)
- `docs/BACKLOG.md` — migrate `RigRestPoseDrawer` once feature ships
- `docs/BACKLOG.md` — migrate `SpatialStructureDrawer` family once feature ships

---

## References

- docs/research/debug_draw_fixed/explore.md
- docs/research/debug_draw_fixed/ideate.md
- docs/research/debug_draw_fixed/evaluate.md
- docs/research/debug_draw_fixed/choose.md
