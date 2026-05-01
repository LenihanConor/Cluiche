# Feature Spec: Quadtree

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diageometry2d.md | **quadtree** |

**Status:** `Approved`

---

## Problem Statement

A uniform spatial grid performs poorly when objects are unevenly distributed — sparse regions waste memory and dense regions produce oversized candidate sets. A quadtree recursively subdivides only where objects are dense, providing efficient broad-phase queries for scenes with non-uniform object distributions (e.g., sparse open worlds, static-heavy levels with clusters of enemies).

---

## Solution Overview

Implement `Quadtree<T>` as a recursive axis-aligned spatial partitioning tree. The tree subdivides a node into four equal child quadrants when the node's object count exceeds a configurable threshold. Objects are identified by `Dia::Core::Handle<T>` and inserted with an `AARect` footprint. The same five query types as `SpatialGrid` are supported. `Quadtree<T>` implements `ISpatialStructure<T>` defined in the spatial-grid feature.

Supports dynamic insertion and removal (objects can be added/removed per frame). Tree structure adapts as objects are added but does not auto-merge underpopulated nodes in v1 (merge is a future optimisation).

### Key Design Points

1. **`Handle<T>` identity** — same slot pool pattern as `SpatialGrid`; consistent across all spatial structures
2. **Max depth + split threshold** — configurable at construction; defaults: max depth 8, split at 8 objects per node
3. **Objects in multiple nodes** — object footprints that span a quadrant boundary are stored in the parent node (not split across children)
4. **No STL in public API** — query output written into caller-provided `DynamicArrayC<Handle<T>>`
5. **Implements `ISpatialStructure<T>`** — DiaRigidBody2D can inject a `Quadtree` in place of a `SpatialGrid`
6. **No auto-merge on remove** — nodes are not merged when object count drops below threshold; `Rebuild()` can be called manually to compact the tree

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Quadtree<T>` exists in `Dia/DiaGeometry2D/Spatial/Quadtree.h/.cpp` | File system check |
| AC2 | `Quadtree<T>` inherits `ISpatialStructure<T>` and compiles as a drop-in replacement for `SpatialGrid<T>` | Build + substitution test |
| AC3 | `Insert()` returns a valid `Handle<T>` | Unit test |
| AC4 | Node splits when object count exceeds threshold | Unit test: insert threshold+1 objects in same region; verify child nodes created |
| AC5 | Objects spanning quadrant boundaries stored in parent node | Unit test: object centred on quadrant boundary; verify not duplicated in children |
| AC6 | `Remove()` with valid handle removes object; not returned by subsequent queries | Unit test |
| AC7 | `Remove()` with stale handle is a no-op | Unit test |
| AC8 | `Update()` moves object to correct new node | Unit test |
| AC9 | `QueryRegion`, `QueryCircle`, `QueryPoint`, `QueryRay`, `QueryKNearest` all return correct results | Unit tests per query type |
| AC10 | No duplicate handles in query output | Unit test: large insert + query |
| AC11 | `Rebuild()` compacts tree after many removals | Unit test: insert 100, remove 90, rebuild, verify tree depth reduced |
| AC12 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::Geometry2D {

struct QuadtreeDef {
    AARect worldBounds;    // Total tree extent
    int    splitThreshold; // Objects per node before subdividing (default: 8)
    int    maxDepth;       // Maximum recursion depth (default: 8)
    int    maxObjects;     // Pre-allocated slot pool size
};

template<typename T>
class Quadtree : public ISpatialStructure<T> {
public:
    explicit Quadtree(const QuadtreeDef& def);

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

    // Quadtree-specific
    void Rebuild();          // Compact tree; merge underpopulated nodes
    int  GetNodeCount()  const;
    int  GetObjectCount() const;
    int  GetDepth()      const;  // Current max depth of populated nodes
};

} // namespace Dia::Geometry2D
```

---

## Implementation Notes

### Node Structure

```cpp
struct QuadNode {
    AARect bounds;
    Dia::Core::DynamicArrayC<uint32_t> objectSlots;  // slot indices of objects in this node
    int children[4];   // indices into node pool; -1 = no child
    bool isLeaf;
};
```

