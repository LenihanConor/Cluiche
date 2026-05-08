# Feature Spec: BVH (Bounding Volume Hierarchy)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diageometry2d.md | **bvh** |

**Status:** `Approved`

---

## Problem Statement

Static geometry (level terrain, walls, platforms, trigger zones) is fixed after level load. A uniform grid wastes memory on empty cells and a quadtree can be suboptimal for sparse, large-footprint objects. A BVH provides the tightest possible axis-aligned bounding boxes around groups of objects, giving fast queries over static scenes regardless of object size or distribution. Once built, a BVH requires no per-frame maintenance.

---

## Solution Overview

Implement `BVH<T>` as a static axis-aligned bounding volume hierarchy. The tree is built once from a batch of objects using a Surface Area Heuristic (SAH) split strategy, which produces near-optimal trees for raycast and region queries. After `Build()`, objects cannot be added or removed — `Rebuild()` from scratch is required if the object set changes.

`BVH<T>` implements `ISpatialStructure<T>` (defined in spatial-grid feature). Insert/Remove/Update on a built BVH assert — callers that need dynamic updates should use `SpatialGrid` or `Quadtree` instead.

### Key Design Points

1. **Static only** — `Build(objects, bounds)` constructs the tree; `Insert`/`Remove`/`Update` assert in debug
2. **SAH split** — Surface Area Heuristic partitions objects at each node to minimise expected query cost; better tree quality than median or midpoint splits
3. **`Handle<T>` identity** — handles are assigned during `Build()` and remain valid until `Rebuild()`
4. **Leaf node object count** — configurable; default 4 objects per leaf (balance between tree depth and per-leaf test count)
5. **No STL in public API** — query output into `DynamicArrayC<Handle<T>>`; build input from `DynamicArrayC<BuildEntry>`
6. **Implements `ISpatialStructure<T>`** — for injection into DiaRigidBody2D static-body broad-phase

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `BVH<T>` exists in `Dia/DiaGeometry2D/Spatial/BVH.h/.cpp` | File system check |
| AC2 | `BVH<T>` implements `ISpatialStructure<T>` | Build check |
| AC3 | `Build()` constructs a valid tree from a batch of objects | Unit test: build then query |
| AC4 | `QueryRegion` returns all objects whose bounds overlap the query region | Unit test |
| AC5 | `QueryCircle` returns all objects whose bounds overlap the query circle | Unit test |
| AC6 | `QueryPoint` returns all objects whose bounds contain the point | Unit test |
| AC7 | `QueryRay` returns all objects whose bounds the ray intersects, in front-to-back order | Unit test |
| AC8 | `QueryKNearest` returns k closest objects by centroid distance | Unit test |
| AC9 | `Insert()` called after `Build()` fires `DIA_ASSERT` | Unit test |
| AC10 | `Resolve(handle)` returns object pointer for valid handle; nullptr for invalid | Unit test |
| AC11 | `Rebuild()` clears tree and rebuilds from updated batch | Unit test: build, rebuild with different set, old handles invalid |
| AC12 | SAH tree depth is shallower than median-split for non-uniform distributions | Performance test / tree depth comparison |
| AC13 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::Geometry2D {

struct BVHDef {
    int maxLeafObjects; // Objects per leaf node before stopping (default: 4)
    int maxObjects;     // Pre-allocated slot pool size
};

template<typename T>
class BVH : public ISpatialStructure<T> {
public:
    struct BuildEntry {
        T      object;
        AARect bounds;
    };

    explicit BVH(const BVHDef& def);

    // Build from batch (assigns handles; clears any previous tree)
    void Build(const Dia::Core::DynamicArrayC<BuildEntry>& entries);
    void Rebuild(const Dia::Core::DynamicArrayC<BuildEntry>& entries);  // alias for clarity
    bool IsBuilt() const;

    // ISpatialStructure<T> — query interface
    void QueryRegion  (const AARect& region,               Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryCircle  (const Circle& circle,               Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryPoint   (const Dia::Maths::Vector2D& point,  Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryRay     (const Ray& ray,                     Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;
    void QueryKNearest(const Dia::Maths::Vector2D& point, int k,
                       Dia::Core::DynamicArrayC<Dia::Core::Handle<T>>& out) const override;

    const T* Resolve(Dia::Core::Handle<T> handle) const override;

    // ISpatialStructure<T> — mutation (asserts — BVH is static)
    Dia::Core::Handle<T> Insert(const T& object, const AARect& bounds) override;  // DIA_ASSERT
    void                 Remove(Dia::Core::Handle<T> handle)           override;  // DIA_ASSERT
    void                 Update(Dia::Core::Handle<T> handle, const AARect& newBounds) override;  // DIA_ASSERT
    void                 Clear()                                        override;  // valid — clears tree

    // BVH diagnostics
    int GetNodeCount()   const;
    int GetLeafCount()   const;
    int GetObjectCount() const;
    int GetDepth()       const;
};

} // namespace Dia::Geometry2D
```

---

## Implementation Notes

### Node Structure

```cpp
struct BVHNode {
    AARect bounds;           // Tight AABB around all objects in subtree
    int    leftChild;        // Index into node pool; -1 = leaf
    int    rightChild;
    int    objectStart;      // Index into sorted object array (leaf only)
    int    objectCount;      // Number of objects in leaf (0 = internal node)
};
```

All nodes stored in a flat array. Leaf nodes reference a contiguous range in the sorted object array — no per-node dynamic allocation.

### SAH Build Algorithm

```
BuildNode(node, objectRange):
  if objectRange.count <= maxLeafObjects:
    MakeLeaf(node, objectRange)
    return

  // Find best split axis and position using SAH
  bestCost = Infinity
  for axis in {X, Y}:
    Sort objects by centroid along axis
    for each candidate split position:
      leftBounds  = AABB of objects[0..split]
      rightBounds = AABB of objects[split..end]
      cost = leftBounds.SurfaceArea()  * leftCount
           + rightBounds.SurfaceArea() * rightCount
      if cost < bestCost: record split

  Partition objects at best split
  BuildNode(leftChild,  leftRange)
  BuildNode(rightChild, rightRange)
```

SAH cost approximates expected raycast traversal cost — minimising it produces trees where large empty regions are quickly skipped.

### Ray Query (front-to-back traversal)

```
QueryRay(node, ray, out):
  if !ray.Intersects(node.bounds): return
  if node.isLeaf:
    for each object in node: out.Add(handle)
    return
  // Visit closer child first (front-to-back order)
  if DistToChild(left) < DistToChild(right):
    QueryRay(left, ray, out)
    QueryRay(right, ray, out)
  else:
    QueryRay(right, ray, out)
    QueryRay(left, ray, out)
```

### K-Nearest (best-first traversal)

Use a min-heap of `(distToNode, nodeIndex)`. Pop nearest node; if leaf, add objects. Prune nodes whose nearest corner exceeds k-th distance found so far.

### File Layout

```
Dia/DiaGeometry2D/Spatial/
├── ISpatialStructure.h   (spatial-grid feature)
├── BVH.h
└── BVH.cpp
```

---

## Dependencies

### Prerequisite
- **`Dia::Core::Handle<T>`** — DiaCore slot pool type
- **spatial-grid** — `ISpatialStructure<T>` interface

### Required Features
- **shape-primitives** — `AARect`, `Circle`, `Ray`

### Required Modules
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`
- **DiaMaths** — `Vector2D`

### Dependent Features
- **DiaRigidBody2D / collision-detection** — can inject `BVH<PhysicsBody*>` as broad-phase for static body sets (terrain, walls)
- Future level-streaming / visibility systems — BVH ideal for static scene queries

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/Geometry2D/TestBVH.cpp`)

1. Build from empty array — `IsBuilt()` true; all queries return nothing
2. Build from single object — `QueryRegion` containing it returns handle
3. Build from N objects — `QueryRegion` returns only overlapping objects
4. `QueryRay` front-to-back — first hit is the nearest object to ray origin
5. `QueryKNearest` — correct k results, sorted by distance
6. `QueryCircle` and `QueryPoint` — correct results vs brute force reference
7. `Insert()` after `Build()` fires `DIA_ASSERT`
8. `Rebuild()` — old handles stale; new handles valid
9. SAH vs median split — for 1000 uniformly-random objects, SAH tree has lower average depth
10. ISpatialStructure substitution — `BVH<int>` and `SpatialGrid<int>` produce identical query results for the same static object set

### Performance Smoke Test
- Build BVH over 10,000 objects; time `QueryRay` for 1000 rays
- Must complete in < 1ms per query on debug build (sanity check, not a hard requirement)

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | ✅ `DynamicArrayC` for all input/output; flat node array |
| PD-007 | Platform | C++20 required | ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ `Dia::Geometry2D::` |
| SD-004 | System | Spatial structures templated on stored type | ✅ `BVH<T>` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Static constraint | What if a caller accidentally uses BVH for dynamic objects? | `DIA_ASSERT` on `Insert`/`Remove`/`Update` in debug. In release, document clearly: BVH is for static sets only. If objects need to move, use SpatialGrid or Quadtree. |
| 2 | SAH binned approximation | Full SAH tests every object as a split candidate (O(n log n) per node). Should a binned approximation (O(n) per node, ~32 bins) be used for large builds? | Binned SAH for v1 — faster build, negligible quality loss in practice. Full SAH deferred unless build time is measured as a problem. |
| 3 | Build time | How long should `Build()` take? | For 10,000 objects, target < 5ms. Not a hard requirement in v1 — document if violated. Level load is the expected call site. |
| 4 | Ray order | Is front-to-back ordering guaranteed for `QueryRay`? | Yes — the traversal visits closer child first. Output array order is front-to-back by node visit order, not by exact hit distance. If exact order is needed, caller sorts by `ContactResult.depth`. |
| 5 | Two-BVH injection | DiaRigidBody2D spec mentions possibly injecting two structures (dynamic grid + static BVH). Does ISpatialStructure support this? | Not directly — `ISpatialStructure` is a single-structure interface. A `CompositeSpatialStructure<T>` wrapper that queries two structures and merges results could be added as a follow-up feature if needed. |
