# System Spec: DiaEntitySpatial

**Parent:** @docs/specs/applications/dia/dia.md  
**Status:** Done

---

## Summary

DiaEntitySpatial is a thin adapter module that bridges `DiaGeometry2D`'s generic `ISpatialStructure<T>` with the DiaEntity domain. It enables spatially-indexed queries over live entities without coupling DiaGeometry2D to the entity concept. Any entity that opts in via a `SpatialComponent` can be found by position, region, ray, or sector. The game side controls what "layer" each entity belongs to via a 31-bit bitmask — bit 31 is reserved as the engine's dirty flag.

---

## Architecture

```
DiaGeometry2D     ISpatialStructure<T>          pure spatial, generic, T-agnostic
                  SpatialGrid<T>  Quadtree<T>
                  BVH<T>  HexGrid<T>
                        │
                        │ T = Entity (instantiation only)
                        ▼
DiaEntitySpatial  SpatialComponent              opt-in data tag on entities
                  EntitySpatialIndex            wraps ISpatialStructure<Entity>
                  EntitySpatialModule           frame sweep + query surface
                        │
                        │ reads SpatialComponent, returns Entity handles
                        ▼
DiaEntity         Domain  Entity  IComponent
```

### Module-owned members

`EntitySpatialModule` owns two fixed-capacity members allocated at construction, never reallocated:

