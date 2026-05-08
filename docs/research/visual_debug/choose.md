# Research: Choice — Visual Debuggers

**Date:** 2026-05-02  
**Chosen candidate:** Full visual debug system — stack-based draw classes + DebugLayerManager

---

## Rationale

All candidates collapsed cleanly into a single architectural pattern: small focused draw classes, each registered as a named layer with a central `DebugLayerManager`. The manager IS the options system — enable/disable, draw order, world scale, multiple instances, and colour palette are all handled at registration time rather than inside each debugger. No `VisualDebuggerOptions` struct needed; the `StringCRC` naming convention (`physics.shapes`, `physics.velocity`) carries the hierarchy.

The only two things that required architecture beyond "register a draw class" were rewind (deferred to `DiaReplay`) and entity picking (deferred to scene editor). Everything else fit the stack model exactly. This was the core finding of the research.

## What Was Ruled Out

| Candidate | Reason not chosen as standalone |
|-----------|--------------------------------|
| VisualDebuggerOptions struct | Replaced entirely by the stack registration model — flags become independent draw classes |
| Minimal Debug HUD | Superseded by DiaVisualDebuggerConsole (Candidate 9) |
| Entity picking (Candidate 10) | Deferred to scene editor research; seams reserved in DebugLayerManager and DebugPrimitive |
| Debug state rewind (Candidate 11) | Deferred to DiaReplay / simulation capture research; seam: DebugFrameData stays trivially copyable |

## Pre-Spec Commitments

All cross-cutting decisions from ideate.md are binding inputs to the spec:

- `#ifdef DIA_DEBUG` guards throughout; nothing links in Release
- Draw order via integer priority at registration; canonical tiers defined
- First-come-first-served budget in priority order; `DroppedCount()` logged on overflow
- Global `debugScale` float on `DebugLayerManager`
- `DIA_ASSERT` on layer name collision at registration; canonical names in `DebugLayerNames.h`
- `DebugColourPalette` with 9 canonical colour meanings; all draw classes use it
- ImGui + imgui-sfml approved as new external dependency
- Test strategy: 7 test types per draw class (toggle isolation, all-on min count, disabled=nothing, manager routing, no mutation, options round-trip, overflow)
- Hot reload deferred; documented as caller responsibility in API
- `DebugFrameData` must remain trivially copyable (rewind seam)
- `entityId` field reserved on `DebugPrimitive` (picking seam)
- `debug.pick` registered as no-op DiaAPI command (picking seam)

## Natural Build Order

1. Candidate 7 — Configurable `DebugFrameData` budget (DiaGraphics, S)
2. Candidate 6 — `TextPrimitive` world-space text (DiaGraphics, S)
3. Candidate 1 — `DebugLayerManager` + `IVisualDebugger` + `DebugColourPalette` + `DebugLayerNames` (DiaVisualDebugger, M)
4. Candidate 2 — Decompose existing debuggers into stacks of focused draw classes (DiaRig2DVisualDebugger, DiaRigidBody2DVisualDebugger, DiaSoftBody2DVisualDebugger, S)
5. Candidate 3 — `DiaIK2DVisualDebugger` stack (S)
6. Candidate 4 — `DiaGeometry2DVisualDebugger` stack (M)
7. Candidate 8 — Editor layer panel plugin (CluicheEditor, S)
8. Candidate 9 — `DiaVisualDebuggerConsole` ImGui console (DiaVisualDebuggerConsole, M)
9. Candidate 5 — `DiaAnimation2DVisualDebugger` stack (M, blocked on DiaAnimation2D)

## Next Step

Run `/spec-system` with this research as context.  
Suggested system name: **DiaVisualDebugger**  
Parent application: **Dia**  
Scope: DebugLayerManager, IVisualDebugger stack pattern, DebugColourPalette, DebugLayerNames, TextPrimitive, configurable budget, all per-module draw class stacks, DiaVisualDebuggerConsole, editor layer panel.
