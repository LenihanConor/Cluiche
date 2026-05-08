# Feature Spec: fixed-draw-layer

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Adds a fixed-draw registration path to `DiaVisualDebugger` for objects whose primitive topology never changes between frames — spatial grids, quadtrees, BVHs, hex grids, rest-pose skeletons, and future fixed-topology types. These objects are registered once at init time; the render thread owns the resulting primitive buffer permanently and draws directly from it each frame, bypassing `DebugFrameData` entirely. A swappable `IObjectRenderer` strategy determines how the buffer is built, enabling default and custom visual modes without touching the source object.

**Problem solved:** Fixed-topology objects currently re-emit thousands of line primitives into `DebugFrameData` every frame (a 100×100 `SpatialGrid` produces ~40 000 line primitives — 40× the default capacity), overflowing the budget and wasting CPU on redundant iteration.

---

## Acceptance Criteria

1. `IObjectRenderer` interface with `BuildPrimitives(const void* sourceObject, IFixedPrimitiveBuffer& out)` — called once at registration and once per `Invalidate()`
2. `IFixedPrimitiveBuffer` — a write-only sink passed to `BuildPrimitives()`; capacity set per-registration by the caller
3. `DebugLayerManager::RegisterFixed(StringCRC name, const void* sourceObject, IObjectRenderer*, unsigned int capacity, int priority)` registers a fixed object; the manager delegates to its internal `FixedDrawRegistry`
4. `DebugLayerManager::LockRegistration()` called once when the render thread starts; any `RegisterFixed()` after that point hits `DIA_ASSERT(!mRegistrationLocked)`
5. `DebugLayerManager::InvalidateFixed(StringCRC name)` triggers a one-time rebuild of the named buffer on the next `Draw()` call
6. Fixed buffers **never** enter `DebugFrameData` — the render thread draws directly from owned buffers
7. `EnableLayer()` / `DisableLayer()` on `DebugLayerManager` toggles fixed layers identically to dynamic layers — single public toggle surface
8. `DebugLayerManager::Draw(FrameData&)` draws fixed layers and dynamic layers in unified priority order
9. `FixedDrawRegistry` is an internal implementation detail — never exposed in `DebugLayerManager`'s public API
10. Buffer pointer swap on `InvalidateFixed()` uses an atomic pointer swap at the next `Draw()` call — no mutex on the hot path
11. `DiaLogger` warning emitted at registration if a single buffer exceeds a configurable threshold (default: 65 536 primitives)
12. Default `IObjectRenderer` implementations provided for: `SpatialGrid`, `Quadtree`, `BVH`, `HexGrid`
13. All code guarded by `#ifdef DIA_DEBUG`; zero overhead in Release
14. New classes added to existing `DiaVisualDebugger.vcxproj` — no new project required

---

## Design

### IObjectRenderer

```cpp
// Dia/DiaVisualDebugger/IObjectRenderer.h
namespace Dia::Debug {

class IObjectRenderer
{
public:
    virtual ~IObjectRenderer() = default;
    virtual void BuildPrimitives(const void* sourceObject,
                                 IFixedPrimitiveBuffer& out) const = 0;
};

// Typed wrapper — game code subclasses this; never writes const void* directly.
template<typename T>
class TypedObjectRenderer : public IObjectRenderer
{
public:
    void BuildPrimitives(const void* src, IFixedPrimitiveBuffer& out) const override final
    {
        DoBuild(*static_cast<const T*>(src), out);
    }

protected:
    virtual void DoBuild(const T& object, IFixedPrimitiveBuffer& out) const = 0;
};

} // namespace Dia::Debug
```

`IObjectRenderer` keeps `FixedDrawRegistry` non-template. `TypedObjectRenderer<T>` is the public-facing base — game code never writes `const void*`. The cast is in one place, `final` on `BuildPrimitives` prevents accidental override of the wrong method.

### IFixedPrimitiveBuffer

