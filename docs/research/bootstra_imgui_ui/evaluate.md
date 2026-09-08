# Research: Evaluate — Bootstrap UI to ImGui

**Input:** docs/research/bootstra_imgui_ui/ideate.md

**Refinement from discussion:** All candidates now assume a two-module split:
- **`DebugUIModule`** (global) — owns ImGui lifecycle (`DiaImGuiManager`, NewFrame/Render)
- **`BootMenuModule`** (Boot stage only) — renders stage-selection menu, depends on `DebugUIModule`

This is factored into scoring — candidates that already assumed this structure score higher on Fit.

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability — does this give other apps/stages something they can reuse?
- **Game Value (0.20):** Improves CluicheTest as a demo/testbed — does this make day-to-day testing better?
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure, PD-001–PD-010, two-module architecture

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| C1: Boot-Only ImGui (MainPU Render) | 2 | 4 | 5 | 4 | 3 | 3.55 |
| C2: Global ImGui Layer (RenderPU) | 5 | 4 | 2 | 2 | 5 | 3.60 |
| C3: Hybrid — Ultralight Stays Global | 2 | 3 | 5 | 5 | 2 | 3.35 |
| C4: ImGui Boot + Stage-Scoped UI + Debug Opt-In | 4 | 4 | 3 | 3 | 5 | 3.75 |
| C5: CLI + ImGui Fallback | 2 | 4 | 4 | 5 | 3 | 3.45 |
| C6: Data-Driven Menu from .diagame | 3 | 4 | 3 | 3 | 4 | 3.40 |
| C7: ImGui Boot + Test Runner Controls | 3 | 5 | 3 | 3 | 4 | 3.55 |
| C8: Fullscreen ImGui Dashboard | 4 | 5 | 1 | 2 | 4 | 3.10 |

## Top 3 Candidates

### Rank 1: C4 — ImGui Boot + Stage-Scoped UIModule + Debug Opt-In (score: 3.75)

**Why:** This candidate directly implements the agreed two-module architecture (`DebugUIModule` global + `BootMenuModule` Boot-only) while also cleaning up the UIModule scoping so Ultralight only pays init cost for stages that declare UI needs. It delivers the boot menu, solves the TestStageHUDModule cross-thread problem, and establishes a pattern for future debug overlays — all within a clean Module/PU architecture that respects PD-002. The stage-scoping of UIModule via `.diastage` config aligns with PD-010's data-driven philosophy.

**Watch out for:** Medium scope — three concerns in one (boot menu + UIModule refactor + debug overlay plumbing). Could be phased: DebugUIModule + BootMenuModule first, UIModule scoping second. Thread ownership of ImGui render needs careful design if `DebugUIModule` runs globally alongside RenderPU.

### Rank 2: C2 — Global ImGui Layer on RenderPU (score: 3.60)

**Why:** Highest engine value — makes DiaImGui a proper cross-thread rendering service that any module on any PU can consume. This is the "do it right" solution for the ImGui thread-safety problem that blocked TestStageHUDModule. Thread-safe command buffer pattern is well-established in game engines.

**Watch out for:** Highest complexity and risk. Building a thread-safe ImGui command buffer (serialize draw calls from MainPU/SimPU, replay on RenderPU) is non-trivial. Overkill if only Boot stage actually needs ImGui rendering near-term. Could be Phase 2 after the boot menu ships.

### Rank 3: C1 / C7 (tied at 3.55)

**C1 — Boot-Only ImGui (MainPU Render):**
**Why:** Simplest path to "ImGui boot menu working." Boot stage has no game rendering, so MainPU can own the render context during Boot without conflicting with RenderPU. Fastest to deliver.
**Watch out for:** Doesn't solve the global debug overlay problem. `DebugUIModule` would be Boot-only in practice, limiting reuse. May need rework when other stages want ImGui.

**C7 — ImGui Boot + Test Runner Controls:**
**Why:** Highest game value — turns the boot menu into a mini test harness with "Run All", per-stage controls, and result export. Very developer-friendly for the CluicheTest use case.
**Watch out for:** Scope creep risk. Test runner logic (sequential stage loading, timeout handling, result aggregation) is substantial. Could be Phase 2 on top of the basic boot menu.

## Recommendation

**C4 (ImGui Boot + Stage-Scoped UIModule + Debug Opt-In)** is the recommended choice. It directly implements the `DebugUIModule` + `BootMenuModule` architecture agreed in discussion, cleans up Ultralight scoping so Boot stage is lightweight, and establishes the pattern for debug overlays in all stages. The medium scope is manageable if phased: Phase 1 delivers `DebugUIModule` (global ImGui lifecycle) + `BootMenuModule` (stage menu with status). Phase 2 scopes UIModule to stages that need it and adds debug overlay opt-in for game stages.

It beats C2 because it doesn't require a thread-safe command buffer upfront — if `DebugUIModule` runs on MainPU and Boot stage doesn't use RenderPU, the simple path works for Phase 1. It beats C1/C3 because it establishes reusable architecture rather than a one-off hack. It respects PD-002 (Module/PU pattern), PD-010 (data-driven stage config), and the agreed naming convention (`DebugUIModule` for ImGui, `UIModule` for Ultralight).
