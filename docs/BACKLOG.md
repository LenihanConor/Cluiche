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
| DiaData | [diadata.md](specs/systems/dia/diadata.md) | 3 features, all Approved — json-definition-loader, asset-type-framework, identity-relationship-backbone | DiaCore ✅ |
| ~~DiaIK~~ | [diaik2d.md](specs/systems/dia/diaik2d.md) | ✅ Done 2026-05-02 — 6 features, 33 tests | — |
| DiaVisualDebugger | [diavisualdebugger.md](specs/systems/dia/diavisualdebugger.md) | 11 features all Approved — see build order below | DiaGraphics ✅, DiaSFML ✅, DiaAPI ✅, DiaCore ✅ |

### DiaVisualDebugger Build Order

Implement these in sequence — each depends on the previous:

| # | Feature | Spec | Notes |
|---|---------|------|-------|
| 1 | debug-budget | [debug-budget.md](specs/features/dia/diavisualdebugger/debug-budget.md) | Extends DiaGraphics — `DroppedCount`, `entityId` on `DebugPrimitive` |
| 2 | debug-text-primitive | [debug-text-primitive.md](specs/features/dia/diavisualdebugger/debug-text-primitive.md) | Extends DiaGraphics + DiaSFML — `Text2D` primitive, `RequestDrawText()` |
| 3 | debug-layer-manager | [debug-layer-manager.md](specs/features/dia/diavisualdebugger/debug-layer-manager.md) | New `DiaVisualDebugger.vcxproj` — `IVisualDebugger`, `DebugLayerManager`, palette, layer names |
| 4 | rig2d-visual-debugger-stack | [rig2d-visual-debugger-stack.md](specs/features/dia/diavisualdebugger/rig2d-visual-debugger-stack.md) | Decomposes existing monolithic debugger into 5 classes |
| 5 | rigidbody2d-visual-debugger-stack | [rigidbody2d-visual-debugger-stack.md](specs/features/dia/diavisualdebugger/rigidbody2d-visual-debugger-stack.md) | Decomposes existing monolithic debugger into 5 classes |
| 6 | softbody2d-visual-debugger-stack | [softbody2d-visual-debugger-stack.md](specs/features/dia/diavisualdebugger/softbody2d-visual-debugger-stack.md) | New `DiaSoftBody2DVisualDebugger.vcxproj` — 4 classes |
| 7 | ik2d-visual-debugger-stack | [ik2d-visual-debugger-stack.md](specs/features/dia/diavisualdebugger/ik2d-visual-debugger-stack.md) | New `DiaIK2DVisualDebugger.vcxproj` — 4 classes + 6 new `IKSolver` accessors |
| 8 | geometry2d-visual-debugger-stack | [geometry2d-visual-debugger-stack.md](specs/features/dia/diavisualdebugger/geometry2d-visual-debugger-stack.md) | New `DiaGeometry2DVisualDebugger.vcxproj` — `ShapeDrawer` + `SpatialStructureDrawer` |
| 9 | debug-console | [debug-console.md](specs/features/dia/diavisualdebugger/debug-console.md) | New `DiaImGui.vcxproj` (backend abstraction) + `DiaVisualDebuggerConsole.vcxproj`; imgui-sfml into DiaSFML via `SFMLImGuiBackend` |
| 10 | debug-editor-panel | [debug-editor-panel.md](specs/features/dia/diavisualdebugger/debug-editor-panel.md) | `DebugLayerPanelPlugin` + CEF UI in `DiaEditor/ui/debug-layers/` |
| 11 | animation2d-visual-debugger-stack | [animation2d-visual-debugger-stack.md](specs/features/dia/diavisualdebugger/animation2d-visual-debugger-stack.md) | New `DiaAnimation2DVisualDebugger.vcxproj` — 3 classes + 11 DiaAnimation2D accessors |

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
| DiaAssetPipeline system | TBD | Needs `/spec-system` — build-time asset pipeline (discover, validate, transform, deploy). Depends on DiaData, DiaAPI, DiaLogger |
| DiaStageLoader system | TBD | Needs `/spec-system` — runtime Stage/Bundle loading from built output. Depends on DiaData |
| DiaAssetBrowserEditor system | TBD | Needs `/spec-system` — editor UI for asset browsing, inspection, relationship graph. Depends on DiaData, DiaEditor |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| ~~DiaRigidBody2D — Visual Debugger~~ | [diarigidbody2dvisualdebugger.md](specs/systems/dia/diarigidbody2dvisualdebugger.md) | Superseded by DiaVisualDebugger / rigidbody2d-visual-debugger-stack |
| ~~DiaSoftBody2D — Visual Debugger~~ | [visual-debugger.md](specs/features/dia/diasoftbody2d/visual-debugger.md) | Superseded by DiaVisualDebugger / softbody2d-visual-debugger-stack |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| ~~DiaRig2D — Exhaustive tests~~ | Done 2026-05-02: 38 new tests (golden, invariant, stress, boundary, determinism, integration) in `Cluiche/Tests/GoogleTests/Rig2D/` |
| DiaApplication — Feature 6: Compile-Time Dependency Validation | Deferred by user ("let's come back and talk about 6") |
| HotReloadManager — `CollectDependentModules()` / `UpdateDependencyReferences()` | Placeholder stubs; needs real implementation |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaStateMachine — `MarkValid()` exposed on definitions | Added to support serializer load path; could be misused to bypass `Validate()`. Consider making package-internal if access control becomes a concern. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — **blocked on DiaData** |

---

## Deferred

| Item | Spec | Reason |
|------|------|--------|
| DiaEnv — env-export | [diaenv.md](specs/systems/dia/diaenv.md) | Covered by deps.json + mirrors + submodules |
| DiaEnv — docker continuous monitoring (`dia env doctor`) | [diaenv.md](specs/systems/dia/diaenv.md) | Premature until setup + verify stable (both now Done) |