```cpp
// Dia/DiaVisualDebugger/IFixedPrimitiveBuffer.h
namespace Dia::Debug {

class IFixedPrimitiveBuffer
{
public:
    virtual ~IFixedPrimitiveBuffer() = default;
    virtual void RequestDraw(const Dia::Maths::Vector2D& from,
                             const Dia::Maths::Vector2D& to,
                             Dia::Graphics::RGBA colour) = 0;
    virtual void RequestDrawRect(const Dia::Maths::Vector2D& min,
                                 const Dia::Maths::Vector2D& max,
                                 Dia::Graphics::RGBA colour) = 0;
    virtual void RequestDrawCircle(const Dia::Maths::Vector2D& centre,
                                   float radius,
                                   Dia::Graphics::RGBA colour) = 0;
    virtual bool IsFull() const = 0;
};

} // namespace Dia::Debug
```

Mirrors the `DebugFrameData` request API surface so renderers can be written without knowing which target they are writing to — a seam for future non-debug promotion.

### FixedDrawRegistry (internal)

```cpp
// Dia/DiaVisualDebugger/FixedDrawRegistry.h  (not in public API)
namespace Dia::Debug {

class FixedDrawRegistry
{
public:
    static const unsigned int kMaxFixed = 32;

    void Register(Dia::Core::StringCRC name,
                  const void*         sourceObject,
                  IObjectRenderer*    renderer,
                  unsigned int        capacity,
                  int                 priority);

    void Unregister(Dia::Core::StringCRC name);

    void EnableLayer (Dia::Core::StringCRC name);
    void DisableLayer(Dia::Core::StringCRC name);
    bool IsLayerEnabled(Dia::Core::StringCRC name) const;
    bool HasLayer(Dia::Core::StringCRC name) const;

    // Marks named entry dirty — rebuilt on next Draw().
    void Invalidate(Dia::Core::StringCRC name);

    // Draws all enabled fixed layers in priority order directly via canvas.
    // Does NOT write to FrameData.
    void Draw(Dia::Graphics::ICanvas& canvas);

private:
    struct FixedEntry
    {
        Dia::Core::StringCRC  name;
        const void*           sourceObject = nullptr;
        IObjectRenderer*      renderer     = nullptr;
        FixedPrimitiveBuffer* buffer       = nullptr; // render-thread owned
        FixedPrimitiveBuffer* pendingBuffer = nullptr; // atomic swap target
        int                   priority     = 0;
        bool                  enabled      = true;
        bool                  dirty        = true;    // true = rebuild next Draw()
    };

    Dia::Core::Containers::DynamicArrayC<FixedEntry, kMaxFixed> mEntries;
    bool mSortDirty = false;
    bool mIsDrawing  = false;  // assert guard — Invalidate() asserts !mIsDrawing

    void RebuildEntry(FixedEntry& entry);
    void SortByPriority();
    int  FindIndex(Dia::Core::StringCRC name) const;
};

} // namespace Dia::Debug
```

### DebugLayerManager additions

```cpp
// New public methods on DebugLayerManager:

// Lock registration — call once when render thread starts.
void LockRegistration();

// Register a fixed-topology object. DIA_ASSERT if called after LockRegistration().
// capacity: max primitives this object will ever emit.
void RegisterFixed(Dia::Core::StringCRC  name,
                   const void*           sourceObject,
                   IObjectRenderer*      renderer,
                   unsigned int          capacity,
                   int                   priority = 0);

void UnregisterFixed(Dia::Core::StringCRC name);

// Triggers one rebuild of the named fixed buffer on next Draw().
void InvalidateFixed(Dia::Core::StringCRC name);
```

`EnableLayer()`, `DisableLayer()`, `IsLayerEnabled()`, `HasLayer()` on `DebugLayerManager` check both the dynamic `mLayers` array and `mFixedRegistry` — single toggle surface for game code.

`Draw(FrameData&)` calls dynamic layers into `FrameData` as before, then calls `mFixedRegistry.Draw(canvas)` for fixed layers. Priority ordering is unified: the combined set is sorted by priority before drawing.

### Draw flow

```
DebugLayerManager::Draw(FrameData&, ICanvas&)
  ├─ sort unified priority list (dynamic + fixed) — only when dirty
  ├─ for each entry in priority order:
  │   ├─ dynamic layer: if enabled → layer.Draw(frameData)   // existing path
  │   └─ fixed layer:   if dirty  → RebuildEntry()           // rare
  │                     if enabled → DrawFromBuffer(canvas)   // every frame
  └─ done
```

