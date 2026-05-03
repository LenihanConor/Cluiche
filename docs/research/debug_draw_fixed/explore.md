# Research: Explore — Debug Draw for Fixed/Registered Objects

**Session date:** 2026-05-03  
**Folder:** docs/research/debug_draw_fixed/

---

## Problem Space Overview

The DiaVisualDebugger system currently calls `IVisualDebugger::Draw(FrameData&)` every frame. Each draw class iterates its source object (a physics world, skeleton, etc.) and emits `DebugPrimitive` values into `FrameData`. For simulation objects that mutate every frame (velocities, contact points, joint angles), this is exactly right — you want a fresh picture each tick.

For **spatially fixed structures** — uniform grids, hex grids, quadtrees, BVHs — the cell topology and boundary lines don't change between frames. A 64×64 spatial grid emits ~16 000 line primitives per frame even though nothing moved. This burns CPU time re-computing the same primitives and burns budget in `DebugFrameData` that other layers could have used. At a 60 Hz sim tick rate that is a 960 000 line-primitive redundant writes per second for a single grid.

The secondary problem is **rendering variety**. A default draw (all cells one colour) is useful for "is the grid there?" debugging. A richer draw (selected cell highlighted, neighbours a different colour, occupied cells distinct from empty ones) is essential when debugging spatial queries. These are not mutually exclusive; a developer may want to swap renderers at runtime or register a domain-specific renderer alongside a generic default. The current `IVisualDebugger` interface has no concept of swappable renderers — the draw logic is baked into the class.

The user also noted that this pattern may later apply to non-debug objects (e.g. static level geometry, background tilemaps). Designing the registration/renderer split with that promotion path in mind — without gold-plating it now — is a constraint on the architecture.

---

## Existing Approaches

- **Re-emit always (current model):** `IVisualDebugger::Draw()` emits every frame from live source state. Correct for dynamic objects; wasteful for static structures.
- **Dirty-flag invalidation:** The object caches its last-emitted primitive set. `Draw()` re-emits only when a dirty flag is set. Requires the object to know it's being debugged (coupling).
- **Persistent draw buffer per object:** Each registered object owns a small `DebugFrameData`-like buffer it fills once. The main renderer blits that buffer into the scene buffer each frame unless the layer is disabled. Persistent primitives survive until explicitly invalidated.
- **Pre-baked mesh / draw call:** Convert the debug structure to a VAO/index buffer once. Cheap to re-draw. Ties the design to a specific renderer backend — not appropriate for a backend-agnostic primitive system.
- **Renderer-strategy pattern (IObjectRenderer):** Separate *what to draw* (the registered object) from *how to draw it* (a renderer). Multiple named renderers registered per object; caller selects active renderer at runtime. Draw only happens when the renderer or the object signals it needs re-baking.
- **Copy-on-register snapshot:** Take a structural snapshot (copy the cell bounds or node AABBs) at registration time. If the object is truly immutable after registration, this snapshot never goes stale. The draw system owns the copy; the source object can be destroyed.
- **Command-stream approach:** Sim thread sends a lightweight "draw frame X" or "skip" token each frame. Render thread decides whether to flush its cached primitive buffer based on the token. No re-computation on sim side at all.

---

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Cache lifetime** | Per-frame (current) · Persistent until explicit invalidate · Auto-dirty on object mutation | Per-frame is safest; persistent requires an invalidation contract |
| **Primitive ownership** | Emitted into shared `FrameData` each frame · Owned per-object buffer blitted into `FrameData` | Per-object buffer enables frame-skip; shared buffer requires re-emit |
| **Renderer binding** | Baked into draw class · Swappable `IObjectRenderer` pointer · Multiple named renderers per object | Named renderers enable "default + custom" simultaneously or swappable at runtime |
| **Source object lifetime** | Hold reference (object must outlive draw class) · Copy structure at registration · Hold pointer + version/generation | Reference is cheapest; copy is safest for threaded use |
| **Thread boundary** | Re-emit on render thread each frame · Fill buffer on sim thread, hand to render · Fill buffer once on register, render thread reads | Once-on-register is cheapest; filling on sim requires careful hand-off |
| **Change signalling** | Dirty flag on object · External `Invalidate()` call · Version counter compared each frame | Dirty flag couples object to debug system; `Invalidate()` is caller-controlled |
| **Renderer selection** | Single active renderer at a time · Multiple active renderers composited · Named slots (default, selected, overlay) | Named slots give a clean API; compositing is powerful but complex |
| **Future promotion** | Debug-only path · Shared `IRegisteredDrawObject` interface promoted to non-debug use | A thin interface with `IsStale()` + `Render(IDrawTarget&)` would work for both |

---

## Known Tradeoffs

