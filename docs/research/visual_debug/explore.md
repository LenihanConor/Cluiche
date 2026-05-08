# Research: Explore — Visual Debuggers

**Session date:** 2026-05-02  
**Folder:** docs/research/visual_debug/

---

## Problem Space Overview

Visual debuggers are read-only rendering overlays that let developers see simulation state — physics bodies, bone hierarchies, spatial structures, AI state, etc. — drawn on top of the running game without modifying the simulation itself. In Cluiche two are already shipping (`DiaRig2DVisualDebugger`, `DiaRigidBody2DVisualDebugger`) and a third (`DiaSoftBody2DVisualDebugger`) is designed and ready to build. The pattern is mature: separate static library project per debugger, a single `Draw(const WorldData&, FrameData&)` entry point, and a `VisualDebuggerOptions` struct for live configuration.

The research question is broader than "what debuggers are missing." It also asks whether there should be shared infrastructure — a manager that owns all debuggers, provides layer toggles, handles enable/disable uniformly, and exposes a consistent interface to both in-game overlays and the CluicheEditor CEF panel. It also asks harder questions: which debug features belong in the editor vs. the in-game overlay, should entity picking exist, and should debug state capture and rewind exist.

The Cluiche engine has a complete editor stack: CluicheEditor (Win32 executable) drives DiaEditor (pure library), which communicates to the running game over WebSocket via DiaDebugServer, using Protobuf-framed messages defined in DiaDebugProtocol. The editor can subscribe to live game state, send DiaAPI commands, and receive streamed responses. A `DebugLayerManager` registered as a DiaAPI command target could expose layer toggles to both in-game keypresses and editor panel checkboxes through the same code path. The established `FrameData` / `DebugPrimitive` primitive system has 7 draw types and a 1024-element buffer per frame. Any infrastructure work must fit into the ProcessingUnit/Phase/Module lifecycle (PD-002) and avoid STL in public APIs (PD-004).

---

## Existing Debuggers

| Module | Status | What it draws |
|--------|--------|---------------|
| DiaRig2DVisualDebugger | Done | Bone hierarchy, joints (colour by type), optional bone name labels, direction arrows |
| DiaRigidBody2DVisualDebugger | Done | Collision shapes (colour by body type), velocity arrows, contact normals, constraint anchors |
| DiaSoftBody2DVisualDebugger | Designed, not built | Particles (colour by state), constraint lines (colour by type), anchor links, velocity arrows with cap marker |

---

## Modules That Do Not Yet Have a Visual Debugger

| Module | State | Debug-worthy data |
|--------|-------|-------------------|
| DiaAnimation2D | Spec approved, not built | Spring chain positions, clip playback cursor, blend stack weights, IK target influence |
| DiaIK2D | Implemented | End effector, target, per-joint limits, solve iteration convergence |
| DiaGeometry2D | Done | 13 primitive shapes, spatial grid cells, quadtree partitions, BVH nodes, intersection contacts |
| DiaInput | Done | Active keys/buttons, action bindings, recorded input event timeline |
| DiaApplication | Done | Phase transitions, frame time per ProcessingUnit, module update order |
| DiaStateMachine | Done | Active state, transition graph, current transition progress |

---

## Existing Approaches (Industry)

- **Per-system debug draw functions** — each system pushes primitives to a global debug draw queue; no separate debugger class needed (Unity DrawGizmos, Godot debug draw calls). Simple but hard to toggle per-system.
- **Debug draw manager / layer registry** — a central manager owns named "layers" (e.g. "physics", "bones"); each layer can be toggled independently. Used by Unreal (UE `SHOWFLAG_ALWAYS`) and PhysX PVD.
- **External debug viewer** — separate process (PhysX Visual Debugger, Recast NavMesh debug viewer) connected via socket. Cluiche already has DiaDebugServer/DiaDebugProtocol infrastructure for this.
- **In-game overlay toggle** — runtime keypress or console command toggles layers. Cheap, requires no editor.
- **Editor panel control** — editor UI panel with checkboxes per layer; can also show per-entity property values. Requires editor integration.
- **Entity picking** — user clicks on the game view; a ray-cast maps screen coords to a simulation entity; the debugger highlights that entity and shows its properties. Requires coordinate transform and spatial query.
- **Debug state rewind** — ring buffer records N frames of debug primitive snapshots; user can scrub back in time. Expensive in memory; more common in dedicated physics/animation tools than in-game overlays.
- **Zero-cost in Release** — all debug draw calls wrapped in `#ifdef DEBUG` or compile-time flags; zero overhead in Release builds.

