# Research: Ideate — Debug Draw for Fixed/Registered Objects

**Input:** docs/research/debug_draw_fixed/explore.md

---

## Candidates

### Candidate 1: IFixedDebugDrawObject — Register-once with per-frame re-emit
**Home module/system:** DiaVisualDebugger (new `IFixedDebugDrawObject` interface alongside `IVisualDebugger`)  
**Size:** S  
**Description:** Introduce a second interface `IFixedDebugDrawObject` that sits alongside `IVisualDebugger`. Callers implement it by overriding `Draw(FrameData&)` as before, but also override `IsStaticTopology() const → true` which signals to `DebugLayerManager` that the output is safe to cache. `DebugLayerManager` runs `Draw()` once, stores the resulting primitives in a small per-entry buffer, and blits that buffer into `FrameData` on subsequent frames without calling `Draw()` again. An explicit `Invalidate(StringCRC layerName)` on the manager forces a re-bake.

The renderer side is unchanged from `IVisualDebugger` — draw logic is still baked into the class. The caller supplies one concrete class per structure type (one for `SpatialGrid`, one for `Quadtree`, etc.) and the manager handles the caching transparently.

**Primary value:** Zero per-frame CPU cost for topology-fixed structures with no API change for the caller beyond flagging `IsStaticTopology()`.

---

### Candidate 2: IRegisteredDrawObject + IObjectRenderer — Full register/renderer split
**Home module/system:** DiaVisualDebugger (new `IRegisteredDrawObject` and `IObjectRenderer` interfaces)  
**Size:** M  
**Description:** Split the two concerns completely. `IRegisteredDrawObject` owns the source data reference and exposes a `RebuildPrimitives(IDrawTarget&)` method. `IObjectRenderer` is a strategy object that implements `Render(const IRegisteredDrawObject&, IDrawTarget&)` — it describes *how* to interpret the object's data as primitives. `DebugLayerManager` gains a `RegisterObject(IRegisteredDrawObject*, IObjectRenderer*, int priority)` overload. The manager caches the output of `RebuildPrimitives` and re-runs it only when `IsStale()` returns true or `Invalidate()` is called.

Multiple `IObjectRenderer` instances can be registered against the same object under different layer names (e.g. `"grid.default"` and `"grid.selected"`), enabling simultaneous or switchable visual modes without touching the source object at all.

**Primary value:** Callers can swap or compose renderers at runtime without changing the source structure or re-registering — the grid doesn't know about the renderer, and the renderer doesn't own the grid.

---

### Candidate 3: DebugDrawRegistrar — Dedicated fixed-object registry separate from DebugLayerManager
**Home module/system:** DiaVisualDebugger (new `DebugDrawRegistrar` class; `DebugLayerManager` remains unchanged)  
**Size:** M  
**Description:** Add a standalone `DebugDrawRegistrar` that manages registered fixed objects independently of `DebugLayerManager`. Game code creates a registrar, registers objects with `Register(StringCRC name, IRegisteredDrawObject*, IObjectRenderer*)`, and calls `registrar.Draw(FrameData&)` each frame. The registrar handles caching, invalidation, and renderer dispatch internally. `DebugLayerManager` is not touched — the registrar is itself registered as a single `IVisualDebugger` layer named `"fixed_objects"` (or omitted from the manager entirely).

The separation keeps `DebugLayerManager` simple and makes the fixed-object system independently testable. The cost is an extra object at the call site and a slightly different API from the existing layer stack.

**Primary value:** Keeps `DebugLayerManager` unchanged and its 64-slot capacity dedicated to dynamic draw classes; fixed objects don't compete for those slots.

---

### Candidate 4: Per-object primitive cache on IVisualDebugger — Mixin/CRTP cache tier
**Home module/system:** DiaVisualDebugger (new `CachedVisualDebugger<T>` CRTP base)  
**Size:** S  
**Description:** A CRTP mixin `CachedVisualDebugger<Derived, kCapacity>` wraps an `IVisualDebugger`. It internally owns a `DynamicArrayC<DebugPrimitive, kCapacity>` buffer. The first call to `Draw()` populates the buffer by calling `Derived::RebuildPrimitives(buffer)`. Subsequent calls blit the buffer to `FrameData` without calling `RebuildPrimitives` again. `Invalidate()` clears the buffer and forces a rebuild on the next frame. `DebugLayerManager` is unmodified — it just calls `Draw()` as usual on any registered `IVisualDebugger`.

Custom renderers are handled by the derived class — the CRTP base is only a caching layer. A caller wanting two visual modes creates two derived classes (one default, one custom) and registers both, each with a distinct layer name.

**Primary value:** Caching is additive and opt-in; existing draw classes are unaffected; no new interfaces for `DebugLayerManager` to understand.

---

### Candidate 5: FixedDrawSlot — Named renderer slots per registered object
**Home module/system:** DiaVisualDebugger (extends `DebugLayerManager` with a `RegisterFixed()` overload)  
**Size:** M  
**Description:** `DebugLayerManager` gains a `RegisterFixed(StringCRC instanceName, const T& object, int priority)` overload (template). The object is stored by reference. A default renderer for `T` is looked up from a `RendererRegistry<T>` (a small static table mapping type IDs to render functions). The caller can optionally call `SetRenderer(StringCRC instanceName, IObjectRenderer*)` to override the default.

