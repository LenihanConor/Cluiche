# Feature Spec: geometry2d-visual-debugger-stack

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Creates a new `DiaGeometry2DVisualDebugger` module with two `IVisualDebugger` draw classes. `ShapeDrawer` draws individual geometry shapes submitted by callers one at a time; `SpatialStructureDrawer` draws the internal cell/node structure of a spatial acceleration structure (`SpatialGrid`, `Quadtree`, `BVH`, or `HexGrid`). Both classes are general-purpose utilities rather than bound to a specific simulation system.

**Problem solved:** There is no mechanism to visualize `DiaGeometry2D` shapes or spatial structures during development. Developers testing intersection queries, spatial partitioning correctness, or collision shape placement must guess at world-space contents. These two classes make geometry state observable in any context where `DiaGeometry2D` is used.

---

## Acceptance Criteria

1. Two draw classes: `ShapeDrawer`, `SpatialStructureDrawer`
2. Each implements `IVisualDebugger` and lives in a new `DiaGeometry2DVisualDebugger.vcxproj` static library
3. Each stores a `const Dia::Debug::DebugLayerManager&` reference (set at construction)
4. `ShapeDrawer` accepts shapes one at a time via `Submit*()` methods per frame; `Draw(FrameData&)` flushes the buffer
5. `SpatialStructureDrawer` is a template class — `SpatialStructureDrawer<T>` holds a `const ISpatialStructure<T>&` by reference
6. `Draw(FrameData&)` uses `DebugColourPalette` constants — no ad-hoc colour literals
7. All sizes multiplied by `DebugLayerManager::GetDebugScale()` where applicable
8. Canonical layer names from `DebugLayerNames.h` used at `GetLayerName()` return
9. New `DiaGeometry2DVisualDebugger.vcxproj` added to `Cluiche.sln`
10. Build with no warnings; all tests pass

---

## Draw Classes

### 1. ShapeDrawer (`LayerNames::kGeometryShapes`, priority 10)

Accepts geometry shapes submitted each frame and draws them as debug primitives. Shapes are submitted via typed `Submit*()` methods before each `Draw()` call; the buffer is cleared at the start of each `Draw()`.

```cpp
class ShapeDrawer : public Dia::Debug::IVisualDebugger
{
public:
    explicit ShapeDrawer(const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override { return Dia::Debug::LayerNames::kGeometryShapes; }
    void Draw(Dia::Graphics::FrameData& frameData) override;

    // Submit shapes before Draw() each frame. Up to kMaxShapes total.
    void SubmitCircle     (const Dia::Geometry2D::Circle&         shape, Dia::Graphics::RGBA colour);
    void SubmitAARect     (const Dia::Geometry2D::AARect&          shape, Dia::Graphics::RGBA colour);
    void SubmitLine       (const Dia::Geometry2D::Line&            shape, Dia::Graphics::RGBA colour);
    void SubmitRay        (const Dia::Geometry2D::Ray&             shape, float displayLength, Dia::Graphics::RGBA colour);
    void SubmitTriangle   (const Dia::Geometry2D::Triangle&        shape, Dia::Graphics::RGBA colour);
    void SubmitConvexPoly (const Dia::Geometry2D::ConvexPolygon&   shape, Dia::Graphics::RGBA colour);

private:
    static constexpr int kMaxShapes = 256;

    struct ShapeEntry { /* tagged union of shape data + colour + type enum */ };
    Dia::Core::Containers::DynamicArrayC<ShapeEntry, kMaxShapes> mPending;
    const Dia::Debug::DebugLayerManager& mManager;
};
```

Draw logic per shape type:
- `Circle`: `RequestDraw(center, radius * scale_factor, colour)` — note: scale does not distort world-space radius; radius is used as-is, scale only applies to decorative overlay sizes
- `AARect`: `RequestDrawRect(bottomLeft, topRight, colour)`
- `Line`: `RequestDraw(pt1, pt2, colour)`
- `Ray`: `RequestDrawRay(origin, direction, displayLength * scale, colour)`
- `Triangle`: `RequestDraw(pt0, pt1, pt2, colour, transparent_fill)`
- `ConvexPolygon`: iterate edges 0→N-1, one `RequestDraw(v_i, v_{i+1 % n}, colour)` per edge

`Draw()` clears `mPending` at start so callers re-submit each frame. If `mPending` is full, additional submits are silently dropped (consistent with debug-budget behaviour).

### 2. SpatialStructureDrawer (`LayerNames::kGeometrySpatial`, priority 5)

Template class that draws the internal cell/node structure of any `ISpatialStructure<T>` implementation. Priority 5 — drawn under shapes so shape outlines appear on top.

```cpp
template <typename T>
class SpatialStructureDrawer : public Dia::Debug::IVisualDebugger
{
public:
    SpatialStructureDrawer(const ISpatialStructure<T>& structure,
                           const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override { return Dia::Debug::LayerNames::kGeometrySpatial; }
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const ISpatialStructure<T>&          mStructure;
    const Dia::Debug::DebugLayerManager& mManager;
};
```