---

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Debugger granularity | Per-module vs. shared base class | Current pattern: per-module standalone class. Shared base would add coupling to a new DiaVisualDebugger module. |
| Manager / registry | None (current) vs. central DebugLayerManager | Manager enables global toggle, editor panel binding, ordered draw calls |
| Toggle surface | In-game keypress vs. editor UI vs. both | Both is ideal; requires manager to be addressable from editor |
| Editor integration | In-game overlay only vs. editor sidebar panel | Editor panel needs DiaEditor CEF bridge; in-game overlay works today |
| Entity picking | None vs. screen-space ray + AABB test | Requires coordinate transform (screen → world) and spatial query; significant effort |
| Rewind | None vs. ring-buffer snapshot vs. full replay | Memory cost is high; partial solution: pause + single-step is much cheaper |
| Primitive budget | Current 1024 hard cap | Large scenes (many bodies) will overflow; budget should be configurable |
| Coloring scheme | Ad-hoc per debugger vs. shared colour palette | Shared palette improves visual consistency; could live in a DiaVisualDebugger shared header |
| Text / labels | Not supported today | Adding screen-space text draw to DebugPrimitive would unlock bone names, entity IDs, property values |

---

## Known Tradeoffs

- **Manager adds coupling.** A `DebugLayerManager` must live somewhere (DiaApplication module? New DiaVisualDebugger module?). Every visual debugger module that registers with it gains a dependency on that module.
- **Separate vcxproj per debugger scales poorly.** At 6+ debuggers the number of projects grows; a shared `DiaVisualDebugger` static lib with all debuggers might be simpler, but it would force a dependency on every system's data types simultaneously, creating a fat dependency.
- **Editor and in-game toggle share the same code path via DiaAPI.** A `DebugLayerManager` that registers DiaAPI commands (`debug.layer.enable`, `debug.layer.disable`, `debug.layer.list`) can be driven by both an in-game keypress handler and an editor panel checkbox — the editor sends a `MessageType::COMMAND_REQUEST` over WebSocket which DiaDebugServer forwards to `CommandRegistry::Execute()`. This unifies both surfaces without duplicating logic.
- **Picking requires coordinate transform infrastructure** that does not exist today. It is a large dependency chain (screen-space → world-space, hit-test on spatial structure).
- **Rewind requires capturing debug-render state, not simulation state.** A ring buffer of `DebugFrameData` snapshots (primitives only) is feasible, but the 1024 primitive cap means large scenes lose fidelity or you need a larger capture buffer.
- **Text rendering is not in the current `DebugPrimitive` tagged union.** Adding it would require DiaGraphics changes and a font resource.

---

## Known Pitfalls (C++ / game engine context)

- Registering debugger instances with a manager via raw pointer — lifetime management becomes fragile if modules are hot-reloaded.
- `DebugPrimitive` buffer overflow (current cap 1024) silently drops primitives; there is no overflow warning today.
- Enabling all debuggers simultaneously in a complex scene will push draw primitive count well past the cap.
- Per-entity filtering (show only body ID 42) requires identity tagging on primitives, which the current `DebugPrimitive` union does not support.
- Thread safety: visual debuggers are called from the sim thread (after physics step) but write into `FrameData` which is read by the render thread. The existing `FrameData` double-buffer/swap mechanism handles this, but any manager state (enable flags) must also be thread-safe.
- Adding text labels to bones/bodies creates a dependency on a font resource system that does not exist.

