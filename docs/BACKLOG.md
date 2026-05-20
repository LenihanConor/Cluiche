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
| Harness Core | [harness-core.md](specs/features/dia/diatestharness/harness-core.md) | DiaTestHarness |
| Smoke Test Scenario | [smoke-test-scenario.md](specs/features/cluichetest/cluichetestscenarios/smoke-test-scenario.md) | CluicheTestScenarios (depends on Harness Core) |

---


### Entity System Stack (build in dependency order)

System specs and the foundation feature are all `Approved`. Each system's child feature specs are still `Planned/TBD` and need `/spec-feature` before implementation. Strict order: HandlePool → remove old IComponent → DiaMailbox → DiaEntity. Research: `docs/research/entity_system/summary.md`.

| # | Item | Spec | What's next |
|---|------|------|-------------|
| 1 | HandlePool\<T\> | [handle-pool.md](specs/features/dia/diacore/handle-pool.md) | **Done** (2026-05-20). Foundation for everything below. |
| 2 | Remove old IComponent infrastructure | — | **Done** (2026-05-20). `Architecture/Components/` deleted, `SkeletonComponent` + `StateMachineComponent` migrated to plain classes, `TestComponent.cpp` deleted, 4818 tests pass. |
| 3 | DiaMailbox features (5 features) | [diamailbox.md](specs/systems/dia/diamailbox.md) | System Approved. Need `/spec-feature` for: address-and-types, typed-queue, subscriptions, routers, module-and-build. |
| 4 | DiaEntity features (11 features) | [diaentity.md](specs/systems/dia/diaentity.md) | System Approved. Need `/spec-feature` in implementation order: foundation, reflection, blueprint-loader, component-deps-and-refs, hierarchy, mailbox-router, query-system, editor-inspection, update-loop, module-and-build (#2 remove-old-icomponent is item #2 above). |
| 5 | EntityModule adapter (CluicheTest) | TBD | Needs `/spec-feature` under CluicheTest — application-level adapter that owns a Realm and plugs into DiaApplicationFlow v2 stage lifecycle (DoStart loads blueprints, returns kLoading until assets resolve, kReady; stage transition destroys Realm). Lives in CluicheTest, not in DiaEntity. |
| 6 | PD-003 / AD-005 Supersede amendment | TBD | After DiaEntity ships, amend platform decision PD-003 and app decision AD-005 (both reference the old IComponent model) to Superseded, pointing to DiaEntity as the new authority. Per SD-ENT-021. Housekeeping. |

---

## Ready to Build (cont.)

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaAPI quit command | TBD | Needed for DiaTestHarness graceful shutdown. No quit command exists today (exit is UI-driven). Needs `/spec-feature` under DiaAPI |
| CluicheTest TestStages system | TBD | Needs `/spec-system` under CluicheTest — multi-stage test stages for deep engine validation (DiaRigidBody2D first). Open questions: phase vs level vs own PU; reporting mechanism. Research: `docs/research/e2e_testing/summary.md` |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| Future DiaE2E system | TBD | Needs `/spec-system` — DiaObservation #1–#7 now Done. Sibling `Cluiche/out/<App>/suites/<id>/` directory, `suite.json` + per-scenario summary + JUnit XML emitter, `dia e2e --suite=<name>` CLI command, scenario subprocess spawning. Research: `docs/research/observ_telemetry/summary.md` "Future: Multi-Scenario E2E Suites". |
| DiaObservation #8–#13 (profiling + domain instrumentation) | [diaobservation.md](specs/systems/dia/diaobservation.md) | #1–#7 Done. #11 (domain-metric-registration) Done (2026-05-20). Features #8 (profiling infrastructure) and #9 (domain profiling) need `/spec-feature`. Features #10, #12–#13 (domain log/trace/health instrumentation) need `/spec-feature`. |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| DiaAssetRuntime / DiaDebugServer / DiaInput / DiaThreading — metrics | Consolidated into DiaObservation Feature #11 (Domain-Level Metric Registration). See [diaobservation.md](specs/systems/dia/diaobservation.md). DiaThreading extraction still needs its own `/spec-system` but its metrics land in Feature #11 once unblocked. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
| DiaDebugServer `ServerStats` struct | `ServerStats` fields are read by `DebugServerHostModule` each tick to populate `MetricRegistry` gauges (`dia.debugserver.connections`, `.subscriptions`, `.tick_ms`, `.messages_sent`). The struct itself is intentional — it is the source feeding the registry, not a hand-rolled metric sink. No action needed. |