Draw logic — dispatched at compile time by specialization or by querying the concrete type at runtime:

**SpatialGrid**: iterate all cells; for each cell draw its `AARect` bounds as `RequestDrawRect(cellMin, cellMax, DebugColourPalette::kInactive)` — grey grid lines.

**Quadtree**: recursive tree walk from root node; draw each node's `bounds` as `RequestDrawRect(bounds.min, bounds.max, colour)`:
- Leaf nodes: `kActive` (white) — occupied leaves are most relevant
- Internal nodes: `kInactive` (grey) — structural hierarchy

**BVH** (only if `IsBuilt()`): recursive tree walk; colour by depth level cycling through `kActive` → `kGoal` → `kHealthy` → `kWarning` (four distinct colours, repeat for deeper nodes):
- `RequestDrawRect(node.bounds.min, node.bounds.max, depthColour[depth % 4])`

**HexGrid**: iterate all valid `HexCoord` cells; for each cell compute the 6 corner vertices of the hexagon (pointy-top orientation from `HexToWorld()`) and draw 6 edge lines using `RequestDraw(cornerA, cornerB, DebugColourPalette::kInactive)`.

Note: `SpatialStructureDrawer` is header-only (template); it lives in `DiaGeometry2DVisualDebugger.h` with the template definitions inline.

---

## Hexagon Vertex Calculation

For pointy-top hexagons with circumradius `hexRadius` at center `worldPos`:

```cpp
// 6 corner angles for pointy-top: 30, 90, 150, 210, 270, 330 degrees
static constexpr float kHexAngles[6] = {
    Dia::Maths::PI / 6.0f,       // 30°
    Dia::Maths::PI / 2.0f,       // 90°
    5.0f * Dia::Maths::PI / 6.0f,// 150°
    7.0f * Dia::Maths::PI / 6.0f,// 210°
    3.0f * Dia::Maths::PI / 2.0f,// 270°
    11.0f * Dia::Maths::PI / 6.0f// 330°
};
// corner_i = worldPos + Vector2D(cos(angle_i), sin(angle_i)) * hexRadius
```

Draw as 6 lines: `corner[i]` → `corner[(i+1) % 6]`.

---

## Registration Example (game code)

```cpp
static ShapeDrawer shapesDrawer(manager);
static SpatialStructureDrawer<GameObject*> gridDrawer(spatialGrid, manager);

manager.Register(&shapesDrawer, 10);
manager.Register(&gridDrawer,    5);

// Each frame:
shapesDrawer.SubmitCircle(someCircle, DebugColourPalette::kWarning);
shapesDrawer.SubmitAARect(queryBounds, DebugColourPalette::kGoal);
manager.Draw(frameData);
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaGeometry2DVisualDebugger/ShapeDrawer.h` | New |
| `Dia/DiaGeometry2DVisualDebugger/ShapeDrawer.cpp` | New |
| `Dia/DiaGeometry2DVisualDebugger/SpatialStructureDrawer.h` | New (template, header-only) |
| `Dia/DiaGeometry2DVisualDebugger/DiaGeometry2DVisualDebugger.vcxproj` | New project |
| `Dia/DiaGeometry2DVisualDebugger/DiaGeometry2DVisualDebugger.vcxproj.filters` | New filters |
| `Cluiche/Cluiche.sln` | Add new project |

