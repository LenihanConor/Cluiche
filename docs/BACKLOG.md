# Cluiche Backlog

Derived from spec status across `docs/specs/`. Updated manually — when a spec moves to Done, remove or tick it here.

---

## In Progress

_Nothing currently in progress._

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| ~~DiaSerializer~~ | [diaserializer.md](specs/systems/dia/diaserializer.md) | ✅ Done 2026-05-02 — Phase 2 + Phase 3a/b/c complete; 43 Phase 3 tests | DiaCore ✅ |
| DiaApplicationEditor | [diaapplicationeditor.md](specs/systems/dia/diaapplicationeditor.md) | 15 features, all Approved — **not yet implemented** | DiaEditor ✅, DiaWebSocket ✅, DiaUICEF ✅ |
| DiaAssetCatalogue | [diaassetcatalogue.md](specs/systems/dia/diaassetcatalogue.md) | 3 features, all Approved — json-definition-loader, asset-type-framework, identity-relationship-backbone | DiaCore ✅ |
| ~~DiaIK~~ | [diaik2d.md](specs/systems/dia/diaik2d.md) | ✅ Done 2026-05-02 — 6 features, 33 tests | — |
| ~~DiaVisualDebugger~~ | [diavisualdebugger.md](specs/systems/dia/diavisualdebugger.md) | ✅ Done 2026-05-04 — all 12 features; 114 C++ tests + 13 Vitest/jsdom tests | DiaGraphics ✅, DiaSFML ✅, DiaAPI ✅, DiaCore ✅ |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System |
|---------|------|--------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ |

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| ~~DiaApplication — data-driven-application-system~~ | [data-driven-application-system.md](specs/features/dia/diaapplication/data-driven-application-system.md) | Done 2026-05-02 — JsonApplicationManifestSerializer + 12 tests |
| DiaAssetPipeline system | [diaassetpipeline.md](specs/systems/dia/diaassetpipeline.md) | Approved — 4 features identified, needs `/spec-feature` for each before building. See [asset-system-overview.md](specs/systems/dia/asset-system-overview.md). |
| DiaAssetRuntime system | TBD | Needs `/spec-system` — runtime Stage→Bundle→Asset resolution, additive over DiaCore FilePath. See [asset-system-overview.md](specs/systems/dia/asset-system-overview.md). Depends on DiaAssetCatalogue |
| DiaAssetCatalogueEditor system | TBD | Needs `/spec-system` — editor UI only, calls DiaAssetCatalogue query API. See [asset-system-overview.md](specs/systems/dia/asset-system-overview.md). Depends on DiaAssetCatalogue, DiaEditor |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| ~~DiaRigidBody2D — Visual Debugger~~ | [diarigidbody2dvisualdebugger.md](specs/systems/dia/diarigidbody2dvisualdebugger.md) | Superseded by DiaVisualDebugger / rigidbody2d-visual-debugger-stack |
| ~~DiaSoftBody2D — Visual Debugger~~ | [visual-debugger.md](specs/features/dia/diasoftbody2d/visual-debugger.md) | Superseded by DiaVisualDebugger / softbody2d-visual-debugger-stack |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| ~~DiaRig2D — Exhaustive tests~~ | Done 2026-05-02: 38 new tests (golden, invariant, stress, boundary, determinism, integration) in `Cluiche/Tests/GoogleTests/Rig2D/` |
| DiaApplication — Feature 6: Compile-Time Dependency Validation | Deferred by user ("let's come back and talk about 6") |
| ~~DiaVisualDebugger — implement fixed-draw-layer (feature 12)~~ | Done 2026-05-04. `FixedDrawRegistry`, `IObjectRenderer`, `TypedObjectRenderer<T>`, `IFixedPrimitiveBuffer`, `FixedPrimitiveBuffer`; default renderers Spatial/Quadtree/BVH/Hex; 24 tests |
| ~~DiaVisualDebugger — migrate Rig2D rest pose to fixed-draw-layer~~ | Done 2026-05-04. `RigRestPoseRenderer` in `DiaRig2DVisualDebugger/`; 4 tests. RestPoseDrawer kept for dynamic callers. |
| ~~DiaVisualDebugger — migrate Geometry2D spatial structures to fixed-draw-layer~~ | Done 2026-05-04. Re-export headers in `DiaGeometry2DVisualDebugger/Renderers/`; canonical renderers in `DiaVisualDebugger/Renderers/`. |
| HotReloadManager — `CollectDependentModules()` / `UpdateDependencyReferences()` | Placeholder stubs; needs real implementation |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaStateMachine — `MarkValid()` exposed on definitions | Added to support serializer load path; could be misused to bypass `Validate()`. Consider making package-internal if access control becomes a concern. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — **blocked on DiaAssetCatalogue** |

---

## Deferred

| Item | Spec | Reason |
|------|------|--------|
| DiaEnv — env-export | [diaenv.md](specs/systems/dia/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/systems/dia/diaenv.md) | Premature until setup + verify stable (both now Done) |
