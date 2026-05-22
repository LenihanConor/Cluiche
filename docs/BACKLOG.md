# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Spec | Features | Depends On |
|--------|------|----------|------------|
| DiaGraphics3D | [diagraphics3d.md](specs/systems/dia/diagraphics3d.md) ✅ | `graphics-3d-types` — Camera3D, lights, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D; new `Dia/DiaGraphics3D/` module; `Dia::Graphics3D::` namespace. Needs system specs for DiaMesh3D/Rig3D/Animation3D/Skinning3D/Scene3D before Phase 2 implements. | DiaMaths (Matrix44), DiaGeometry3D, DiaGraphics ✅ |
| DiaBgfx3D | [diabgfx3d.md](specs/systems/dia/diabgfx3d.md) ✅ | `3d-renderers` — Canvas3D, MeshRenderer, SkinnedMeshRenderer, ShadowRenderer, MaterialRegistry, MeshGpuCache, 6 shaders; `Dia::Bgfx3D::` namespace; Phase 2 ship gate. Blocked on Phase 1 + DiaScene3D chain. | DiaBgfx (Phase 1), DiaScene3D chain, DiaGraphics3D ✅ |
| DiaMesh3D | TBD — needs `/spec-system` | `mesh-asset-and-loader` feature already Approved (parent currently `render-backend`); needs own system spec. glTF 2.0 static+skinned mesh loading, `Mesh3DAsset`, `IAssetTypeHandler` plug-in. | DiaMaths, DiaGeometry3D, DiaAssetRuntime |
| DiaRig3D | TBD — needs `/spec-system` | `skeleton-and-pose` feature already Approved; needs own system spec. Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`. Mirrors DiaRig2D. | DiaMesh3D |
| DiaAnimation3D | TBD — needs `/spec-system` | `clip-and-player` feature already Approved; needs own system spec. AnimationClip3D, ClipPlayer3D, glTF loader, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. | DiaRig3D |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
| DiaScene3D | TBD — needs `/spec-system` | `scene-graph` feature already Approved; needs own system spec. Flat-list scene, Transform3D parent chains, frustum culling, `Submit(scene, frameData3D)`. | DiaSkinning3D, DiaGraphics3D, DiaGeometry3D |

---

### Standalone Features (system Done, feature Approved)

| Feature | Spec | System |
|---------|------|--------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ |

---


### Entity System Stack (build in dependency order)

System specs and the foundation feature are all `Approved`. Each system's child feature specs are still `Planned/TBD` and need `/spec-feature` before implementation. Strict order: HandlePool → remove old IComponent → DiaMailbox → DiaEntity. Research: `docs/research/entity_system/summary.md`.

| # | Item | Spec | What's next |
|---|------|------|-------------|
| 1 | HandlePool\<T\> | [handle-pool.md](specs/features/dia/diacore/handle-pool.md) | **Done** (2026-05-20). Foundation for everything below. |
| 2 | Remove old IComponent infrastructure | — | **Done** (2026-05-20). `Architecture/Components/` deleted, `SkeletonComponent` + `StateMachineComponent` migrated to plain classes, `TestComponent.cpp` deleted, 4818 tests pass. |
| 3 | DiaMailbox features (5 features) | [diamailbox.md](specs/systems/dia/diamailbox.md) | **Done** (2026-05-21). All 5 features implemented; 62/62 tests GREEN. DIA_LOG_WARNING + dia.mailbox.{sent,dropped,drained} metrics added. |
| 4 | DiaEntity features (11 features) | [diaentity.md](specs/systems/dia/diaentity.md) | System Approved. Need `/spec-feature` in implementation order: foundation, reflection, blueprint-loader, component-deps-and-refs, hierarchy, mailbox-router, query-system, editor-inspection, update-loop, module-and-build (#2 remove-old-icomponent is item #2 above). |
| 5 | EntityModule adapter (CluicheTest) | TBD | Needs `/spec-feature` under CluicheTest — application-level adapter that owns a Realm and plugs into DiaApplicationFlow v2 stage lifecycle (DoStart loads blueprints, returns kLoading until assets resolve, kReady; stage transition destroys Realm). Lives in CluicheTest, not in DiaEntity. |
| 6 | PD-003 / AD-005 Supersede amendment | TBD | After DiaEntity ships, amend platform decision PD-003 and app decision AD-005 (both reference the old IComponent model) to Superseded, pointing to DiaEntity as the new authority. Per SD-ENT-021. Housekeeping. |

---

## Ready to Build (cont.)


---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |

---

## E2E Orchestration Stack (build in dependency order)

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

Replaces previous DiaTestHarness + DiaE2E + Harness Core + Smoke Test Scenario + DiaAPI quit command items. The old `docs/specs/systems/dia/diatestharness.md` and `docs/specs/features/dia/diatestharness/harness-core.md` are superseded.

| # | Item | Type | Size | Depends on | Status |
|---|------|------|------|------------|--------|
| 0 | DiaAPI — relax name validation to `[a-z0-9._-]+`; add JSON command path | Folded into item #2 | XS | — | Spec'd (part of baseline-commands) |
| 1 | DiaApplicationFlow — [transition-guards](specs/features/dia/diaapplicationflow/transition-guards.md) | Done | S | — | **Done** |
| 2 | DiaApplicationFlow — [baseline-commands](specs/features/dia/diaapplicationflow/baseline-commands.md) (`dia.app.quit`, `dia.app.report` + DiaAPI JSON path) | Done | XS | — | **Done** |
| 3 | [DiaAutomation](specs/systems/dia/diaautomation.md) system + AutomationModule (CluicheGameBaseline) | Done — all feature specs created + implemented | M | 1, 2 | **Done** |
| 4 | [dia orchestrate](specs/features/dia/diacli/dia-orchestrate.md) CLI + pytest plugin | Approved — ready to implement | S | 3 | **Approved** |
| 5 | CluicheTest [smoke scenario](specs/features/cluichetest/cluichetestscenarios/smoke-scenario.md) (pytest) | Approved — ready to implement | XS | 4 | **Approved** |
| 6 | CluicheTest [TestStages](specs/systems/cluichetest/teststages.md) system (RigidBody2D, EntityTest, … with checkpoints) | System Approved — individual stages need `/spec-feature` | M–L | 4 | **System Approved** |
| 6b | Extract AutomationModuleBase from game AutomationModule into DiaAutomation | Refactor (evaluate shared base vs duplication from working code) | XS–S | 3 implemented | Deferred — decide from working code |
| 7 | CluicheEditor EditorAutomationModule wiring + `IEditorPlugin::RegisterCheckpoints` | `/spec-feature` under CluicheEditor | S | 6b | Deferred — after 6b |
| 8 | [Metric threshold assertions](specs/features/dia/diaautomation/metric-assertions.md) (pytest fixture + `dia.automation.get_metric`) | Approved — ready to implement | XS | 3 | **Approved** |

**Implementation order:** 1 → 2 → 3 (feature specs needed) → 4 → 5+6+8 fan out. Items 6b+7 deferred until working code exists.

Items 1+2 can run in parallel. Items 5, 6 (individual stages), and 8 can fan out once 4 is done.

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
