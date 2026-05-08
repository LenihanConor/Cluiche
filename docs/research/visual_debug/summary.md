# Research Summary — Visual Debuggers

**Session folder:** docs/research/visual_debug/  
**Date:** 2026-05-02

---

## One-Line Answer

Replace the current ad-hoc per-debugger pattern with a stack of small focused draw classes registered by name with a central `DebugLayerManager` — the manager becomes the options system, the toggle surface, and the editor bridge simultaneously.

---

## Journey

1. **Explored:** Mapped the full visual debug problem space across 6 modules lacking debuggers, the complete editor stack (CluicheEditor → DiaEditor → DiaDebugServer → DiaAPI → Protobuf), and the editor-vs-in-game strategy. Key finding: the editor already has a full command/subscription bridge; a `DebugLayerManager` registered as a DiaAPI command target unifies both control surfaces through the same code path.

2. **Ideated:** 11 candidates generated covering infrastructure (manager, budget, text), per-module debuggers (IK2D, Geometry2D, Animation2D), configurability of existing debuggers, editor panel, DiaVisualDebuggerConsole (ImGui), entity picking, and rewind. Scope ranged from S to XL. 9 cross-cutting decisions resolved (Release policy, draw order, budget, world scale, colour palette, multiple instances, hot reload, testing, ImGui).

3. **Evaluated:** Options pass scored highest (4.75) for immediate value; configurable budget (4.25) and DiaIK2DVisualDebugger (4.20) close behind. DiaVisualDebuggerConsole scored lower on cost/risk but highest on game value.

4. **Chose:** All candidates collapsed into one architectural insight — stack-based registration — which made the choice a system rather than a single feature. Only rewind and entity picking fell outside the pattern, and both were correctly deferred.

---

## Chosen Work Item

**Name:** DiaVisualDebugger — Visual Debug System  
**Home module:** New `DiaVisualDebugger` module + extensions to `DiaGraphics`, `DiaSFML`, `DiaRig2DVisualDebugger`, `DiaRigidBody2DVisualDebugger`, `DiaSoftBody2DVisualDebugger`  
**Suggested spec type:** System  
**Estimated size:** M × 9 features (total ~L–XL across all features)

---

## Key Insights from Exploration

- **The stack IS the options system.** Decomposing each debugger into focused draw classes (one per concern) registered with the manager entirely replaces `VisualDebuggerOptions` boolean flags. No struct, no sub-layer protocol — just named layers with a naming convention.
- **Editor and in-game share one code path.** In-game keypresses and editor panel checkboxes both call the same DiaAPI commands (`debug.layer.enable/disable`). The manager doesn't know or care which surface triggered it.
- **In-game overlay = spatial/real-time. Editor panel = structured/queryable.** Physics shapes, bones, and IK chains belong in the in-game overlay (world coordinates, `FrameData`). Property tables, state graphs, metric charts, and timelines belong in the editor (CEF, React, VisJS).
- **Only two things escaped the pattern.** Rewind requires a simulation-state recording system (`DiaReplay`), not a primitive snapshot. Entity picking requires screen→world transform, spatial query, and entity ID convention — all owned by the future scene editor. Both are deferred with explicit seams.
- **Seams reserved now:** `entityId` field on `DebugPrimitive`, `debug.pick` no-op command, `ScreenToWorld` hook, `DebugFrameData` trivially copyable constraint.
- **`DebugColourPalette` enforces a shared visual language.** 9 canonical colour meanings across all draw classes make simultaneous multi-debugger views readable.

---

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| VisualDebuggerOptions struct | Superseded by stack registration — flags become independent draw classes |
| Minimal Debug HUD | Superseded by DiaVisualDebuggerConsole (Candidate 9) |
| Entity picking | Deferred to scene editor — requires entity ID convention, camera API, property inspector |
| Debug state rewind | Deferred to DiaReplay — wrong layer; should capture simulation state not primitives |

---

## References

- docs/research/visual_debug/explore.md
- docs/research/visual_debug/ideate.md
- docs/research/visual_debug/evaluate.md
- docs/research/visual_debug/choose.md
