# Feature Spec: SpatialGrid3D

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaGeometry3D | @docs/specs/applications/dia/systems/diageometry3d/diageometry3d.md |
| Feature | SpatialGrid3D | (this document) |

## Summary

Add `Dia::Geometry3D::ISpatialStructure3D<T>` interface and `Dia::Geometry3D::SpatialGrid3D<T, MaxObjects, MaxCells>` template — a uniform 3D grid spatial acceleration structure. Mirrors the structure of `Dia::Geometry2D::SpatialGrid<T, MaxObjects>` (`Dia/DiaGeometry2D/Spatial/SpatialGrid.h`) with a third template parameter for cell-count cap (per system SD-007) and a `QueryFrustum` query (per SD-008).

Six query types: `QueryRegion(AABB)`, `QuerySphere(Sphere)`, `QueryPoint(Vector3D)`, `QueryRay(Ray)`, **`QueryFrustum(Frustum)`** (the priority for rendering culling), `QueryKNearest(Vector3D, k)`. Slot pool with generation counters, LIFO free list, cell array of slot indices, visited bitset for query deduplication — all mirroring the 2D implementation.

## Problem

Without `SpatialGrid3D`, every 3D consumer (rendering culling, AI proximity queries, future physics broadphase) must either:
- Iterate every object linearly per frame (O(N) per query, no acceleration); or
- Build its own ad-hoc spatial partitioning.

The 2D module already proved the SpatialGrid pattern works at scale. Mirroring it in 3D, with `MaxCells` template parameter for density tuning and `QueryFrustum` for the rendering driver, is the unblock for every 3D consumer.

## Acceptance Criteria

### Interface

1. `ISpatialStructure3D<T>` lives in `Dia/DiaGeometry3D/Spatial/ISpatialStructure3D.h`.
2. Six query methods (Region, Sphere, Point, Ray, Frustum, KNearest) plus mutators (Insert, Remove, Update, Clear) and a Resolve method — exactly matching the API in the system spec `Public Interfaces` section.
3. `static constexpr unsigned int kMaxQueryResults = 1024;` defined in the same header — mirrors 2D `Dia::Geometry2D::kMaxQueryResults`.
4. All query results returned via caller-provided `Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>&` — no STL containers.

### SpatialGrid3D template

5. Template signature: `template<typename T, unsigned int MaxObjects = 2048, unsigned int MaxCells = 4096> class SpatialGrid3D : public ISpatialStructure3D<T>`.
6. `Def` struct: `AABB worldBounds; float cellSize;`. Constructor `explicit SpatialGrid3D(const Def& def)`.
7. **Slot pool:** `mSlots` is a `DynamicArrayC<Slot, MaxObjects>` where `Slot { T object; AABB bounds; uint32_t generation; bool occupied; }`. Generation counter starts at 0 in unused slots and bumps on every Insert.
8. **Free list:** LIFO `uint32_t mFreeList[MaxObjects]; int mFreeCount;`. Initialised highest index first so the first Insert returns slot 0.
9. **Cell grid:** dense `mCells[MaxCells]` array of `DynamicArrayC<uint32_t, kMaxObjectsPerCell>` where `kMaxObjectsPerCell = 64` (mirrors 2D).
10. **Cell index math:** `cellIdx = cz * (cellCountX * cellCountY) + cy * cellCountX + cx`. `WorldToCellClamped(x, y, z, outCx, outCy, outCz)` clamps to [0, cellCountX/Y/Z - 1]. Constructor computes `mCellCountX/Y/Z` from `worldBounds` extent / `cellSize` and asserts `cellCountX * cellCountY * cellCountZ <= MaxCells`.
11. **Insert:** pops a free slot, sets `occupied = true`, bumps generation (handles wraparound: if generation rolls to zero, advance to one — generation 0 means "never used"), inserts the slot index into every cell its AABB overlaps, returns `Handle<T>(slotIdx, generation)`.
12. **Remove:** asserts the handle is valid (slot occupied AND generation matches), removes slot index from each cell it overlaps, marks slot unoccupied, pushes slot index back onto free list, leaves generation unchanged so a re-issued slot has a new generation.
13. **Update:** asserts the handle is valid, removes slot index from old cells, updates `Slot::bounds`, inserts into new cells.
14. **Resolve:** returns pointer to `Slot::object` if handle is valid; returns nullptr if generation mismatches or slot is unoccupied.

### Query implementations