**Prerequisites:** `debug-budget`, `debug-text-primitive`, `debug-layer-manager` all implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Create `DiaGeometry2DVisualDebugger/` directory and new vcxproj | Modelled on `DiaRigidBody2DVisualDebugger.vcxproj` |
| 2 | Write `ShapeDrawer.h/.cpp` — `ShapeEntry` union, `Submit*()` methods, `Draw()` flush | 6 shape types |
| 3 | Write `SpatialStructureDrawer.h` — template class with `Draw()` dispatching by 4 concrete types | Header-only |
| 4 | Add project to `Cluiche.sln` | |
| 5 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 6 | Write tests | `TestGeometry2DVisualDebuggerStack.cpp` |
| 7 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/Geometry2D/TestGeometry2DVisualDebuggerStack.cpp`

| Suite | Test | What it verifies |
|-------|------|-----------------|
| ShapeDrawer | `SubmitCircle_ThenDraw_DrawsCircle` | 1 circle submit → 1 circle primitive |
| ShapeDrawer | `SubmitLine_ThenDraw_DrawsLine` | 1 line submit → 1 line primitive |
| ShapeDrawer | `SubmitRay_ThenDraw_DrawsRay` | 1 ray submit → 1 ray primitive |
| ShapeDrawer | `SubmitAARect_ThenDraw_DrawsRect` | 1 rect submit → 1 rect primitive |
| ShapeDrawer | `SubmitTriangle_ThenDraw_DrawsTriangle` | 1 triangle submit → 1 triangle primitive |
| ShapeDrawer | `SubmitConvexPoly_FourVerts_DrawsFourLines` | 4-vertex polygon → 4 line primitives |
| ShapeDrawer | `Draw_ClearsBuffer_NoPrimitivesNextDraw` | Submit then draw; second draw with no submits → 0 primitives |
| ShapeDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| ShapeDrawer | `LayerName_IsGeometryShapes` | `GetLayerName() == LayerNames::kGeometryShapes` |
| SpatialStructureDrawer (Grid) | `Draw_SpatialGrid_DrawsCells` | Grid with N cells → N rect primitives |
| SpatialStructureDrawer (Grid) | `Draw_Colour_IsInactive` | Cell rects are `kInactive` |
| SpatialStructureDrawer (Grid) | `LayerName_IsGeometrySpatial` | `GetLayerName() == LayerNames::kGeometrySpatial` |
| SpatialStructureDrawer (Quadtree) | `Draw_Quadtree_DrawsNodes` | Quadtree with known depth → correct node count |
| SpatialStructureDrawer (BVH) | `Draw_BVH_NotBuilt_NoPrimitives` | `IsBuilt() == false` → 0 primitives |
| SpatialStructureDrawer (BVH) | `Draw_BVH_Built_DrawsNodes` | Built BVH → at least 1 rect primitive |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — layer names use `LayerNames::kGeometry*` constants (StringCRC) |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — draw classes are plain C++ objects |
| PD-003 | Component-based entities | Compliant — no entity ID concerns |
| PD-004 | No STL in public APIs | Compliant — `Submit*()` takes const references to Geometry2D types; `DynamicArrayC` used for buffer |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — new vcxproj created and added to solution |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant |
| PD-009 | Generated output under `Cluiche/out/` | Compliant |
| AD-001 | Module YAML frontmatter | Compliant — new `dia.geometry2dvisualdebugger.architecture.module.md` created |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all classes in `Dia::Geometry2D::` namespace |
| SD-DBG-001 | Stack of focused draw classes | Compliant — two classes registered independently |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — project not linked in Release |
| SD-DBG-003 | Priority-ordered draw | Compliant — spatial at 5 (under shapes), shapes at 10 |
| SD-DBG-005 | Global `debugScale` | Compliant — display lengths multiplied by `mManager.GetDebugScale()` |
| SD-DBG-006 | Assert on name collision | Compliant — each drawer returns a distinct `LayerNames::kGeometry*` constant |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all colours from palette |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — both geometry draw classes in `DiaGeometry2DVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | ShapeDrawer per-frame submit model | `ShapeDrawer` requires re-submitting shapes each frame. This differs from the reference-based model used by all other draw classes (which hold a `const World&` and iterate it). Is this the right model for geometry? | Yes — geometry shapes in `DiaGeometry2D` are standalone value types, not owned by a world/simulation object. There is no "geometry world" to hold a reference to. The submit-per-frame model is the natural fit and matches how callers use shapes. |
| 2 | `SpatialStructureDrawer` type dispatch | The template class must dispatch `Draw()` behaviour differently for `SpatialGrid`, `Quadtree`, `BVH`, and `HexGrid`. Since `ISpatialStructure<T>` doesn't expose internal node structure, the draw class must hold a reference to the concrete type, not the interface. Should the template parameter be the concrete type instead of `T`? | The template parameter `T` is the object type (e.g., `GameObject*`). The concrete spatial structure type is what the caller passes to the constructor. Use a separate non-type approach: provide four concrete non-template draw classes (`SpatialGridDrawer<T>`, `QuadtreeDrawer<T>`, `BVHDrawer<T>`, `HexGridDrawer<T>`) instead of one base template — each directly holds the concrete type. This removes the need for runtime dispatch and is simpler to test. Update the class design above accordingly at implementation time. |
| 3 | `ShapeEntry` union size | `ShapeEntry` must hold the largest shape type: `ConvexPolygon` (16 × `Vector2D` = 128 bytes). With `kMaxShapes = 256`, the buffer is ~33 KB on the stack. Is this within budget? | 33 KB is at the outer edge of typical stack frame budget but acceptable for a debug-only draw class that is rarely active simultaneously with physics and IK draws. If stack pressure is a concern, reduce `kMaxShapes` to 128 (≈16 KB). |
| 4 | debugScale and world-space shapes | Circle radius, AARect bounds, line endpoints are world-space values — multiplying them by `debugScale` would distort the shape. Only `Ray` `displayLength` should be scaled. Is this correctly handled? | Yes — the spec states `displayLength * scale` for rays and "sizes multiplied by debugScale where applicable." World-space shape data (positions, radii, vertices) is drawn as-is. debugScale only affects decorative lengths like ray display length, not physical extents. |
| 5 | HexGrid vertex calculation — `hexRadius` access | `HexGrid::Def` stores `hexRadius` but the `HexGrid` class may not expose it via a public accessor. Does `HexGrid` expose `GetHexRadius()` or equivalent? | Must be verified at implementation time. If no accessor exists, add `float GetHexRadius() const` to `HexGrid.h`. It returns `mHexRadius` (precomputed from `def.hexRadius`). |

---

## Status

`Approved`