### InvalidateFixed — buffer swap

```cpp
void FixedDrawRegistry::Invalidate(Dia::Core::StringCRC name)
{
    int idx = FindIndex(name);
    DIA_ASSERT(idx >= 0, "InvalidateFixed — layer not registered");
    mEntries[idx].dirty = true;
    // RebuildEntry() called on next Draw() — no cross-thread access here
    // because Invalidate() is only called at safe sync points (same thread contract)
}
```

No atomic needed: `Invalidate()` is called on the sim thread at a safe frame boundary (same point that produces `FrameData`). The render thread only reads `dirty` inside `Draw()`, which is called after the sim has finished producing that frame's data. This is the same contract the existing `FrameData` producer/consumer uses.

### Default renderers

```cpp
// Dia/DiaVisualDebugger/Renderers/SpatialGridRenderer.h
class SpatialGridRenderer : public TypedObjectRenderer<Dia::Geometry2D::SpatialGrid>
{
protected:
    void DoBuild(const Dia::Geometry2D::SpatialGrid& grid,
                 IFixedPrimitiveBuffer& out) const override;
    // Iterates all cells; draws AARect bounds in DebugColourPalette::kInactive
};

class QuadtreeRenderer : public TypedObjectRenderer<Dia::Geometry2D::Quadtree> { ... };
class BVHRenderer      : public TypedObjectRenderer<Dia::Geometry2D::BVH>      { ... };
class HexGridRenderer  : public TypedObjectRenderer<Dia::Geometry2D::HexGrid>  { ... };
```

No `const void*` anywhere in game-facing code. Draw logic mirrors the `SpatialStructureDrawer` design from `geometry2d-visual-debugger-stack` — those classes will be amended to use this path as a follow-on.

---

## Registration Example (game code)

```cpp
// During init — before render thread starts:
static SpatialGridRenderer gridRenderer;
manager.RegisterFixed("nav_grid.topology", &myGrid, &gridRenderer, 8192, 0);
manager.RegisterFixed("nav_grid.selection", &myGrid, &selectedGridRenderer, 64, 10);

manager.LockRegistration(); // render thread starts after this

// Every frame — sim thread:
manager.Draw(frameData, canvas);  // fixed layers drawn from buffer, dynamic via frameData

// If grid is ever rebuilt:
manager.InvalidateFixed("nav_grid.topology"); // rebuilds buffer once on next Draw()
```

---

## Files Changed / Created

| File | Change |
|------|--------|
| `Dia/DiaVisualDebugger/IObjectRenderer.h` | New |
| `Dia/DiaVisualDebugger/IFixedPrimitiveBuffer.h` | New |
| `Dia/DiaVisualDebugger/FixedPrimitiveBuffer.h` | New — concrete implementation of `IFixedPrimitiveBuffer` |
| `Dia/DiaVisualDebugger/FixedPrimitiveBuffer.cpp` | New |
| `Dia/DiaVisualDebugger/FixedDrawRegistry.h` | New (internal) |
| `Dia/DiaVisualDebugger/FixedDrawRegistry.cpp` | New |
| `Dia/DiaVisualDebugger/Renderers/SpatialGridRenderer.h` | New |
| `Dia/DiaVisualDebugger/Renderers/SpatialGridRenderer.cpp` | New |
| `Dia/DiaVisualDebugger/Renderers/QuadtreeRenderer.h` | New |
| `Dia/DiaVisualDebugger/Renderers/QuadtreeRenderer.cpp` | New |
| `Dia/DiaVisualDebugger/Renderers/BVHRenderer.h` | New |
| `Dia/DiaVisualDebugger/Renderers/BVHRenderer.cpp` | New |
| `Dia/DiaVisualDebugger/Renderers/HexGridRenderer.h` | New |
| `Dia/DiaVisualDebugger/Renderers/HexGridRenderer.cpp` | New |
| `Dia/DiaVisualDebugger/DebugLayerManager.h` | Modified — `LockRegistration()`, `RegisterFixed()`, `UnregisterFixed()`, `InvalidateFixed()`; `Draw()` gains `ICanvas&` parameter |
| `Dia/DiaVisualDebugger/DebugLayerManager.cpp` | Modified |
| `Dia/DiaVisualDebugger/DiaVisualDebugger.vcxproj` | Modified — add new files |
| `Dia/DiaVisualDebugger/DiaVisualDebugger.vcxproj.filters` | Modified |