---

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaRig2DVisualDebugger | Reference implementation; defines naming/API pattern |
| DiaRigidBody2DVisualDebugger | Reference implementation; `VisualDebuggerOptions` pattern |
| DiaGraphics / DebugFrameData | Primitive submission API — `RequestDraw(...)` with 7 types |
| DiaDebugServer + DiaDebugProtocol | Full editor-game bridge: WebSocket transport, Protobuf framing, command dispatch via DiaAPI::CommandRegistry |
| DiaGeometry2D | 13 shape types + spatial structures, all renderable with existing primitives |
| DiaIK2D | End-effector chains — renderable with existing ray/line/circle primitives |
| DiaStateMachine | State graph — renderable as labelled nodes/edges (requires text or external tool) |
| DiaApplicationEditor | 15 approved features including `live-phase-visualization` (pause/resume). A new `DebugLayerPanel` plugin would sit alongside these, subscribing to `debug.layer.state` data and sending `debug.layer.enable/disable` commands. |
| DiaInput / InputRecorder | Records input sequences; could overlay event timeline on game view |
| CluicheTest | 3 ProcessingUnits (Main, Render, Sim); frame-time overlay would help profile all three |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Layer names in a manager must use `StringCRC`, not `std::string` |
| PD-002 ProcessingUnit/Phase/Module | A `DebugLayerManager` should be a `Module` inside a `Phase`; its update is called each frame in the correct thread context |
| PD-003 Component system | Per-entity debug filtering (select one body/bone chain) should use `IComponent`-based entity IDs, not ad-hoc integers |
| PD-004 No STL in public APIs | Manager's layer registry must use `DiaCore::HashTable` or `DynamicArrayC`, not `std::map`/`std::vector` |
| PD-007 C++20 | Can use `std::span` for passing debugger lists; concepts for `IVisualDebugger` contract |

---

## Editor vs. In-Game Strategy

The full editor stack exists (CluicheEditor → DiaEditor → DiaDebugServer → DiaAPI → CommandRegistry). The question is which debug surfaces belong where.

### The Dividing Line

| Concern | In-Game Overlay | Editor Panel | Reason |
|---------|----------------|--------------|--------|
| Simulation-space overlays (physics shapes, bones, IK chains) | ✅ | ❌ | Drawn in world coordinates using FrameData; editor has no game camera |
| Layer toggle (enable/disable per debugger) | ✅ (keypress) | ✅ (checkbox) | Same DiaAPI command, two surfaces |
| Per-entity property values (body mass, velocity magnitude) | ❌ (no text today) | ✅ (table/inspector panel) | Editor can render rich text; in-game needs font system |
| Frame time / per-PU metrics | Optional overlay | ✅ (already streamed via core_metrics every 500ms) | Editor has it for free via DiaDebugServer broadcast |
| State machine graph (nodes + transitions) | ❌ (needs graph layout) | ✅ (VisJS graph) | Graph layout is a UI concern; editor uses VisJS |
| Phase transition visualization | ❌ | ✅ (DiaApplicationEditor live-phase-visualization) | Already designed in editor |
| Input event timeline | Optional overlay | ✅ (timeline chart) | Rich timeline belongs in editor; in-game can show "current key" |
| Rewind / scrubbing | ❌ (expensive) | ✅ (if implemented) | Pause+step in editor covers 80% of the use case |
| Entity picking / selection | ❌ (first-class in-game feature, large scope) | ✅ (click-to-select from editor, maps to entity ID) | Editor picking via ray-cast command; result highlights in both |

### Key Insight: Two Complementary Debug Surfaces

**In-game overlay** is for *spatial, real-time* information — things that only make sense when drawn in the world coordinate system alongside the game visuals (collision shapes, IK chains, bone hierarchies). It is always approximate and primitive-count limited.

**Editor panel** is for *structured, queryable* information — property tables, state graphs, timelines, metric charts. It is rich but not spatially registered to the game view.

A `DebugLayerManager` is the bridge: it controls which in-game overlays are active and exposes its state to the editor via a `debug.layer.state` subscription, so the editor panel reflects exactly what the in-game overlay is showing.

---

## Open Questions for Ideation

- Should all visual debuggers share a common `IVisualDebugger` interface (with `Draw(FrameData&)` and `SetEnabled(bool)`) so a manager can hold them polymorphically — or is the per-module standalone pattern sufficient?
- Where does a `DebugLayerManager` live: new `DiaVisualDebugger` module, inside `DiaGraphics`, or inside `DiaApplication`?
- Layer toggle commands should flow via DiaAPI (both in-game keypresses and editor panel send the same `debug.layer.enable/disable` command). Should the in-game keypress handler be built into the `DebugLayerManager` itself, or left to game code?
- Is the 1024 primitive cap a hard constraint going forward, or should it become configurable at manager construction?
- Should per-entity filtering be scoped to a future "picking" feature, or should debuggers accept an optional entity-ID filter parameter from day one?
- Is debug state rewind in scope at all, or should "pause + single-step" (already in DiaApplicationEditor) be sufficient?
- Should `DiaGeometry2DVisualDebugger` focus on individual shapes (spatial query results, intersection contacts) or on the spatial structure (grid cells, quadtree nodes)?
- Should frame-time overlay be a visual debugger or a separate `DiaProfiler` system?