- **Persistent buffer + `DebugFrameData` blit:** Avoids per-frame re-emission but requires `DebugFrameData` to support either a "merge from another buffer" operation or a persistent-primitive tier. The SD-DBG-009 decision requires `DebugFrameData` to stay trivially copyable — a blit-merge is safe; a linked-list of buffers is not.
- **Swappable renderers add a registration step:** Callers must register both the object and its renderer(s). This is more code at the call site but enables extensibility. For game code debugging a custom pathfinding grid, a default line renderer is useful immediately, and a custom "cost heatmap" renderer can be added later.
- **Hold-by-reference vs copy:** Hold-by-reference is zero-cost but creates a dangling reference risk if the sim destroys the object while the render thread still holds it. Copy eliminates the risk but only works if the structure is reasonably small and has value semantics (no internal pointers).
- **Dirty flag couples source to debug:** Having the spatial grid manage a dirty flag for its debug draw creates a dependency in the wrong direction. An external `Invalidate()` call keeps the source clean but shifts responsibility to the caller.
- **Per-object buffer size:** A 128×128 grid has 32 768 cells → 131 072 edge lines. Even at 8 bytes per line primitive, that is ~1 MB per registered object. A per-object buffer needs a configurable or generous capacity.
- **Renderer selection at runtime:** Switching from "default renderer" to "selected cell renderer" must be cheap (a pointer swap or an index update, not a re-registration). This argues for storing the renderer alongside the registration entry, not baked into the draw class.

---

## Known Pitfalls (C++ / game engine context)

- **Dangling reference on sim-side teardown:** A spatial grid registered with the debug layer that is then destroyed mid-session leaves the draw class holding a dangling reference. Need unregister-on-destroy or a safety ownership model.
- **DebugFrameData overflow for large structures:** A 100×100 grid produces 40 000 line primitives. `DebugFrameData::kDefaultCapacity` is 1024. Either per-object buffers bypass the global budget, or the fixed-object layer needs a dedicated budget slice.
- **Name collision:** Two `SpatialGrid` instances registered under the same `StringCRC` layer name. SD-DBG-006 asserts on collision. Fixed objects need a per-instance name, not just a per-type name (e.g., `geometry.spatial_grid.0`, `geometry.spatial_grid.1`).
- **Thread safety on registration:** If registration happens on the sim thread and `Draw()` runs on the render thread, the `DebugLayerManager::mLayers` array is accessed from two threads. Existing `DebugLayerManager` has no mutex. Registration must be fenced or restricted to a safe point.
- **Template specialisation for concrete spatial types:** `SpatialStructureDrawer<T>` was flagged in the geometry2d spec (AIQ-2) as needing to be split into four concrete classes (`SpatialGridDrawer<T>`, `QuadtreeDrawer<T>`, etc.) because `ISpatialStructure<T>` doesn't expose internal structure. The fixed-draw pattern must respect the same constraint.
- **`#ifdef DIA_DEBUG` strips entire module in Release:** All registration calls in game code must also be guarded. If a non-debug promotion path is added later, the guard boundary shifts — plan for this.

---

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaVisualDebugger (planned) | `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`, `DebugLayerNames` — the home for any new registration abstraction |
| DiaGeometry2D | `SpatialGrid`, `Quadtree`, `BVH`, `HexGrid` — the first concrete consumers of fixed-draw registration |
| DiaGraphics | `DebugFrameData`, `DebugPrimitive` — the output target; `SD-DBG-009` requires trivially copyable; blit-merge op would need to be added here |
| DiaCore | `DynamicArrayC` — fixed-capacity array for per-object primitive buffers (no `std::vector`) |
| DiaRigidBody2D / DiaSoftBody2D | Future consumers if static collision geometry or static soft anchors need fixed-draw |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Per-instance layer names must be `StringCRC` — callers supply unique name strings at registration time |
| PD-004 No STL in public APIs | Per-object primitive buffer must use `DynamicArrayC` or a fixed-size array; no `std::vector` |
| SD-DBG-001 Stack of draw classes | Fixed-draw registration is an extension of the same stack; new registrar should fit as an `IVisualDebugger` implementation, not a parallel mechanism |
| SD-DBG-002 `#ifdef DIA_DEBUG` | The whole mechanism is debug-only for now; promotion to non-debug is a future decision |
| SD-DBG-003 Priority tiers | Spatial structures draw at priority 0 (canonical tier); fixed-draw objects should follow the same convention |
| SD-DBG-006 Assert on name collision | Per-instance names are the caller's responsibility; the assert is the enforcement mechanism |
| SD-DBG-009 `DebugFrameData` trivially copyable | Any per-object buffer must also be trivially copyable if it is to be snapshotted by a future replay system |
| SD-DBG-010 `DebugColourPalette` binding | Renderers must use palette colours; custom renderers can add domain-specific use of existing palette entries (e.g. kGoal = selected, kWarning = neighbour) |

---

## Open Questions for Ideation

- Should a registered fixed object own its primitive buffer, or should the buffer live inside `DebugLayerManager`?
- Is "re-emit primitives each frame" acceptable if the buffer is small enough, or must caching be mandatory?
- What is the right API for supplying a custom renderer — a virtual `IObjectRenderer` interface, a stored functor, or a named "renderer slots" system?
- Should `DebugLayerManager` gain a first-class `RegisterFixed()` overload, or should fixed-draw be a specialised `IVisualDebugger` subclass that the caller implements?
- How do multiple instances of the same structure type (two separate `SpatialGrid`s) get unique layer names without ceremony for the caller?
- If the pattern is promoted to non-debug use, should the renderer interface live in `DiaGraphics` or remain in `DiaVisualDebugger`?
- For the "selected cell" use case: is the selection state pushed by the sim thread to the renderer, or does the renderer query it from `DebugLayerManager::GetSelectedEntityId()`?
- Does the sim thread need to do any work per frame at all, or can a zero-overhead "just toggle draw" path be the default?
