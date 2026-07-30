# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| ~~DiaAIBudget~~ | IAIBudgetedSystem, AIBudgetScheduler, AIBudgetModule, Budget Metrics | DiaApplicationFlow ✅, DiaMetrics ✅, DiaCore ✅ |
| ~~DiaCondition~~ | IConditionContext, ConditionRegistry, ConditionExpr, ConditionGuardAdapter, Test Utilities | DiaCore ✅, DiaStateMachine ✅ |
| ~~DiaRules~~ | RuleActionRegistry, RuleSet, RuleSetComponent, Test Utilities | DiaCondition ✅ |
| DiaUtilityAI | ResponseCurve, ActionDef, UtilitySet, AsyncEvaluation, GroupConsideration, UtilitySetComponent, ScoreOverlay, TestUtilities | DiaCondition ✅, DiaRules ✅, DiaAIBudget ✅ |
| DiaHTN | OperatorRegistry, RuleActionBridge, HTNDomain, HTNPlan, SyncPlanner, AsyncPlanning, HTNPlannerComponent, TestUtilities | DiaCondition ✅, DiaRules ✅, DiaAIBudget ✅ |

---

### Standalone Features (system Done, feature Approved)

| Feature | System | Depends On |
|---------|--------|------------|
| AI Personality | DiaUtilityAI | DiaUtilityAI core built first |

---

## In Progress

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| ~~DiaAIDecisionInspector~~ | — | Needs `/spec-system` — CluicheEditor panel showing per-entity AI decision state: blackboard slots → condition results → fired rules → utility scores in one vertical read. Depends on DiaAIBudget, DiaCondition, DiaRules, DiaUtilityAI all built + at least one CluicheTest entity running all four systems. DiaEntityInspector (stub) is a soft prerequisite for entity selection. |
| RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline ✅ unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

---

## E2E Orchestration Stack

Architecture redesigned 2026-05-20. Source of truth: **[docs/research/e2e_testing/design-decisions.md](research/e2e_testing/design-decisions.md)**.

### Deferred

| # | Item | Notes |
|---|------|-------|
| 6b | Extract AutomationModuleBase into DiaAutomation | Evaluate from working code after 2+ apps use it |
| DiaRig3D system | feature spec exists (`skeleton-and-pose.md`) | Needs `/spec-system` — Bone3D, Skeleton3D, Pose3D, FK, `SkeletonComponent3D`, `Rig3DAsset`. Mirrors DiaRig2D. | 
| DiaAnimation3D system | feature spec exists (`clip-and-player.md`) | Needs `/spec-system` — AnimationClip3D, ClipPlayer3D, STEP/LINEAR/CUBICSPLINE, `AnimationComponent3D`. glTF animation import is build-time only. |
| DiaStateMachineEditor system | TBD | Needs `/spec-system` — editor plugin for state machine visual debugging + design-time editing. Depends on DiaStateMachine ✅, DiaEditor |
| DiaSkinning3D | TBD — needs `/spec-system` | `skinning-palette` feature already Approved; needs own system spec. SkinningManager, per-frame Matrix34 palettes, `skinningPaletteIndex` on draw commands. | DiaAnimation3D, DiaGraphics3D |
---

### Blocked on Linux/CMake migration

| Item | Blocked by | Notes |
|------|-----------|-------|
| Clang-Tidy analysis | CMake migration (compile_commands.json) | Unblocked by C2 (Foundation CMake pilot) — see DiaArchitecture system below |
| TSan (ThreadSanitizer) | Linux target (WSL2 CI) | Only reliable race detector for Main/Render/Sim threading model; TSan doesn't run on Windows |

---

## E2E Test Coverage Gaps

### Quick wins (no new C++ work needed)

| Item | Notes |
|------|-------|
| Add RigidBody2D + SoftBody2D scenarios to default.json | `scenarios/cluichetest/rigidbody2d/test_rigidbody2d_settle.py` and `softbody2d/test_softbody2d_settle.py` exist on disk but are missing from `plans/cluichetest/default.json`. Add both entries. |
| Dedicated scenario: EntityTestStage | 6 checkpoints (spawn_complete, query_correct, hierarchy_valid, destroy_cascade, mailbox_received, lifecycle_complete) — richest stage, generic runner only. Write `scenarios/cluichetest/entity/test_entity_stage.py`. |
| Dedicated scenario: Scene2DTestStage | 5 checkpoints (scene.loaded, cameras_hydrated, lights_hydrated, entities_spawned, layers_resolved). Write `scenarios/cluichetest/scene2d/test_scene2d_stage.py`. |
| Dedicated scenario: Animation2DTestStage | 2 checkpoints (animation2d.clips_completed, pose_correct). Write `scenarios/cluichetest/animation2d/test_animation2d_stage.py`. |
| Dedicated scenario: IK2DTestStage | 3 checkpoints (ik2d.two_bone_converged, fabrik_converged, look_at_accurate). Write `scenarios/cluichetest/ik2d/test_ik2d_stage.py`. |
| Dedicated scenario: Geometry2DTestStage | 1 checkpoint. Write `scenarios/cluichetest/geometry2d/test_geometry2d_stage.py`. |
| Dedicated scenario: TestAssetRuntimeStage | 2 checkpoints (all_loaded, clean_reload). Write `scenarios/cluichetest/asset_runtime/test_asset_runtime_stage.py`. |

### Needs investigation

| Item | Notes |
|------|-------|
| E2E suite: UIUltralightTestStage hard crashes app on generic runner | `test_all_stages.py` navigates every stage; UIUltralightTestStage crashes the app (no error log, instant death) when `UIUltralightTestStageModule::BeginStart` fires. Affects all subsequent tests in the session. Root cause unknown — likely Ultralight native library crash during DoStart. Needs investigation: add crash guard / null-check in UIUltralightTestStageModule::DoStart, or find the Ultralight init failure. See `docs/research/e2e_testing/uiultralight_crash_notes.md`. |

### Needs new C++ stage

| Item | Notes |
|------|-------|
| ~~AIDecisionTestStage + scenario~~ | Integration proof that DiaCondition + DiaRules + DiaUtilityAI + DiaAIBudget all wire together correctly. Single entity with BlackboardComponent + RuleSetComponent + UtilitySetComponent. Checkpoints: `ai.condition.health_low_passes`, `ai.condition.enemy_visible_passes`, `ai.rules.call_for_help_fired`, `ai.utility.flee_wins`, `ai.budget.work_item_completed`. Prerequisite: all four AI systems built. |
| E2E suite: Pathfinding CluicheTest stage + scenario | No E2E coverage for pathfinding. Needs a CluicheTest stage that runs a pathfinding agent to a goal and a pytest scenario that checkpoints arrival. Prerequisite: DiaPathfinding system built and integrated into CluicheTest. |

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| ~~`dia diagnose --last-run` CLI command~~ | Automate crash triage: find latest session log, extract last module transition + all ERROR entries, show incomplete E2E report, check `%LocalAppData%\CrashDumps`. Saves the manual log-digging cycle after every crash. |
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| ~~E2E suite: RigidBody2DTestStage / SoftBody2DTestStage checkpoint failures~~ | ~~Doubled frame budgets (900→1800, 600→1200) and pytest timeouts (15s→100s, 14s→70s) to match ~50ms Debug SimPU frame rate.~~ |
