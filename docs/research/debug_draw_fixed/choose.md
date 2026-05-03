# Research: Choice — Debug Draw for Fixed/Registered Objects

**Date:** 2026-05-03  
**Chosen candidate:** A + B — Render-Thread Fixed Buffer + IObjectRenderer

---

## Rationale

All three original requirements are met:
- **Register once** — sim thread calls `RegisterFixed()` at init with a source object and renderer; render thread owns the resulting buffer permanently
- **Toggle is the stream** — `EnableLayer()` / `DisableLayer()` on the existing layer system is the only per-frame signal; no token stream, no per-frame sim involvement
- **Custom renderers** — `IObjectRenderer` is a strategy passed at registration; multiple renderers for the same object type register under different layer names giving independent toggle

The dynamic overlay (selected cell, highlighted neighbours, etc.) continues to use the existing `IVisualDebugger` / FrameData path unchanged. The two paths are fully independent and compose by running simultaneously.

The design scales cleanly to future fixed-topology object types (navmesh, static collision geometry, rest-pose skeleton) — each needs only a new `IObjectRenderer` implementation.

The non-debug promotion path is clean: `IObjectRenderer::BuildPrimitives()` is the same signature whether output goes to a debug primitive buffer or a vertex buffer for real static geometry rendering.

---

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| A alone | No renderer flexibility — would need to be retrofitted; adding B at design time costs little |
| A + B + C (DebugDrawGroup) | Multi-instance naming problem not yet felt; add as follow-on feature spec when needed |
| Re-emit large capacity | Doesn't solve FrameData overflow for large structures; deferred CPU cost rather than eliminated |
| Full mid-session thread handoff | Over-engineered for init-time-only registration; revisit if mid-session registration is needed |

---

## Pre-Spec Commitments

1. **Registration locked at init.** `LockRegistration()` called once when the render thread starts. Any `RegisterFixed()` after that point hits `DIA_ASSERT`. No mid-session registration in V1.

2. **Fixed buffers never touch DebugFrameData.** The render thread draws fixed buffers directly. FrameData capacity is reserved for dynamic overlays and existing draw classes.

3. **`IObjectRenderer::BuildPrimitives()` runs on the sim thread at registration and at `Invalidate()` only.** Render thread never calls back into the source object after registration.

4. **Dynamic overlays use the existing IVisualDebugger / FrameData path unchanged.** No new mechanism for small per-frame draws — just submit normally.

5. **Thread handoff mechanism resolved in spec before implementation.** Recommendation: double-buffer pointer swap at the sim/render frame boundary. No mutex on the hot path.

6. **`DebugDrawGroup` (Candidate C) deferred** to a follow-on feature spec when multiple instances of the same type make manual naming painful.

7. **`#ifdef DIA_DEBUG` guards apply.** All fixed-draw registration and buffer management is debug-only. Zero overhead in Release.

8. **`DebugColourPalette` binding applies to all `IObjectRenderer` implementations.** No ad-hoc colour literals.

9. **Per-registration buffer capacity is caller-specified.** No global fixed capacity — large grids get a large buffer, small structures get a small one. A system-level total budget is tracked and logged via DiaLogger on overflow.

---

## Next Step

Run `/spec-system` with this choice as input, or add a feature spec to the existing `DiaVisualDebugger` system spec.

**Suggested home:** New feature spec under `docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md`  
**Suggested parent system:** `docs/specs/systems/dia/diadebug.md`  
**Estimated size:** M
