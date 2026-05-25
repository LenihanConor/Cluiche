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

| Feature | Spec | System | Notes |
|---------|------|--------|-------|
| per-app-bin-layout | [per-app-bin-layout.md](specs/features/dia/diapipeline/per-app-bin-layout.md) | DiaPipeline ✅ | |
| diabgfx-imgui-backend | [imgui-backend.md](specs/features/dia/diabgfx/imgui-backend.md) | RenderBackend (Phase 1) | **Blocked**: requires `External/bgfx/examples/common/imgui/` — run `dia env setup --dep bgfx` with examples flag to stage. `canvas-parity` Done (2026-05-25). |
| ~~ToolbarPanelSwitcher~~ | **Done** (2026-05-22) — full-name pills + `⋯ +N` overflow dropdown, `ProjectContextButton` moved right. | | |

---


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

### Core Stack (Done)

| # | Item | Status |
|---|------|--------|
| 0 | DiaAPI — JSON command path | **Done** (folded into #2) |
| 1 | DiaApplicationFlow — [transition-guards](specs/features/dia/diaapplicationflow/transition-guards.md) | **Done** |
| 2 | DiaApplicationFlow — [baseline-commands](specs/features/dia/diaapplicationflow/baseline-commands.md) | **Done** |
| 3 | [DiaAutomation](specs/systems/dia/diaautomation.md) system + AutomationModule | **Done** |
| 4 | [dia orchestrate](specs/features/dia/diacli/dia-orchestrate.md) CLI + pytest plugin | **Done** |
| 5 | CluicheTest [smoke scenario](specs/features/cluichetest/cluichetestscenarios/smoke-scenario.md) | **Done** — `dia orchestrate` passes end-to-end |
| 8 | [Metric threshold assertions](specs/features/dia/diaautomation/metric-assertions.md) — `dia.automation.get_metric` + pytest `assert_metric` fixture | **Done** (2026-05-24) |

### TestStages — Needs `/spec-feature` per stage

System spec: [teststages.md](specs/systems/cluichetest/teststages.md) (Approved). Pattern: one manifest stage per engine feature, one Module per stage, checkpoints as validation contract. Star topology: Boot → Stage → Boot.

**Candidate stages** (build in any order — independent once pattern established):

| Stage | Engine System | Validates | Checkpoint examples | Needs |
|-------|--------------|-----------|---------------------|-------|
| RigidBody2DStage | DiaRigidBody2D | Circle drop + settle detection, collision response | `rigid_body.circle_settled`, `rigid_body.collision_detected` | `/spec-feature` |
| StateMachineStage | DiaStateMachine | State transitions, guard evaluation, event firing | `state_machine.reached_target`, `state_machine.guard_blocked` | `/spec-feature` |
| Animation2DStage | DiaAnimation2D + DiaRig2D | Clip playback, pose validation, blend weights | `animation.clip_complete`, `animation.pose_matches` | `/spec-feature` |
| SoftBody2DStage | DiaSoftBody2D | Rope/cloth stabilization, spring convergence | `soft_body.rope_settled` | `/spec-feature` |
| GeometryStage | DiaGeometry2D | Intersection tests, spatial queries | `geometry.intersection_correct` | `/spec-feature` |
| EntityTestStage | DiaEntity | Spawn, query, destroy, component lifecycle | `entity.count_correct`, `entity.hierarchy_valid` | DiaEntity implemented first |

**First stage to build:** RigidBody2DStage — physics is already stable, no other system dependencies, clear pass/fail checkpoint (body settles). Proves the test-stage pattern works.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| 7 | CluicheEditor EditorAutomationModule | After 6b |

---

## Static Bug Detection Stack

Research complete: [docs/research/static_cpp_bug/](research/static_cpp_bug/). Bundle A chosen.

### Active (no blockers)

| Item | What's needed | Notes |
|------|--------------|-------|
| `sanitizer-configs` | Implement | [sanitizer-configs.md](specs/features/dia/diabugdetection/sanitizer-configs.md) ✅ — `Debug-Asan` + `Debug-Ubsan` configs; `dia run googletest --config Asan\|Ubsan`; `dia check --tool=sanitizer` |
| `cppcheck-integration` | Implement | [cppcheck-integration.md](specs/features/dia/diabugdetection/cppcheck-integration.md) ✅ — winget install; `dia check`; SARIF output; `.cppcheck-suppressions.xml` |
| `ci-gate` | Implement after cppcheck-integration | [ci-gate.md](specs/features/dia/diabugdetection/ci-gate.md) ✅ — `dia pipeline --stage static-analysis`; baseline diff; `delta.sarif` |
| `dia-diagnose-loop` | Implement last (needs findings from above) | [dia-diagnose-loop.md](specs/features/dia/diabugdetection/dia-diagnose-loop.md) ✅ — agentic Claude fix loop; stage on success; revert on stuck |

### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| Clang-Tidy analysis | CMake migration (compile_commands.json) | Full project coverage once CMake is the build system; start with DiaCore module mirror |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| DiaAssetRuntime — Hot reload path | Asset Lifecycle Management adds `Failed → Loading` retry but no `Loaded → Loading → Loaded` path. Still need a `ReloadAsset(assetId)` for live iteration (future feature on top of lifecycle management). |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| Phase 3d — Physics body serialization | DiaRigidBody2D / DiaSoftBody2D body definitions — DiaAssetCatalogue ✅ now unblocked |
