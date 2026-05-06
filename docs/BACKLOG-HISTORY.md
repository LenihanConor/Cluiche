# Cluiche Backlog History

Completed items moved from BACKLOG.md. For active work see [BACKLOG.md](BACKLOG.md).

---

## Completed Systems

| System | Spec | Completed | Notes |
|--------|------|-----------|-------|
| DiaAssetPipeline | [diaassetpipeline.md](specs/systems/dia/diaassetpipeline.md) | 2026-05-05 | All 4 features; CLI command surface, built-in type handlers, deploy integration |
| DiaSerializer | [diaserializer.md](specs/systems/dia/diaserializer.md) | 2026-05-02 | Phase 2 + Phase 3a/b/c complete; 43 Phase 3 tests |
| DiaAssetCatalogue | [diaassetcatalogue.md](specs/systems/dia/diaassetcatalogue.md) | 2026-05-04 | All 4 features; 92 tests (34 feature + 58 exhaustive) |
| DiaIK | [diaik2d.md](specs/systems/dia/diaik2d.md) | 2026-05-02 | 6 features, 33 tests |
| DiaVisualDebugger | [diavisualdebugger.md](specs/systems/dia/diavisualdebugger.md) | 2026-05-04 | All 12 features; 114 C++ tests + 13 Vitest/jsdom tests |
| DiaAssetRuntime | [diaassetruntime.md](specs/systems/dia/diaassetruntime.md) | 2026-05-05 | 6 features, all Done; 58 tests |
| DiaAssetRuntimeEditor | [diaassetruntimeeditor.md](specs/systems/dia/diaassetruntimeeditor.md) | 2026-05-05 | 4 features, all Done; 58 editor tests; gap analysis performed and fixes applied |

---

## Completed Standalone Features

| Feature | Spec | System | Completed |
|---------|------|--------|-----------|
| data-driven-application-system | [data-driven-application-system.md](specs/features/dia/diaapplication/data-driven-application-system.md) | DiaApplication | 2026-05-02 — JsonApplicationManifestSerializer + 12 tests |

---

## Completed Standalone Features (continued)

| Feature | Spec | System | Completed |
|---------|------|--------|-----------|
| DiaCore — DirectedGraph + RelationshipIndex refactor | [directed-graph.md](specs/features/dia/diacore/directed-graph.md) | DiaCore | 2026-05-05 — `DirectedGraph<..., IDType>` with 3 policies + 7th IDType template param; RelationshipIndex rebuilt on DirectedGraph with CRC keys (~47KB stack); 78 graph tests + 92 asset catalogue tests passing |

---

## Completed Loose Ends

| Item | Completed | Notes |
|------|-----------|-------|
| FlatStateMachine `WildcardTransitionFiresFromAnyState` crash | 2026-05-05 | Stack overflow from `MetadataArray` embedded in each `StateDef`; fixed by moving state metadata to parallel slab `mStateMetadata` on definition, reducing `StateDef` from ~2604 B to ~432 B |
| DiaRig2D — Exhaustive tests | 2026-05-02 | 38 new tests (golden, invariant, stress, boundary, determinism, integration) in `Cluiche/Tests/GoogleTests/Rig2D/` |
| DiaVisualDebugger — implement fixed-draw-layer (feature 12) | 2026-05-04 | `FixedDrawRegistry`, `IObjectRenderer`, `TypedObjectRenderer<T>`, `IFixedPrimitiveBuffer`, `FixedPrimitiveBuffer`; default renderers Spatial/Quadtree/BVH/Hex; 24 tests |
| DiaVisualDebugger — migrate Rig2D rest pose to fixed-draw-layer | 2026-05-04 | `RigRestPoseRenderer` in `DiaRig2DVisualDebugger/`; 4 tests. RestPoseDrawer kept for dynamic callers |
| DiaVisualDebugger — migrate Geometry2D spatial structures to fixed-draw-layer | 2026-05-04 | Re-export headers in `DiaGeometry2DVisualDebugger/Renderers/`; canonical renderers in `DiaVisualDebugger/Renderers/` |

---

## Superseded Items

| Item | Reason |
|------|--------|
| DiaRigidBody2D — Visual Debugger | Superseded by DiaVisualDebugger / rigidbody2d-visual-debugger-stack |
| DiaSoftBody2D — Visual Debugger | Superseded by DiaVisualDebugger / softbody2d-visual-debugger-stack |
| DiaAssetPipeline system spec work | Approved — moved to Ready to Build |
| DiaAssetRuntime system spec work | Approved — moved to Ready to Build |
| DiaAssetCatalogueEditor system spec work | Approved — moved to Ready to Build |
| DiaApplication — Flow Tree (Phases A/B/C) spec work | Approved — moved to Ready to Build |

---

## Deferred

| Item | Spec | Reason |
|------|------|--------|
| DiaEnv — env-export | [diaenv.md](specs/systems/dia/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/systems/dia/diaenv.md) | Premature until setup + verify stable (both now Done) |
