# Research: Ideate — Debug Draw for Fixed/Registered Objects

**Input:** docs/research/debug_draw_fixed/explore.md  
**Revised:** 2026-05-03 (original 9 candidates collapsed after design discussion)

---

## Key insight from discussion

The original candidates split along the wrong axis (caching strategy). The real axis is:

> **Does the large primitive set ever enter FrameData?**

If yes — the design has a fundamental overflow problem for large structures (grids, quadtrees). The correct answer is: fixed topology never touches FrameData. The render thread owns the buffer permanently. FrameData is reserved for small, fast-changing overlays only.

This collapses the original 9 candidates into 3 composable pieces.

---

## Candidates

### Candidate A: Render-Thread Fixed Buffer (core architecture)
**Home module/system:** DiaVisualDebugger — new `FixedDrawLayer` alongside `IVisualDebugger`  
**Size:** M  
**Description:** A separate registration path for fixed-topology objects. At registration time the sim thread calls a supplied renderer function once, which builds a primitive buffer. That buffer is handed to the render thread which owns it permanently. Every subsequent frame the render thread draws directly from that buffer — no FrameData involved, no sim thread involved, no source object referenced.

`Invalidate()` is the only per-object operation after registration. It triggers one rebuild on the sim thread, sends a new buffer to the render thread, and the old buffer is freed. Called only when the topology genuinely changes (grid resized, quadtree rebuilt). Not called on selection change, frame toggle, or anything dynamic.

The layer toggle (`EnableLayer` / `DisableLayer`) is the only per-frame signal from anywhere. No token stream, no dirty flag.

```
REGISTRATION (sim thread, once):
  object + renderer → builds primitive buffer → ownership transferred to render thread

EVERY FRAME (render thread):
  if enabled: draw from owned buffer → screen       // no FrameData, no sim work

INVALIDATE (sim thread, rare):
  rebuild buffer → send new buffer to render thread → free old buffer
```

**Scales to:** Any fixed-topology object type (grid, hexgrid, quadtree, BVH, navmesh, static collision, rest-pose skeleton). Each just needs a renderer function written once.

**Primary value:** Zero per-frame CPU and zero FrameData budget cost for large static structures, regardless of size.

---

### Candidate B: IObjectRenderer — Renderer strategy at registration
**Home module/system:** DiaVisualDebugger — interface passed into Candidate A's registration  
**Size:** S  
**Description:** A single-method interface: `BuildPrimitives(const void* sourceObject, IFixedPrimitiveBuffer& out)`. Passed at registration alongside the object. Determines how the buffer is built. Multiple renderer implementations can exist for the same object type — `DefaultGridRenderer`, `SelectedGridRenderer`, `HeatmapGridRenderer`.

Two renderers can be registered against the same object under different layer names, giving simultaneous or independently-toggled visual modes:

```cpp
manager.RegisterFixed("nav_grid.topology", &myGrid, &defaultRenderer);
manager.RegisterFixed("nav_grid.selection", &myGrid, &selectedRenderer);
// two independent layers, same source object, two separate buffers
```

The source object is only accessed during `BuildPrimitives()` — which runs at registration and at `Invalidate()`. After that, the render thread never touches it.

**Note on the dynamic overlay case:** Selection highlighting (selected cell = red, neighbours = orange) is NOT a fixed renderer. It is a small dynamic overlay — 7 primitives per frame — submitted normally via FrameData. It changes every frame as selection changes. Don't put it in the fixed buffer.

**Primary value:** Draw logic is decoupled from the source object. New visual modes require a new renderer, not a new source class.

---

### Candidate C: DebugDrawGroup — Multi-instance naming
**Home module/system:** DiaVisualDebugger — lightweight aggregate over Candidate A  
**Size:** S  
**Description:** Registers multiple fixed buffers under a single parent layer name. `manager.RegisterFixedGroup("nav_grids", renderers)` gives one toggle that controls all grid instances simultaneously, while individual instances can still be toggled by index or child name.

Solves the practical problem of two `SpatialGrid` instances both wanting the name `"geometry.spatial_grid"` — SD-DBG-006 would assert. The group holds child names (`"geometry.spatial_grid.0"`, `"geometry.spatial_grid.1"`) and the parent name is the toggle surface.

**Primary value:** Multiple instances of the same type don't require the caller to invent unique names manually.

---

## What was ruled out and why

| Original Candidate | Ruling |
|-------------------|--------|
| 1. IFixedDebugDrawObject flag | Primitives still flow through FrameData — overflow problem not solved |
| 2. IRegisteredDrawObject + IObjectRenderer (original) | Cache blits into FrameData each frame — same overflow problem; render thread still holds sim-side reference |
| 3. DebugDrawRegistrar | Parallel system fighting the existing layer stack; absorbed by Candidate A |
| 4. CachedVisualDebugger CRTP | Good for recomputation cost but primitives still in FrameData; useful as internal impl detail of Candidate A, not standalone |
| 5. FixedDrawSlot + IVersioned | IVersioned couples source objects to debug system in wrong direction; absorbed by Candidate A + explicit Invalidate() |
| 6. IObjectRenderer injection | Absorbed — this is how Candidate B's default renderers are implemented |
| 7. Full command-stream | Simplified away — layer toggle IS the stream; no per-frame token needed |
| 8. DebugDrawGroup | Kept as Candidate C |
| 9. Full system (original) | Too many moving parts; Candidates A+B+C achieve same goals with less |

---

## How the three candidates compose

```
Candidate A  ← the core mechanism (render-thread owns buffer, register once)
  + Candidate B  ← plugged in at registration (how the buffer is built)
  + Candidate C  ← optional, additive (multi-instance naming)
```

Candidate A alone gives you register-once + toggle. Add B for custom renderers. Add C if you have multiple instances of the same type. Each is independently useful — you don't need all three to ship V1.

---

## Coverage Map

| Design axis (from explore.md) | How it's handled |
|-------------------------------|-----------------|
| Cache lifetime | Permanent until `Invalidate()` — never per-frame |
| Primitive ownership | Render thread owns buffer — never FrameData |
| Renderer binding | Candidate B — strategy passed at registration |
| Source object lifetime | Sim can destroy after registration — render thread holds no reference |
| Thread boundary | Buffer transferred at registration; `Invalidate()` sends new buffer |
| Change signalling | Explicit `Invalidate()` — caller-controlled, no coupling to source object |
| Renderer selection | Two registrations under two names — independent toggle |
| Future promotion | Same pattern (buffer + renderer) applies directly to non-debug static geometry |
