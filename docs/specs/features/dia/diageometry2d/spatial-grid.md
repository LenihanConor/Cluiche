# Feature Spec: Spatial Grid

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diageometry2d.md | **spatial-grid** |

**Status:** `Approved`

---

## Problem Statement

Game systems need to efficiently query which objects occupy a region of space without testing every object every frame. A uniform spatial grid is the simplest acceleration structure — it divides the world into fixed-size cells and stores object references per cell. It is ideal for scenes with dense, uniformly distributed, frequently-moving objects (dynamic physics bodies, enemies, particles). Without it, spatial queries are O(n) over all objects.

This feature also defines `ISpatialStructure<T>` — the common interface that DiaRigidBody2D injects for its broad-phase, allowing Grid, Quadtree, and BVH to be swapped transparently.

---

## Solution Overview

Implement `SpatialGrid<T>` as a fixed-cell-size uniform grid over a defined world bounds. Objects are inserted with an `AARect` footprint and identified by `Dia::Core::Handle<T>`. The grid supports five query types: AARect region, Circle region, Point, Ray, and K-nearest. All query results are written into caller-provided `DynamicArrayC` output buffers.

Also define `ISpatialStructure<T>` as the abstract base class shared by Grid, Quadtree, and BVH.

### Key Design Points

1. **`Handle<T>` identity** — insert returns a `Handle<T>`; remove and update take the same handle; stale handles are detected via generation counter
2. **Fixed cell size** — set at construction; world divided into `ceil(worldSize / cellSize)` cells per axis
3. **Object-to-cell mapping** — objects spanning multiple cells are inserted into all overlapping cells (reference duplication, not object duplication)
4. **No STL in public API** — query results written into `Dia::Core::DynamicArrayC<Handle<T>>` passed by caller
5. **`ISpatialStructure<T>` defined here** — Grid is the first implementor; Quadtree and BVH implement the same interface

**Prerequisite:** `Dia::Core::Handle<T>` must be implemented in DiaCore before this feature can be built.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `ISpatialStructure<T>` interface exists in `Dia/DiaGeometry2D/Spatial/ISpatialStructure.h` | File system check |
| AC2 | `SpatialGrid<T>` exists in `Dia/DiaGeometry2D/Spatial/SpatialGrid.h/.cpp` | File system check |
| AC3 | `Insert()` returns a valid `Handle<T>` | Unit test |
| AC4 | `Remove()` with valid handle removes the object; subsequent query does not return it | Unit test |
| AC5 | `Remove()` with stale handle is a no-op (no crash) | Unit test |
| AC6 | `Update()` moves object to correct new cells | Unit test: insert, move bounds, query old and new positions |
| AC7 | `QueryRegion(AARect)` returns all objects whose bounds overlap the region | Unit test |
| AC8 | `QueryCircle(Circle)` returns all objects whose bounds overlap the circle | Unit test |
| AC9 | `QueryPoint(Vector2D)` returns all objects whose bounds contain the point | Unit test |
| AC10 | `QueryRay(Ray)` returns all objects whose bounds the ray intersects | Unit test |
| AC11 | `QueryKNearest(point, k)` returns the k closest objects by centroid distance | Unit test: insert 10 objects, query k=3, verify order |
| AC12 | Objects spanning multiple cells are returned once per query (no duplicates) | Unit test |
| AC13 | `Dia::Core::Handle<T>` implemented in DiaCore | Build check |
| AC14 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
// --- ISpatialStructure<T> (Dia/DiaGeometry2D/Spatial/ISpatialStructure.h) ---

namespace Dia::Geometry2D {

template<typename T>
class ISpatialStructure {
public:
    virtual ~ISpatialStructure() = default;

    virtual Dia::Core::Handle<T> Insert(const T& object, const AARect& bounds) = 0;
    virtual void                 Remove(Dia::Core::Handle<T> handle) = 0;
    virtual void                 Update(Dia::Core::Handle<T> handle, const AARect& newBounds) = 0;
    virtual void                 Clear() = 0;

