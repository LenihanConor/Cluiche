# Research Summary — Bootstrap UI to ImGui

**Session folder:** docs/research/bootstra_imgui_ui/
**Date:** 2026-05-24

## One-Line Answer

Replace CluicheTest's Ultralight/Webix boot screen with an ImGui stage-selection menu via two new modules: `DebugUIModule` (global ImGui lifecycle) and `BootMenuModule` (Boot stage menu with stage status).

## Journey

1. **Explored:** CluicheTest's boot screen uses Ultralight + Webix HTML to render a stage list with test-result badges. The engine already has `DiaImGui` with `IImGuiBackend` abstraction and `SFMLImGuiBackend` — all infrastructure exists, just not wired into the boot flow.
2. **Ideated:** 8 candidates generated ranging from minimal swap (S) to full dashboard (L). Discussion refined the architecture: `DebugUIModule` for ImGui (developer-facing), `UIModule` for Ultralight (player-facing), clear separation of concerns.
3. **Evaluated:** C4 (ImGui Boot + Stage-Scoped UIModule + Debug Opt-In) scored highest (3.75) on combined engine value, fit, and manageable cost. Beats the thread-safe command buffer approach (C2) on pragmatism and the minimal swap (C1) on architectural reuse.
4. **Chose:** C4 confirmed. Two-phase delivery: Phase 1 ships the boot menu, Phase 2 scopes Ultralight and adds debug overlay opt-in for game stages.

## Chosen Work Item

**Name:** ImGui Boot Menu + Stage-Scoped UIModule
**Home module:** CluicheTest (DebugUIModule + BootMenuModule) with DiaImGui as engine dependency
**Suggested spec type:** Feature (under CluicheTest application flow system)
**Estimated size:** M (1–3 weeks, two phases)

## Key Insights from Exploration

- Boot stage has no game rendering — MainPU can own the ImGui render context during Boot without conflicting with RenderPU. Simplifies Phase 1 dramatically.
- `TestResultsRegistry` (singleton, per-stage pass/fail/timeout/running) already exists and provides all status data the menu needs.
- `Application::GetStageTransitions()` returns navigable stages at runtime — menu auto-discovers stages from manifest without hardcoding.
- `UIModule` currently runs globally (all stages) which means Boot pays Ultralight init cost for nothing. Stage-scoping via `.diastage` config aligns with PD-010.
- Automation (`dia.automation.navigate_to` via WebSocket) is completely independent of the boot menu — no changes needed to E2E infrastructure.
- The `#ifdef DIA_DEBUG` gate on `SFMLImGuiBackend` may need revisiting if the boot menu should work in Release builds.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C2: Global ImGui Layer (RenderPU) | Thread-safe command buffer overkill for near-term; Phase 2 if needed |
| C1: Boot-Only ImGui | Too narrow, no reusable debug overlay pattern |
| C3: Hybrid — Ultralight Stays Global | Leaves architectural debt (Boot pays for unused Ultralight) |
| C5: CLI + ImGui Fallback | Automation already works via WebSocket |
| C6: Data-Driven Menu from .diagame | Over-engineered for current stage count |
| C7: Test Runner Controls | "Run All" / "Re-run Failed" explicitly not wanted |
| C8: Fullscreen Dashboard | Too large; can grow organically from simple menu |

## References

- docs/research/bootstra_imgui_ui/explore.md
- docs/research/bootstra_imgui_ui/ideate.md
- docs/research/bootstra_imgui_ui/evaluate.md
- docs/research/bootstra_imgui_ui/choose.md
- docs/research/bootstra_imgui_ui/mockup_boot_menu.html
