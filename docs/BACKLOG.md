# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

_Nothing here._


---

### Standalone Features (system Done, feature Approved)

_Nothing here._

---

## In Progress

_Nothing here._

---

## Spec Work Needed (Draft or unset — review/approve before building)

| Item | Spec | What's needed |
|------|------|---------------|
| DiaAIBudget | [diaaibudget.md](specs/applications/dia/systems/diaaibudget/diaaibudget.md) | Spec `Draft` — awaiting approval. Frame-budget AI scheduler: `AIBudgetScheduler` (priority-ordered work queue, Critical/Normal/Background tiers, microsecond flush), `AIBudgetModule` (IModule wrapper on SimPU), DiaMetrics counters. Hard dependency of DiaUtilityAI. |
| DiaCondition | [diacondition.md](specs/applications/dia/systems/diacondition/diacondition.md) | Spec `Draft` — awaiting approval. Shared expression evaluator: `ConditionRegistry` (float/bool accessor registration), `ConditionExpr` (JSON-loadable boolean expression tree), `ConditionGuardAdapter` (zero-change DiaStateMachine integration). Foundation for DiaRules + DiaUtilityAI. Depends on DiaBlackboard ✅. |
| DiaRules | [diarules.md](specs/applications/dia/systems/diarules/diarules.md) | Spec `Draft` — awaiting approval. Forward-chaining rule engine: `RuleActionRegistry` (open handler registration by StringCRC), `RuleSet` (all-matching condition→action evaluation), JSON loader, `RuleSetComponent`. Depends on DiaCondition. |
| DiaUtilityAI | [diautilityai.md](specs/applications/dia/systems/diautilityai/diautilityai.md) | Spec `Draft` — awaiting approval. Score-based action selection: `ResponseCurve` (easing-shaped scorers), `ActionDef` (prerequisites + scorers + cooldown + max_concurrent), `GroupConsiderationContext` (squad coordination), `UtilitySet` (sync + async eval), DiaVisualDebugger score overlay. Depends on DiaAIBudget + DiaCondition. |
| RenderTestPlugin (CluicheEditor) | — | Needs `/spec-system` — visual debugger panel: wipe slider, region grid, expectation authoring, AI triage panel, render targets. DiaRenderTest CLI Pipeline now shipped ✅ — this is unblocked. Mockup: [render_test_debugger_mockup.html](research/render_offline_test/render_test_debugger_mockup.html). Research: [render_offline_test/summary.md](research/render_offline_test/summary.md) |

---

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

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| `Dia::Core::Blackboard` — general-purpose key-value store | Identified during DiaStateMachine research; useful for AI, animation, gameplay. Needs `/spec-feature` under DiaCore. |
| **DiaBgfx3D — Specular / simple PBR follow-up** | GGX/Blinn-Phong BRDF next iteration. PBR foundation shipped; further tuning/extension. Needs `/spec-feature` under DiaBgfx3D. |