15. **`QueryRegion(AABB)`**: compute cell range overlapping the region AABB, iterate every slot index in those cells, deduplicate via a stack-allocated visited bitset of size `MaxObjects`, append `Handle<T>(slotIdx, generation)` to `out` for each unique slot whose `bounds` intersects the region (uses `IntersectionTests::Test(AABB, AABB)`).
16. **`QuerySphere(Sphere)`**: compute cell range overlapping the sphere's AABB bound, iterate as above, filter via `IntersectionTests::Test(AABB, Sphere)`.
17. **`QueryPoint(Vector3D)`**: compute single cell, iterate slots in that cell, filter via `IntersectionTests::Contains(AABB, point)`.
18. **`QueryRay(Ray)`**: walk every cell the ray passes through using a 3D Amanatides–Woo DDA traversal, iterate slots in each visited cell, dedup, filter via `IntersectionTests::Test(Ray, AABB)`.
19. **`QueryFrustum(Frustum)`** — **the priority query**: compute the frustum's AABB bound (via `Frustum::CalculateAABB`), iterate cells in that AABB, **frustum-test each cell's AABB to skip wholly-outside cells** (per system Q7 and user direction), then per-object frustum-test inside surviving cells using `IntersectionTests::Test(AABB, Frustum)`.
20. **`QueryKNearest(Vector3D point, int k)`**: expanding-ring traversal — start at `point`'s cell, expand in concentric cell shells, gather slots, sort by squared distance from point to slot AABB center, return first `k`. Stops expansion when the current shell's minimum distance exceeds the kth-best slot's distance (early termination).

### Debug accessors (read-only — for the deferred visual debugger)

21. `GetCellCountX()`, `GetCellCountY()`, `GetCellCountZ()`, `GetCellSize()`, `GetWorldBounds()` — mirror the 2D module's accessor pattern (`SpatialGrid.h:62-65`).
22. `GetCellCount()`, `GetObjectCount()` — total counts.

### Project file integration

23. New files registered in `DiaGeometry3D.vcxproj` and `.vcxproj.filters` under `Spatial` filter.
24. `dia.geometry3d.architecture.module.md` updated to list `ISpatialStructure3D` and `SpatialGrid3D` in `public_api.entry_points`.

## API Design