    // Queries — results written into caller-provided output array
    virtual void QueryRegion (const AARect&              region, Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const = 0;
    virtual void QueryCircle (const Circle&              circle, Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const = 0;
    virtual void QueryPoint  (const Dia::Maths::Vector2D& point, Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const = 0;
    virtual void QueryRay    (const Ray&                 ray,    Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const = 0;
    virtual void QueryKNearest(const Dia::Maths::Vector2D& point, int k,
                                Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const = 0;

    // Resolve handle back to stored object (returns nullptr if stale)
    virtual const T* Resolve(Dia::Core::Handle<T> handle) const = 0;
};

} // namespace Dia::Geometry2D


// --- SpatialGrid<T> (Dia/DiaGeometry2D/Spatial/SpatialGrid.h) ---

namespace Dia::Geometry2D {

struct SpatialGridDef {
    AARect worldBounds;   // Total grid extent
    float  cellSize;      // Width/height of each cell
    int    maxObjects;    // Pre-allocated slot pool size
};

template<typename T>
class SpatialGrid : public ISpatialStructure<T> {
public:
    explicit SpatialGrid(const SpatialGridDef& def);

    // ISpatialStructure<T> implementation
    Dia::Core::Handle<T> Insert(const T& object, const AARect& bounds) override;
    void                 Remove(Dia::Core::Handle<T> handle)           override;
    void                 Update(Dia::Core::Handle<T> handle, const AARect& newBounds) override;
    void                 Clear()                                        override;

    void QueryRegion  (const AARect& region,               Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryCircle  (const Circle& circle,               Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryPoint   (const Dia::Maths::Vector2D& point,  Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryRay     (const Ray& ray,                     Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryKNearest(const Dia::Maths::Vector2D& point, int k,
                       Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;

    const T* Resolve(Dia::Core::Handle<T> handle) const override;

    // Grid-specific diagnostics
    int GetCellCount()   const;
    int GetObjectCount() const;
};

} // namespace Dia::Geometry2D
```

---

## Implementation Notes

### Handle\<T\> (DiaCore prerequisite)

```cpp
// Dia/DiaCore/Containers/Handle.h
namespace Dia::Core {
    template<typename T>
    struct Handle {
        static constexpr Handle Invalid() { return { 0, 0 }; }
        bool IsValid() const { return index != 0; }

        uint32_t index;       // Slot index (1-based; 0 = invalid)
        uint32_t generation;  // Generation counter for stale detection
    };
}
```

The `SpatialGrid` (and all spatial structures) internally maintain a slot pool:
```
SlotPool:
  slot[i] = { T object, AARect bounds, uint32_t generation, bool occupied }

Handle.index     → slot index
Handle.generation → must match slot[index].generation to be valid
```

On `Remove(handle)`: mark slot as unoccupied, increment `slot.generation`. Old handles now fail the generation check.

### Cell Indexing

```cpp
int CellX(float x) const { return (int)((x - mBounds.GetMin().x) / mCellSize); }
int CellY(float y) const { return (int)((y - mBounds.GetMin().y) / mCellSize); }
int CellIndex(int cx, int cy) const { return cy * mCellCountX + cx; }
```

Each cell holds a `DynamicArrayC<uint32_t>` of slot indices. Objects spanning N cells have their slot index in N cell lists.

### Duplicate Prevention in Queries

Use a visited bitset (stack-allocated for small counts, dynamic for large) to ensure each slot index is added to output at most once per query call.

### K-Nearest Implementation

Expand AARect search region outward from point in cell increments until `k` objects found, then sort by centroid distance. No heap allocation — sort in the caller-provided output array.

### File Layout

```
Dia/DiaGeometry2D/Spatial/
├── ISpatialStructure.h
├── SpatialGrid.h
└── SpatialGrid.cpp
```

---

## Dependencies

### Prerequisite (DiaCore — must exist first)
- **`Dia::Core::Handle<T>`** — new type in `Dia/DiaCore/Containers/Handle.h`

### Required Features
- **shape-primitives** — `AARect`, `Circle`, `Ray` used for queries

### Required Modules
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`
- **DiaMaths** — `Vector2D`

### Dependent Features
- **quadtree**, **bvh** — both implement `ISpatialStructure<T>` defined here
- **DiaRigidBody2D / collision-detection** — injects `ISpatialStructure<PhysicsBody*>` for broad-phase

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/Geometry2D/TestSpatialGrid.cpp`)

1. Insert single object; `QueryRegion` covering it returns its handle
2. Insert single object; `QueryRegion` not covering it returns nothing
3. Insert object; `Remove`; query returns nothing
4. Stale handle after `Remove` — `Resolve()` returns nullptr
5. Object spanning 4 cells — returned once per query (no duplicates)
6. `Update` — object moves; old position query returns nothing; new position query returns handle
7. `QueryCircle` — objects inside circle returned; objects outside not
8. `QueryPoint` — only objects whose bounds contain the point returned
9. `QueryRay` — ray through row of objects returns all intersecting
10. `QueryKNearest(point, 3)` — returns 3 closest in correct order
11. `Clear` — all subsequent queries return nothing

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | ✅ `DynamicArrayC` for output; fixed-size internal arrays |
| PD-007 | Platform | C++20 required | ✅ Template class; compiles under C++20 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ `Dia::Geometry2D::` |
| SD-004 | System | Spatial structures templated on stored type | ✅ `SpatialGrid<T>` |
| SD-005 | System | No STL in public API | ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Handle | Where does `Handle<T>` live — DiaCore or DiaGeometry2D? | DiaCore — it's a general utility, not geometry-specific. `SpatialGrid`, `Quadtree`, and `BVH` all use it. |
| 2 | World bounds | What happens when an object is inserted outside `worldBounds`? | `DIA_ASSERT` in debug; in release, clamp to boundary cells. Document the precondition clearly. |
| 3 | Cell size | How does the caller choose a good cell size? | Rule of thumb: 2–4× the average object diameter. Document in the header. No automatic tuning in v1. |
| 4 | QueryKNearest | Is sorting in the output array acceptable if the array is large? | Fine for k ≤ ~50 (typical use case). If k is large, consider a partial sort. Document the recommendation. |
| 5 | Thread safety | Should queries be thread-safe for concurrent reads? | Read queries are safe to call concurrently if no writes occur simultaneously. Writes (`Insert`, `Remove`, `Update`) are not thread-safe — caller must synchronise. Document this clearly. |