Each registered fixed object occupies one slot in the manager (counts against `kMaxLayers = 64`). The manager auto-invalidates the cache if `IsStale()` on the registered object returns true (objects implement a lightweight version-counter interface `IVersioned`). No explicit `Invalidate()` required for objects that already track mutation via a version counter.

**Primary value:** Minimal call-site boilerplate — `manager.RegisterFixed("my.grid", myGrid)` just works with the default renderer; custom renderers are opt-in.

---

### Candidate 6: IObjectRenderer only — Renderer-strategy injected into existing draw classes
**Home module/system:** DiaGeometry2DVisualDebugger (modify existing planned draw classes)  
**Size:** S  
**Description:** Don't add a registration system at all. Instead, modify the planned `SpatialGridDrawer<T>`, `QuadtreeDrawer<T>` etc. in `DiaGeometry2DVisualDebugger` to accept an optional `IObjectRenderer*` constructor argument. If non-null, the renderer is called instead of the default draw logic. Caching is handled by the draw class itself using a dirty flag exposed by the spatial structure (or a `bool mCacheDirty` that the caller flips via `Invalidate()`).

This is the smallest possible change — it adds custom renderers without a new registration architecture. It's also the least general: each draw class must be modified individually, and the pattern doesn't compose with future non-debug use.

**Primary value:** Very low implementation cost; immediately enables custom renderer injection for grid/quadtree/BVH draw classes with minimal new infrastructure.

---

### Candidate 7: Command-stream model — Sim pushes draw-or-skip tokens, render thread owns buffer
**Home module/system:** DiaVisualDebugger + DiaApplication thread boundary  
**Size:** L  
**Description:** Sim thread registers a fixed object once and thereafter pushes a lightweight "draw token" (enabled/disabled flag + optional renderer selector) into the thread-safe frame queue. The render thread holds a persistent `PerObjectPrimitiveBuffer` per registered object. On the first frame (or after invalidation), the render thread calls `RebuildPrimitives()`. On subsequent frames, if the token says "draw", the buffer is blitted to `FrameData`. The sim thread never calls draw logic again.

This is architecturally the cleanest for threading — the sim thread has zero per-frame draw cost after initial registration, and the render thread owns all draw state. The cost is significant: a new message type in the frame queue, per-object buffers on the render side, and a more complex registration handshake. It also partially duplicates the existing `FrameData` producer/consumer pattern.

**Primary value:** True zero sim-thread cost per frame for fixed draws; explicit thread ownership of draw data; extends naturally to non-debug use later.

---

### Candidate 8: DebugDrawGroup — Batch multiple instances under one layer name
**Home module/system:** DiaVisualDebugger (new `DebugDrawGroup` aggregate IVisualDebugger)  
**Size:** S  
**Description:** A `DebugDrawGroup` implements `IVisualDebugger` and holds an array of `IVisualDebugger*` children. Registering a group with `DebugLayerManager` registers a single named layer that dispatches `Draw()` to all children. Each child can have its own primitive cache. This solves the "multiple instances of the same structure type need unique names" problem: game code creates one group named `"geometry.grids"` and adds all `SpatialGridDrawer` instances into it. Toggling the group layer enables/disables all grid draws simultaneously.

Individual children can still be toggled via `DebugDrawGroup::EnableChild(StringCRC childName)`. The group itself doesn't need unique per-instance layer names in `DebugLayerManager` — only the group's name must be unique.

**Primary value:** Solves the multiple-instance naming problem elegantly; one layer toggle controls all instances of a type while per-instance control is still possible.

---

### Candidate 9: Full system — IRegisteredDrawObject + renderer slots + DebugDrawGroup + cache
**Home module/system:** DiaVisualDebugger (new `FixedDebugDraw` subsystem — multiple new classes)  
**Size:** L  
**Description:** Combines Candidates 2, 4, and 8 into a coherent subsystem. `IRegisteredDrawObject` is the source abstraction; `IObjectRenderer` is the strategy; `CachedVisualDebugger` provides the primitive cache; `DebugDrawGroup` handles multiple instances. The render side additionally gains a `RendererSlot` concept: each registered object has a named set of renderers (default, selected, custom...) and callers activate a slot by name. `DebugLayerManager` itself is not changed — the subsystem plugs into it as one or more `IVisualDebugger` instances.

This is the most complete design and maps most directly to the user's stated goals (register once, stream says draw/no-draw, custom renderers). It also provides the cleanest promotion path to non-debug use since `IRegisteredDrawObject` and `IObjectRenderer` have no `#ifdef` in their core interfaces.

**Primary value:** Covers all stated requirements — register once, zero-cost re-draw, default + custom renderers, multiple instances — in a single coherent API.

---

## Coverage Map

The nine candidates span the full design space from explore.md:

- **Cache strategy:** Candidates 1, 4 (transparent caching), 7 (render-thread-owned buffer), 3/2/9 (explicit cache in registrar)
- **Renderer flexibility:** Candidates 6 (inject into existing classes), 2/5/9 (strategy pattern), 1/4 (baked into subclass)
- **DebugLayerManager impact:** Candidates 4, 6, 8 (zero change), 1 (minor — new flag), 5 (new overload), 3/7/9 (new subsystem)
- **Size range:** S (1, 4, 6, 8), M (2, 3, 5), L (7, 9)
- **Future promotion readiness:** Candidates 2, 7, 9 have clean non-debug promotion paths; others are more tightly coupled to the debug system
- **Complexity at call site:** Candidates 5 and 6 are lowest ceremony; Candidate 7 is highest