```cpp
// Dia/DiaGeometry3D/Spatial/ISpatialStructure3D.h
namespace Dia::Geometry3D {

static constexpr unsigned int kMaxQueryResults = 1024;

template<typename T>
class ISpatialStructure3D
{
public:
    virtual ~ISpatialStructure3D() = default;

    virtual Dia::Core::Handle<T> Insert(const T& object, const AABB& bounds) = 0;
    virtual void Remove(Dia::Core::Handle<T> handle) = 0;
    virtual void Update(Dia::Core::Handle<T> handle, const AABB& newBounds) = 0;
    virtual void Clear() = 0;

    virtual void QueryRegion  (const AABB& region,                              Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QuerySphere  (const Sphere& sphere,                            Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryPoint   (const Dia::Maths::Vector3D& point,               Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryRay     (const Ray& ray,                                  Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryFrustum (const Frustum& frustum,                          Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;
    virtual void QueryKNearest(const Dia::Maths::Vector3D& point, int k,        Dia::Core::Containers::DynamicArrayC<Dia::Core::Handle<T>, kMaxQueryResults>& out) const = 0;

    virtual const T* Resolve(Dia::Core::Handle<T> handle) const = 0;
};

}  // namespace Dia::Geometry3D

// Dia/DiaGeometry3D/Spatial/SpatialGrid3D.h
namespace Dia::Geometry3D {

template<typename T, unsigned int MaxObjects = 2048, unsigned int MaxCells = 4096>
class SpatialGrid3D : public ISpatialStructure3D<T>
{
public:
    struct Def
    {
        AABB  worldBounds;
        float cellSize;
    };

    explicit SpatialGrid3D(const Def& def);

    // ISpatialStructure3D overrides ...

    int GetCellCount()   const;
    int GetObjectCount() const;
    int GetCellCountX()  const { return mCellCountX; }
    int GetCellCountY()  const { return mCellCountY; }
    int GetCellCountZ()  const { return mCellCountZ; }
    float GetCellSize()    const { return mCellSize; }
    const AABB& GetWorldBounds() const { return mWorldBounds; }

private:
    struct Slot
    {
        T        object;
        AABB     bounds;
        uint32_t generation;
        bool     occupied;
    };

    static constexpr int kMaxObjectsPerCell = 64;

    Dia::Core::Containers::DynamicArrayC<Slot, MaxObjects> mSlots;
    uint32_t mFreeList[MaxObjects];
    int      mFreeCount;
    int      mOccupiedCount;

    Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxObjectsPerCell> mCells[MaxCells];
    int   mCellCountX;
    int   mCellCountY;
    int   mCellCountZ;
    float mCellSize;
    AABB  mWorldBounds;

    bool WorldToCellClamped(float x, float y, float z, int& cx, int& cy, int& cz) const;
    void GetCellRange(const AABB& bounds, int& minCx, int& minCy, int& minCz,
                                            int& maxCx, int& maxCy, int& maxCz) const;
    bool IsHandleValid(Dia::Core::Handle<T> handle) const;
    void RemoveFromCells(uint32_t slotIdx, const AABB& bounds);
    void InsertIntoCells(uint32_t slotIdx, const AABB& bounds);
};

}  // namespace Dia::Geometry3D
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create `Dia/DiaGeometry3D/Spatial/ISpatialStructure3D.h` | Interface per AC 1–4 |
| 2 | Create `Dia/DiaGeometry3D/Spatial/SpatialGrid3DDef.h` | Optional: separate `Def` struct file if it grows. Otherwise inline in SpatialGrid3D.h. |
| 3 | Create `Dia/DiaGeometry3D/Spatial/SpatialGrid3D.h` | Class declaration |
| 4 | Create `Dia/DiaGeometry3D/Spatial/SpatialGrid3D.inl` | Constructor; Insert/Remove/Update/Clear/Resolve; QueryRegion/Sphere/Point |
| 5 | Implement `QueryRay` (3D DDA) | Amanatides–Woo cell traversal |
| 6 | Implement `QueryFrustum` (priority) | AABB-of-frustum prefilter + per-cell frustum test + per-object frustum test |
| 7 | Implement `QueryKNearest` (expanding-ring) | Concentric cell shells with early termination |
| 8 | Update `dia.geometry3d.architecture.module.md` | Add ISpatialStructure3D and SpatialGrid3D entries |
| 9 | Update `DiaGeometry3D.vcxproj` and `.vcxproj.filters` | Add Spatial filter |
| 10 | Add tests | `Cluiche/Tests/GoogleTests/Geometry3D/TestSpatialGrid3D.cpp` |
| 11 | Run `dia run googletest --filter="SpatialGrid3D*"` | All green |

## Test Plan (Task 10)

| Suite | Tests |
|-------|-------|
| `SpatialGrid3DBasicTest` | Construction with worldBounds (0,0,0)→(100,100,100) and cellSize=10 produces 10×10×10 grid; capacity assert; CellCount/X/Y/Z accessors |
| `SpatialGrid3DInsertTest` | Insert returns valid Handle; resolved object equals input; subsequent Insert advances generation |
| `SpatialGrid3DStaleHandleTest` | Insert → Remove → Insert (same slot reused, new generation) → Resolve(old handle) returns nullptr; Resolve(new handle) returns new object |
| `SpatialGrid3DMultiCellTest` | Object spanning multiple cells appears in QueryRegion exactly once (visited bitset dedup) |
| `SpatialGrid3DUpdateTest` | Insert object at A, Update bounds to B, Query at A returns nothing, Query at B returns object |
| `SpatialGrid3DQueryRegionTest` | Empty region, region containing all objects, region containing some |
| `SpatialGrid3DQuerySphereTest` | Sphere overlapping no objects, sphere overlapping AABB but not the embedded object detail (no false positives), tangent sphere |
| `SpatialGrid3DQueryPointTest` | Point inside an object's AABB, point outside all objects, point exactly on AABB face |
| `SpatialGrid3DQueryRayTest` | Ray hitting one object, ray hitting multiple in order, ray missing all, ray starting inside an object |
| `SpatialGrid3DQueryFrustumTest` | **The priority test.** Place 8 known objects (one per octant) in a 100×100×100 grid; construct a Y-up RH perspective frustum centered on a known eye/target; assert the correct subset is returned. Test inside-frustum, outside-frustum, partially-overlapping cases |
| `SpatialGrid3DKNearestTest` | k=1 finds nearest; k=N returns all objects sorted by distance; k > object count returns all |
| `SpatialGrid3DClearTest` | Insert N objects, Clear, GetObjectCount == 0, all handles invalidated |
| `SpatialGrid3DDenseGridTest` | Construct with `MaxCells=32768` template arg, verify 32×32×32 grid succeeds; default `MaxCells=4096` rejects 32³ at construction time |

## Files

| File | Action |
|------|--------|
| `Dia/DiaGeometry3D/Spatial/ISpatialStructure3D.h` | Create |
| `Dia/DiaGeometry3D/Spatial/SpatialGrid3D.h` | Create |
| `Dia/DiaGeometry3D/Spatial/SpatialGrid3D.inl` | Create |
| `Dia/DiaGeometry3D/Docs/dia.geometry3d.architecture.module.md` | Modify — add ISpatialStructure3D + SpatialGrid3D |
| `Dia/DiaGeometry3D/DiaGeometry3D.vcxproj` and `.vcxproj.filters` | Modify — add Spatial filter |
| `Cluiche/Tests/GoogleTests/Geometry3D/TestSpatialGrid3D.cpp` | Create |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` and `.filters` | Modify |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaGeometry3D — Shapes (this system) | AABB, Sphere, Ray, Frustum |
| DiaGeometry3D — IntersectionTests (this system) | Test(AABB, AABB), Test(AABB, Sphere), Test(Ray, AABB), Test(AABB, Frustum), Contains(AABB, point) |
| DiaMaths — Vector3D | Point queries, KNearest |
| DiaCore — `Handle<T>`, `DynamicArrayC` | Slot identity, query result containers |
| DiaCore — `DIA_ASSERT` | Capacity, handle validity, cell-count bound |

