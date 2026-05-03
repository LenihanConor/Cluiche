# Research: Evaluate — Debug Draw for Fixed/Registered Objects

**Input:** docs/research/debug_draw_fixed/ideate.md

---

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability — 5 = broadly reusable pattern, 1 = narrow one-off
- **Game Value (0.20):** Improves CluicheTest as demo/testbed — 5 = directly enables key debug workflows, 1 = marginal benefit
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap (S), 1 = very expensive (XL)
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood pattern, 1 = high unknowns
- **Cluiche Fit (0.15):** Aligns with PD-001 through PD-007, existing module structure, and DiaVisualDebugger system decisions

---

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. IFixedDebugDrawObject flag | 3 | 3 | 4 | 4 | 4 | 3.55 |
| 2. IRegisteredDrawObject + IObjectRenderer | 5 | 4 | 3 | 3 | 4 | **3.85** |
| 3. DebugDrawRegistrar (parallel system) | 3 | 3 | 3 | 3 | 3 | 3.00 |
| 4. CachedVisualDebugger CRTP | 4 | 3 | 4 | 4 | 4 | **3.80** |
| 5. FixedDrawSlot + IVersioned | 4 | 4 | 3 | 3 | 3 | 3.45 |
| 6. IObjectRenderer injection into existing classes | 2 | 3 | 5 | 5 | 4 | **3.70** |
| 7. Command-stream / render-thread buffer | 5 | 3 | 1 | 2 | 3 | 2.85 |
| 8. DebugDrawGroup aggregate | 3 | 3 | 4 | 4 | 4 | 3.55 |
| 9. Full system (2 + 4 + 8 combined) | 5 | 5 | 2 | 2 | 4 | 3.65 |

---

## Top 3 Candidates

### Rank 1: Candidate 2 — IRegisteredDrawObject + IObjectRenderer (score: 3.85)
**Why:** The object/renderer split is the only design that directly addresses all three of the user's stated goals in one architecture: register once (source held by reference in `IRegisteredDrawObject`), stream controls draw/no-draw (the layer toggle on `DebugLayerManager`), and multiple/custom renderers (`IObjectRenderer` is a strategy swapped at runtime). Scoring 5 on Engine Value reflects that `IRegisteredDrawObject` and `IObjectRenderer` have no inherent debug coupling — they are clean interfaces that can be promoted to non-debug use later (PD-002/PD-003 alignment). The M cost is real but bounded: two interfaces, a per-object primitive cache, and a `RegisterObject()` overload on `DebugLayerManager`.  
**Watch out for:** Thread safety on `DebugLayerManager` — if the sim registers/deregisters objects while the render thread calls `Draw()`, the `mLayers` array needs a guard. Also, per-object primitive buffers can be large for big grids (128×128 = ~130k edge lines); capacity must be caller-configurable rather than fixed.

### Rank 2: Candidate 4 — CachedVisualDebugger CRTP (score: 3.80)
**Why:** Zero change to `DebugLayerManager` (already approved and being implemented) means no spec amendment. The CRTP caching mixin is an opt-in addition that any existing or future `IVisualDebugger` subclass can inherit from — high reuse at low coordination cost. SD-DBG-009 (`DebugFrameData` trivially copyable) is respected because the per-object buffer is a `DynamicArrayC<DebugPrimitive, N>`, which is trivially copyable. The missing piece relative to the user's requirements is custom renderers: with CRTP, each renderer variant is a separate derived class, which means more boilerplate for the caller.  
**Watch out for:** `kCapacity` template parameter makes the buffer size a compile-time decision. A 128×128 `HexGrid` needing 49 152 line primitives would require `kCapacity = 50000`, which is ~2.4 MB on the stack per object — unacceptable. The buffer needs to be heap-allocated (or the capacity must come from a pool) for large structures, which complicates the CRTP template. Note: Candidate 4 is a strong *implementation detail* for Candidate 2 — `IRegisteredDrawObject` implementations can use this pattern internally.

### Rank 3: Candidate 6 — IObjectRenderer injection into existing draw classes (score: 3.70)
**Why:** Maximum cheapness (cost score 5) and minimum risk (5) — just adds a nullable `IObjectRenderer*` constructor argument to the existing planned `SpatialGridDrawer`, `QuadtreeDrawer`, etc. classes. Delivers custom renderer capability immediately with zero new infrastructure. Useful as an interim or alongside a larger system.  
**Watch out for:** Not a general pattern — each draw class must be individually modified; no shared registration or caching mechanism emerges. Doesn't solve the per-frame redundant re-emission problem (just the renderer variety problem). If Candidate 2 is built, Candidate 6 becomes redundant. Best treated as a standalone stopgap or as the way Candidate 2's default renderer is plugged into the concrete draw classes.

---

## Recommendation

**Candidate 2 (IRegisteredDrawObject + IObjectRenderer)** is the right architectural answer. It is the only design that fully honours all three user requirements — register-once, draw-command toggle, and swappable/multiple renderers — without baking render logic into source objects or requiring a parallel system outside `DebugLayerManager`. The two interfaces have no hard coupling to `#ifdef DIA_DEBUG`, giving a clean promotion path to non-debug use that the user flagged as likely (PD-004 and PD-001 compliance is straightforward since public APIs use `StringCRC` names and `DynamicArrayC` buffers).

Candidate 4's CRTP cache should be treated as a companion implementation detail — `IRegisteredDrawObject` concrete implementations for large structures (grids, quadtrees) would inherit from `CachedVisualDebugger<Derived, N>` to get transparent primitive caching without re-inventing the buffer management. Candidate 8 (DebugDrawGroup) is complementary for the multiple-instance naming problem and can be included as a lightweight addition alongside Candidate 2. Candidate 6's renderer injection pattern becomes the mechanism by which the *default* `IObjectRenderer` for each spatial type is implemented — same idea, just within the right framework.

Candidate 7 was the only design with a true zero-sim-thread model, but its XL cost and high uncertainty (new message types, per-object render-thread buffers, uncertain interaction with `FrameData` producer/consumer) make it premature. The command-stream pattern should be revisited if profiling shows sim-thread overhead from large structure registration is a real bottleneck.