**Prerequisites:** `debug-layer-manager` must be implemented first.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Write `IObjectRenderer.h` and `IFixedPrimitiveBuffer.h` | Header only |
| 2 | Write `FixedPrimitiveBuffer.h/.cpp` | Concrete buffer — `DynamicArrayC<DebugPrimitive, N>` heap-allocated with caller-specified capacity |
| 3 | Write `FixedDrawRegistry.h/.cpp` | Internal class — register, enable/disable, invalidate, draw |
| 4 | Write default renderers: `SpatialGridRenderer`, `QuadtreeRenderer`, `BVHRenderer`, `HexGridRenderer` | One `.h/.cpp` pair each |
| 5 | Modify `DebugLayerManager` — add `LockRegistration()`, `RegisterFixed()`, `UnregisterFixed()`, `InvalidateFixed()`; route `EnableLayer`/`DisableLayer`/`HasLayer` through both dynamic + fixed; update `Draw()` signature | |
| 6 | Update `DiaVisualDebugger.vcxproj` and `.vcxproj.filters` | Add all new files |
| 7 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 8 | Write tests | `TestFixedDrawLayer.cpp` in GoogleTests |
| 9 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/DiaVisualDebugger/TestFixedDrawLayer.cpp`

A stub `IObjectRenderer` (`TestRenderer`) records `BuildPrimitives()` call count and emits a configurable number of line primitives. A stub `ICanvas` records draw calls made by `FixedDrawRegistry::Draw()`.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| Registration | `RegisterFixed_SingleEntry_CountOne` | Register one fixed layer → `HasLayer(name) == true` |
| Registration | `RegisterFixed_DuplicateNameAsserts` | Same name twice → `DIA_ASSERT` fires |
| Registration | `LockRegistration_RegisterAfterLockAsserts` | `LockRegistration()` then `RegisterFixed()` → `DIA_ASSERT` fires |
| Registration | `RegisterFixed_BeforeLock_NoAssert` | `RegisterFixed()` before `LockRegistration()` → no assert |
| Registration | `UnregisterFixed_RemovesEntry` | Register then unregister → `HasLayer == false` |
| Registration | `UnregisterFixed_UnknownNameNoOp` | Unregister unknown name → no crash |
| Toggle | `EnableDisable_DefaultEnabled` | Registered layer is enabled by default |
| Toggle | `DisableFixed_SkipsDrawOnCanvas` | Disable → `Draw()` → canvas receives zero draw calls for that layer |
| Toggle | `EnableFixed_DrawsOnCanvas` | Enable after disable → `Draw()` → canvas receives draw calls |
| Toggle | `EnableLayer_RoutestoFixedRegistry` | `manager.EnableLayer(fixedName)` → fixed registry entry enabled |
| Toggle | `DisableLayer_RoutesToFixedRegistry` | `manager.DisableLayer(fixedName)` → fixed registry entry disabled |
| Build | `Draw_BuildsOnFirstCall` | Fresh registration → `Draw()` → `TestRenderer::BuildPrimitivesCallCount == 1` |
| Build | `Draw_DoesNotRebuildOnSecondCall` | Two `Draw()` calls → `BuildPrimitivesCallCount == 1` |
| Build | `Invalidate_RebuildOnNextDraw` | `InvalidateFixed()` → next `Draw()` → `BuildPrimitivesCallCount == 2` |
| Build | `Invalidate_DoesNotRebuildUntilDraw` | `InvalidateFixed()` without `Draw()` → `BuildPrimitivesCallCount == 1` |
| Draw | `Draw_FixedLayerNotInFrameData` | Fixed layer registered → `Draw()` → `FrameData` primitive count unchanged |
| Draw | `Draw_FixedLayerHitsCanvas` | Fixed layer registered → `Draw()` → canvas receives primitives |
| Draw | `Draw_PriorityOrder_FixedBeforeDynamic` | Fixed at priority 0, dynamic at priority 10 → fixed drawn first |
| Draw | `Draw_PriorityOrder_DynamicBeforeFixed` | Dynamic at priority 0, fixed at priority 10 → dynamic drawn first |
| Draw | `Draw_EmptyRegistry_NoOp` | No fixed layers → `Draw()` → no crash, canvas not called |
| Renderers | `SpatialGridRenderer_EmitsCellRects` | 2×2 grid → `BuildPrimitives` → 4 rect primitives |
| Renderers | `SpatialGridRenderer_ColoursAreInactive` | All rects use `DebugColourPalette::kInactive` |
| Renderers | `QuadtreeRenderer_EmitsNodeRects` | Quadtree with known structure → correct node count |
| Renderers | `HexGridRenderer_EmitsHexEdges` | 1-cell hex grid → 6 line primitives |
| Renderers | `BVHRenderer_NotBuilt_NoPrimitives` | `IsBuilt() == false` → 0 primitives |
| Buffer | `FixedPrimitiveBuffer_CapacityRespected` | Fill to capacity → `IsFull() == true` → further requests dropped |
| Buffer | `FixedPrimitiveBuffer_ClearResets` | Fill → clear → `IsFull() == false`, count zero |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for all identifiers | Compliant — all layer names use `StringCRC`; no raw `const char*` comparisons in registry lookups |
| PD-002 | ProcessingUnit/Phase/Module architecture | Compliant — `LockRegistration()` called during module start; `Draw()` called from appropriate phase; registry is a plain C++ object with no lifecycle of its own |
| PD-003 | Component-based entities | Compliant — `entityId` picking seam inherited from `DebugPrimitive`; no entity concerns introduced here |
| PD-004 | No STL in public APIs | Compliant — `RegisterFixed()` takes `const void*`, `IObjectRenderer*`, `unsigned int`, `int`; `FixedDrawRegistry` uses `DynamicArrayC<FixedEntry, kMaxFixed>`; no STL in any public method |
| PD-005 | x64 only | Compliant — no new projects; existing `DiaVisualDebugger.vcxproj` targets x64 |
| PD-006 | Visual Studio project files are source of truth | Compliant — new files added to `DiaVisualDebugger.vcxproj` and `.vcxproj.filters` |
| PD-007 | C++20 required | Compliant — all code under `/std:c++20` |
| PD-008 | `Directory.Build.props` owns OutDir/IntDir | Compliant — no per-project overrides |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — `dia.visualdebugger.architecture.module.md` updated to reflect new classes |
| AD-002 | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all new classes in `Dia::Debug::` namespace |
| AD-004 | ProcessingUnit/Phase/Module for app structure | Compliant — reinforces PD-002 |
| AD-005 | Component-based entities | Compliant — no entity concerns |
| SD-DBG-001 | Stack of focused draw classes, not options flags | Compliant — fixed layers are registered independently by name, same model as dynamic layers |
| SD-DBG-002 | `#ifdef DIA_DEBUG` guards | Compliant — all fixed-draw code inside `DIA_DEBUG`; `LockRegistration()`, `RegisterFixed()`, `InvalidateFixed()` are all no-ops in Release |
| SD-DBG-003 | Priority tiers at registration | Compliant — fixed layers participate in the same priority ordering as dynamic layers; spatial structures at 0 per canonical tier |
| SD-DBG-005 | Global `debugScale` on manager | Compliant — default renderers read `DebugLayerManager::GetDebugScale()` before submitting sizes |
| SD-DBG-006 | `DIA_ASSERT` on layer name collision | Compliant — `FixedDrawRegistry::Register()` asserts on duplicate name; `DebugLayerManager::RegisterFixed()` also checks dynamic layer names to prevent cross-type collision |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all default renderers use `DebugColourPalette` constants |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — all new classes added to existing `DiaVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | IObjectRenderer — `const void*` | Using `const void*` for type erasure is unconventional in a C++20 codebase. Should `IObjectRenderer` be a template or use `std::any`? | `TypedObjectRenderer<T>` wrapper adopted. `IObjectRenderer` keeps `FixedDrawRegistry` non-template; `TypedObjectRenderer<T>` hides the `const void*` cast in one place (`BuildPrimitives`, marked `final`). Game code subclasses `TypedObjectRenderer<MySpatialGrid>` and overrides `DoBuild()` — never writes `const void*`. `std::any` rejected (PD-004). |
| 2 | `Draw()` signature change | `DebugLayerManager::Draw()` currently takes only `FrameData&`. Adding `ICanvas&` is a breaking change to all existing call sites. How many call sites exist? | Option A confirmed: `Draw(FrameData&, ICanvas&)`. Audit all `DebugLayerManager::Draw()` call sites before implementation — expected to be one per render phase. `ICanvas` is already available at the render call site (it is the render target). Each site is a one-line change. |
| 3 | `FixedPrimitiveBuffer` heap allocation | The buffer capacity is caller-specified at registration time. Should `FixedPrimitiveBuffer` use `DiaCore` heap allocation or a fixed compile-time template parameter? | Plain `new DebugPrimitive[capacity]` / `delete[]` at unregister. Allocated once at registration, freed once at unregister — no hot-path or fragmentation concern. Debug-only and stripped in Release, so production memory management is not a factor. Adding a DiaCore allocator dependency for a buffer touched twice over the session lifetime is unnecessary overhead. Fixed compile-time template parameter rejected — would force `FixedDrawRegistry` to be templated. |
| 4 | `Invalidate()` thread safety | The spec states `Invalidate()` is called at a safe frame boundary, same contract as `FrameData`. Is this contract enforced or assumed? | Enforced by assert. `FixedDrawRegistry` carries a `bool mIsDrawing` flag, set to `true` at the start of `Draw()` and cleared at the end. `Invalidate()` asserts `!mIsDrawing`. Catches the obvious violation (calling `InvalidateFixed()` from inside a draw callback or on the wrong thread while draw is in flight). Won't catch a true concurrent cross-thread race (the flag is not atomic), but makes the contract explicit and catches misuse in debug builds. Usage contract documented: `InvalidateFixed()` must be called on the sim thread before `Draw()` is called for that frame — same contract as `FrameData` production. |
| 5 | Default renderer for `BVH` — `IsBuilt()` | `BVHRenderer` must check `IsBuilt()` before iterating nodes. Does `BVH` expose `IsBuilt()` publicly? | Must be verified at implementation time. If missing, add `bool IsBuilt() const` to `BVH.h`. |
| 6 | Cross-type name collision | SD-DBG-006 asserts on duplicate layer name. `RegisterFixed()` must also check against dynamic layer names (and vice versa) to prevent the same `StringCRC` existing in both registries. Is this checked? | Compliant — `DebugLayerManager::RegisterFixed()` checks `mLayers` (dynamic) before delegating to `mFixedRegistry`. `Register()` (dynamic) checks `mFixedRegistry::HasLayer()` before adding. Both assert on collision. |
| 7 | `DebugDrawGroup` deferral | Multi-instance naming (two `SpatialGrid` instances) is deferred. What happens if a caller registers two grids — does the second assert fire immediately? | Yes — SD-DBG-006 asserts. Caller must supply distinct names manually: `"nav_grid.topology"` and `"combat_grid.topology"`. This is the expected V1 behaviour. `DebugDrawGroup` (Candidate C from research) is the follow-on feature that automates this. Document in the spec as a known limitation. |
| 8 | Non-debug promotion seam | `IObjectRenderer` and `IFixedPrimitiveBuffer` are the natural promotion interfaces. Should they live in `DiaVisualDebugger` or in a more foundational module (`DiaGraphics`) now? | Stay in `DiaVisualDebugger` for now — moving them to `DiaGraphics` is a future decision when a concrete non-debug consumer exists. The interfaces have no `#ifdef DIA_DEBUG` in their definitions, so the move is a file-copy not a redesign. |

---

## Status

`Approved`