- `mSpatialHandles` — `DynamicArrayC<Handle<Entity>, kMaxEntitiesPerDomain>`: maps entity slot index to the structure's internal slot handle returned by `Insert()`. `Invalid` handle = entity not yet inserted. Required for dirty-flag removal.
- `mResolveBuffer` — `DynamicArrayC<Handle<Entity>, kMaxQueryResults>`: intermediate buffer reused on every query call for the Resolve step. Making it module-owned eliminates per-call stack pressure and makes queries non-reentrant (safe since all calls must be on the owning PU's thread).

### Frame update

`EntitySpatialModule::Update()` runs in two passes:

**Pass 1 — dirty sweep:**
Calls `Domain::Query<SpatialComponent>()` (returns `QueryView` — two pointers, no allocation). For each entity in the view:
- If bit 31 of `mLayerMask` is set (dirty):
  - If `mSpatialHandles[e.index]` is valid: call `index.Remove(mSpatialHandles[e.index])`
  - Call `index.Insert(e, position, radius)`, store returned `Handle<Entity>` in `mSpatialHandles[e.index]`
  - Clear bit 31

**Pass 2 — detach/destroy sweep:**
Iterates `mSpatialHandles`. Any slot with a valid handle whose entity is no longer in the current `QueryView` (component detached or entity destroyed) gets `index.Remove()`d and its slot reset to `Invalid`. This handles both `QueueRemoveComponent<SpatialComponent>` and entity destruction.

New entities are always dirty on first frame — `mSpatialHandles[e.index]` is `Invalid` so the Remove step is skipped and Insert proceeds directly.

### Query path

All five query methods share the same structure:
1. Call the underlying `ISpatialStructure<Entity>` query into `mResolveBuffer` (yields `Handle<Entity>` — the structure's slot handles)
2. For each handle: call `index.Resolve(handle)` to get the stored `Entity` value
3. Validate via `Domain::IsAlive(entity)` — drops stale generational handles
4. Apply exact geometric rejection per query shape (see below) — eliminates AARect broad-phase false positives
5. Apply layer-mask filter: `entity.layerMask & mask` (bit 31 masked out before comparison)
6. Write passing entities to caller's `out` buffer; silently truncate if `out` is full (debug assert on truncation)

**Exact rejection per shape:**
- `QueryCircle`: `|candidate.position - center| <= queryRadius + candidate.radius`
- `QueryRegion`: AARect vs AARect — exact by construction, no rejection needed
- `QueryRay`: closest point on ray to `candidate.position <= candidate.radius`
- `QueryKNearest`: distance check same as `QueryCircle`; result sorted ascending by `|candidate.position - origin|` after filtering (O(k log k))
- `QuerySector`: `QueryCircle` broad phase + angle rejection: `dot(normalize(candidate.position - origin), dir) >= cos(halfAngle)`. The `QueryCircle` + angle filter is an internal implementation detail — callers see only `QuerySector`.

### Threading

All methods on `EntitySpatialModule` must be called from the owning PU's thread. Concurrent queries on the same instance are not supported. Two separate `EntitySpatialModule` instances (e.g. different topologies on the same domain) are independent and safe with respect to each other.

### Memory footprint

- `SpatialComponent`: `Vec2 position` (8 bytes) + `float radius` (4 bytes) + `uint32_t mLayerMask` (4 bytes, bit 31 reserved as dirty flag) = **16 bytes**
- Spatial structure stores `Entity` handle (8 bytes) per slot plus structure overhead. No component data duplicated in the index.
- `mSpatialHandles`: `kMaxEntitiesPerDomain × 8 bytes`
- `mResolveBuffer`: `kMaxQueryResults × 8 bytes`
- All capacities fixed at construction — no mid-frame heap growth.

---

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| SpatialComponent | Opt-in data component: `Vec2 position`, `float radius`, `uint32_t mLayerMask` (bit 31 reserved as dirty flag; bits 0–30 available to game) | Done |
| EntitySpatialIndex | `ISpatialStructure<Entity>` wrapper with topology selection (square / hex) | Done |
| EntitySpatialModule | Per-domain IModule: owns index + resolve buffer + spatial handle map; dirty-flag sweep; five query shapes | Done |
| Test Utilities | Helpers to populate a domain with positioned entities and assert exact query result sets | Done |
| Visual Debugger | `DiaEntitySpatialVisualDebugger` — grid overlay, entity circle overlay, query shape overlay; see [diaentityspatialvisualdebugger.md](../diaentityspatialvisualdebugger/diaentityspatialvisualdebugger.md) | Draft |

---

## Acceptance Criteria

1. `SpatialComponent` is a diaentitytemplate component (`DIA_COMPONENT`/`FIELD` macros) holding `Vec2 position`, `float radius`, `uint32_t mLayerMask`; bit 31 is the engine-reserved dirty flag; bits 0–30 are available to the game side; a debug assert fires if the game sets bit 31 directly
2. `EntitySpatialModule::Update()` performs a dirty-flag sweep then a detach/destroy sweep each frame (see Architecture)
3. All five query methods take a caller-owned `DynamicArrayC<Entity, N>& out` out-parameter — no heap allocation on the query path
4. `QueryCircle(Vec2 center, float radius, uint32_t mask, out)` populates `out` with live, mask-matching, geometrically exact entity handles
5. `QueryRegion(AARect, uint32_t mask, out)` populates `out` with matching handles
6. `QueryKNearest(Vec2 origin, int k, uint32_t mask, out)` populates `out` with up to `k` nearest handles sorted ascending by distance from `origin`
7. `QueryRay(Ray, float maxDist, uint32_t mask, out)` populates `out` with matching handles
8. `QuerySector(Vec2 origin, Vec2 dir, float radius, float halfAngle, uint32_t mask, out)` populates `out` with handles within the arc; implemented internally as `QueryCircle` + angle filter
9. All query results are validated via `Domain::IsAlive(entity)` — stale generational handles are silently dropped
10. All queries apply exact geometric rejection after the AARect broad phase — no false positives reach the caller
11. Results exceeding the caller's `out` capacity are silently truncated; a debug assert fires on truncation
12. Index topology selected at construction: `SquareGrid` or `HexGrid`; both supported
13. Index is per-domain — one `EntitySpatialModule` per domain; no global/static state; all capacities fixed at construction
14. An entity with no `SpatialComponent` is never inserted and never returned by any query
15. An entity whose `SpatialComponent` is detached is removed from the index by the end of the same frame's Update
16. Test utilities allow constructing a domain with N positioned entities and asserting exact query result sets
17. Google Tests cover all five query shapes, layer mask filtering, dirty-flag incremental update, component detach removal, hex vs square topology, and stale-handle safety

---

## Binding Decisions

| Decision | How this feature complies |
|----------|--------------------------|
| PD-001 — StringCRC for IDs | `SpatialComponent` type ID registered via `StringCRC` (`kUniqueId`) |
| PD-004 / AD-002 — No STL in public APIs | Query results via `DynamicArrayC<Entity, N>& out`; no `std::vector` in any public header |
| AD-003 — Namespace `Dia::<Module>::` | All types live in `Dia::EntitySpatial::` |
| diaentitytemplate (AD-005 superseded) — component model | `SpatialComponent` uses the `DIA_COMPONENT` / `FIELD` macro pattern |

---

## Design Decisions

| # | Question | Decision |
|---|----------|----------|
| 1 | Incremental vs full rebuild | Dirty-flag sweep — only entities with bit 31 set are re-inserted. Full rebuild avoided. |
| 2 | Multiple indices per domain | Two `EntitySpatialModule` instances with different topology parameters. No named registry needed — game side manages instances. |
| 3 | Query result ownership | Caller-owned out-parameter (`DynamicArrayC<Entity, N>& out`). No heap allocation on query path. `ISpatialStructure<T>` query takes the same out-parameter pattern. |
| 4 | Dirty flag placement | Bit 31 of `mLayerMask` reserved as dirty flag. Game side has bits 0–30. Debug assert guards against accidental writes to bit 31. Struct stays 16 bytes. |
| 5 | `ISpatialStructure<T>` template parameter | `T = Entity` (which is already `Handle<EntityTag>`). `ISpatialStructure<Handle<Entity>>` would be triple-nested; rejected. |
| 6 | Resolve buffer ownership | Module-owned `mResolveBuffer` member — eliminates per-call stack pressure; queries are non-reentrant by design. |
| 7 | `QueryKNearest` sort | Sorted in `EntitySpatialModule` after resolve + filter. Comparator: ascending `|candidate.position - origin|`. O(k log k). Not a contract on `ISpatialStructure`. |
| 8 | `QuerySector` implementation | `QueryCircle` broad phase + angle post-filter at module layer. Not a native `ISpatialStructure` operation. Hidden from callers. |