Nodes are stored in a flat pool (not heap-allocated per node) to avoid fragmentation:
```
QuadNode nodePool[maxNodes];
```
`maxNodes` is derived from `maxDepth` and `splitThreshold` at construction, or pre-allocated conservatively.

### Insertion Algorithm

```
InsertInto(node, slotIndex, objectBounds):
  if node is leaf:
    add slotIndex to node.objectSlots
    if node.objectSlots.count > splitThreshold and node.depth < maxDepth:
      Subdivide(node)       // create 4 children, redistribute objects
  else:
    // Try to place in a single child that fully contains the object
    child = ChildThatContains(node, objectBounds)
    if child exists:
      InsertInto(child, slotIndex, objectBounds)
    else:
      add slotIndex to node.objectSlots  // spans multiple children — store in parent
```

### K-Nearest Implementation

Priority traversal: nodes closer to query point visited first using a min-heap of `(distanceToNode, nodeIndex)`. Prune nodes whose closest corner is farther than the k-th nearest found so far.

### File Layout

```
Dia/DiaGeometry2D/Spatial/
├── ISpatialStructure.h   (defined in spatial-grid feature)
├── Quadtree.h
└── Quadtree.cpp
```

---

## Dependencies

### Prerequisite
- **`Dia::Core::Handle<T>`** — DiaCore slot pool type (defined in spatial-grid feature)
- **spatial-grid** — `ISpatialStructure<T>` interface defined there

### Required Features
- **shape-primitives** — `AARect`, `Circle`, `Ray`

### Required Modules
- **DiaCore** — `DynamicArrayC`, `DIA_ASSERT`
- **DiaMaths** — `Vector2D`

### Dependent Features
- **DiaRigidBody2D / collision-detection** — can inject `Quadtree<PhysicsBody*>` as broad-phase for non-uniform scenes

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/Geometry2D/TestQuadtree.cpp`)

1. Insert below threshold — no split, single node
2. Insert at threshold+1 — node splits, children created
3. Object spanning boundary — stored in parent, not duplicated in children
4. `Remove` — object gone from queries; stale handle safe
5. `Update` — object moves between nodes correctly
6. Deep insert — objects at max depth all queryable
7. `QueryRegion` — objects in region returned; others not
8. `QueryCircle` — objects within circle returned
9. `QueryPoint` — point containment correct
10. `QueryRay` — ray traversal returns correct intersecting objects
11. `QueryKNearest` — k=3 from a point, correct order, no duplicates
12. `Rebuild` after mass removal — depth decreases
13. ISpatialStructure substitution — `SpatialGrid<int>` and `Quadtree<int>` produce identical results for the same queries over the same objects

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-004 | Platform | No STL containers in public APIs | ✅ `DynamicArrayC` throughout; flat node pool |
| PD-007 | Platform | C++20 required | ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | ✅ `Dia::Geometry2D::` |
| SD-004 | System | Spatial structures templated on stored type | ✅ `Quadtree<T>` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Node pool | How large should the pre-allocated node pool be? | Conservative upper bound: `4^maxDepth` nodes. At maxDepth=8 that's 65536 — probably too large. Cap at a practical limit (e.g., 4096 nodes) and `DIA_ASSERT` if exceeded. Document recommended maxDepth settings. |
| 2 | Boundary objects | Objects stored in parent nodes (spanning children) are tested in every query that reaches that node. Is this acceptable? | Yes — these are objects that genuinely touch multiple regions. The correct optimisation is choosing a good split threshold so few objects end up in parent nodes. |
| 3 | Rebuild cost | When should `Rebuild()` be called? | After bulk removals (e.g., level unload). Not needed per-frame for dynamic objects — use `Update()` instead. |
| 4 | Dynamic vs static | Quadtree supports dynamic updates — is this the right structure for static geometry? | For purely static geometry, BVH is better (tighter bounds, faster queries). Quadtree is the right choice when objects move occasionally (e.g., AI agents that move every few seconds). |
| 5 | Thread safety | Same as SpatialGrid — concurrent reads safe, writes not. | Yes — document the same threading contract. |
