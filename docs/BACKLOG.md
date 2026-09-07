# Cluiche Backlog

Derived from spec status across `docs/specs/`. When a spec moves to Done, move it to [BACKLOG-HISTORY.md](BACKLOG-HISTORY.md).

---

## Ready to Build (Approved → implement)

These specs are `Approved` with all features `Approved`. No spec work needed — go straight to implementation.

### Systems

| System | Features | Depends On |
|--------|----------|------------|
| ~~DiaAttribute~~ | System spec + all 6 feature specs Approved ✅: core, conditional-modifiers, change-notifications, accessor-bridge, visual-debugger, save-serialization. Data-driven gameplay attribute/stat framework — same idiom as DiaEconomy (StringCRC, JSON schema, DiaCondition-gated modifiers, Observer events), no shared code. Plan: [diaattribute.plan.md](specs/applications/dia/systems/diaattribute/diaattribute.plan.md) — 6 tasks, 3 phases. Entry point: task 1 (core). Task 1 has an open design question (component attachment mechanism) to resolve before `AttributeSetComponent`; task 4 (accessor-bridge) is the highest-uncertainty task — `DiaCondition`'s `ConditionRegistry` has no unregister method, may need an upstream fix. Deferred: Archetype/Instance Layering. Parked: Dependency-Graph Derived Attributes, Non-Numeric Typed Properties. Research: [gameplay_attribut_stat_system/summary.md](research/gameplay_attribut_stat_system/summary.md). | DiaCore ✅, diaentitytemplate ✅, DiaCondition ✅ (optional), DiaSaveGame ✅ (optional) |

---

## Spec Work Needed (Draft or unset — review/approve before building)

### Other Spec Work

| Item | Spec | What's needed |
|------|------|---------------|
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

---

## Loose Ends (non-spec items)

| Item | Notes |
|------|-------|
| RenderTechnique asset type | Layer-level rendering policy (blend mode, post-process like bloom/distortion). Layers reference a technique by name; renderer resolves at draw time. Needs `/spec-feature` under DiaGraphics or DiaBgfx once the scene system lands. |
| Camera2D controller (pan/zoom/reset) | Application-side input→Camera2D wiring for CluicheTest stages (keyboard pan, scroll zoom, home-key reset). Unblocked once coord2d-debug-overlay ships Camera2D + renderer integration. |
| Architecture layer violation remediation | `dia check arch` flags ~35 violations across 6 independent root causes (VisualDebugger family → DiaVisualDebugger — the long-standing ~42-file conflict below, now audited; DiaScalarFieldInspector → DiaEditor; GridVisibilityVisualDebugger mistagged vs. its own domain; a duplicate RGBA type pulling RigidBody2DVisualDebugger into the visual domain; DiaSensor/DiaTriggerScript falsely attributed to `dia.root` via malformed module.md metadata; DiaSimTime → DiaSaveGame). Audit complete with a recommended fix and task breakdown per cluster: [arch_layer_violations/plan.md](refactors/arch_layer_violations/plan.md). One task (retagging `dia.editor` from `domain/visual/core` to `foundation/application`) is gated on explicit sign-off before implementation. |