**Order:** Implements after `shape-primitives` and `intersection-tests` (same system spec). Must be the last DiaGeometry3D feature in the implementation chain.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** Uses `Handle<T>` for slot identity, which is compatible with StringCRC where consumers want it. |
| PD-004 | No STL in public API | **Compliant.** All public methods use `DynamicArrayC`, `Handle<T>`, DiaGeometry3D / DiaMaths types. |
| PD-005 | x64 only | **Compliant.** |
| PD-006 | VS project files source of truth | **Compliant.** |
| PD-007 | C++20 required | **Compliant.** Templates compile under /std:c++20. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** |
| AD-001 | Module YAML | **Compliant.** Architecture doc updated. |
| AD-002 | No STL public APIs | **Compliant.** |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** |
| SD-001 | `Dia::Geometry3D::` namespace | **Compliant.** |
| SD-004 | Spatial structures templated on stored type with `Handle<T>` identity | **Compliant — directly satisfies.** |
| SD-005 | No STL in public API | **Compliant.** |
| SD-006 | Y-up RH | **Compliant.** Frustum query honours RH inward-normal frustums. |
| SD-007 | `MaxCells` template parameter (default 4096) | **Compliant — directly satisfies.** |
| SD-008 | Six query types: Region, Sphere, Point, Ray, Frustum, KNearest | **Compliant — directly satisfies.** |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | QueryFrustum strategy | System spec Q7 + user feature interview chose AABB-of-frustum prefilter + per-cell frustum test + per-object frustum test. Why three levels? | Two-level culling already addresses the common case; three-level is what the user direction specified. The cost of the per-cell test is amortised across the cell's contents — for a cell with 64 objects, one frustum-vs-cell-AABB test replaces 64 frustum-vs-object-AABB tests when the cell is wholly outside. Profile if it becomes a hotspot. |
| 2 | DDA for QueryRay | Why Amanatides–Woo? | Standard, well-documented 3D voxel traversal. Visits exactly the cells the ray enters; no false positives on cell traversal. Mirrors the well-known 2D variant if it's used in the 2D module's QueryRay. |
| 3 | KNearest expanding-ring | Why not a priority queue / k-d tree-like approach? | Expanding-ring is appropriate for a uniform grid — cells are already laid out in a fixed pattern. A priority queue would add allocation overhead (banned by AC) and not improve worst-case complexity for uniform distributions. K-d tree is a different feature spec for a different acceleration structure. |
| 4 | Generation wraparound | What happens after 2^32 inserts into the same slot? | Generation wraps. The mitigation: when bumping generation, if it rolls to 0 (the "unused" sentinel), advance to 1. Documented in source. 2^32 inserts to one slot is years at typical game rates; not a realistic concern but the wraparound handling is cheap. |
| 5 | Capacity panic | What if Insert is called when free list is empty? | Asserts in debug, returns invalid Handle in release. Mirror of 2D module behaviour. Caller's responsibility to size MaxObjects appropriately. |
| 6 | MaxCells assertion | The constructor asserts `cellCountX * cellCountY * cellCountZ <= MaxCells`. What's the user-friendly message? | "Spatial grid cell count (X*Y*Z) exceeds MaxCells template argument. Reduce cellSize, expand worldBounds, or instantiate with a larger MaxCells." Documented in source. |
| 7 | QueryFrustum interaction with the `Frustum::CalculateAABB` method | This depends on `Frustum::CalculateAABB()` from shape-primitives. Confirmed it exists? | Yes — added to shape-primitives AC 19 / AI Review Q10 during the feature interview. |
| 8 | Object inside multiple cells | When an object spans cells, it's stored once per cell. Does dedup actually work? | Yes — visited bitset of size MaxObjects is stack-allocated at the start of each query, slot indices added to results are first marked in the bitset; duplicates are skipped. Mirrors 2D module exactly. |
| 9 | Threading | Is SpatialGrid3D thread-safe? | No. Same as 2D module — single-threaded by design. Concurrent reads from immutable state are safe (after construction); concurrent inserts/removes/queries are not. Documented. |
| 10 | Approval gate | What blocks Approval? | Hard prerequisite: shape-primitives + intersection-tests features implemented. Spec can be Approved on its own merits. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Last DiaGeometry3D feature; gates the system spec's `Done` status.
