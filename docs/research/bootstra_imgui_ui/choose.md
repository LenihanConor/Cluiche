# Research: Choice — Bootstrap UI to ImGui

**Date:** 2026-05-24
**Chosen candidate:** C4 — ImGui Boot + Stage-Scoped UIModule + Debug Opt-In

## Rationale

C4 directly implements the agreed two-module architecture: `DebugUIModule` (global, owns ImGui lifecycle) and `BootMenuModule` (Boot stage only, renders the stage-selection menu). It also scopes `UIModule` (Ultralight) to stages that declare UI needs — so Boot pays zero Ultralight cost, while DummyStage (future UI Stage) retains it. This establishes a clean pattern for debug overlays in any stage without the complexity of a thread-safe command buffer (that can come later if needed).

Scored highest on Cluiche Fit (respects PD-002 Module/PU pattern, PD-010 data-driven stage config) and balances Engine Value with Implementation Cost.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C2: Global ImGui Layer (RenderPU) | Thread-safe command buffer is overkill for near-term needs; can be Phase 2 if other stages need cross-PU ImGui |
| C1: Boot-Only ImGui (MainPU Render) | Too narrow — doesn't solve UIModule scoping or establish reusable debug overlay pattern |
| C3: Hybrid — Ultralight Stays Global | Leaves architectural debt (Boot pays Ultralight init for nothing) |
| C5: CLI + ImGui Fallback | Nice-to-have CLI shortcut but automation already works via WebSocket |
| C6: Data-Driven Menu from .diagame | Over-engineered for 3 stages; manifest metadata additions can come later |
| C7: ImGui Boot + Test Runner Controls | "Run All" / "Re-run Failed" explicitly not wanted; just Launch |
| C8: Fullscreen Dashboard | Way too large; dashboard can grow organically from the simple boot menu |

## Pre-Spec Commitments

- **Two modules**: `DebugUIModule` (global, all stages) owns ImGui lifecycle via `DiaImGuiManager`. `BootMenuModule` (Boot stage only) renders the menu and depends on `DebugUIModule`.
- **UIModule (Ultralight)** becomes stage-scoped — only initializes for stages that declare UI needs (DummyStage / future UI Stage). Boot stage does not create Ultralight.
- **Menu layout** per mockup (`mockup_boot_menu.html`): dot (●/○ loaded this session), stage name, status badge (PASSED/FAILED/NOT RUN). Single "Launch" button. "Automation: Connected" badge. Title "CluicheTest — Bootstrap". Enter = Launch.
- **Automation unchanged**: WebSocket `dia.automation.navigate_to` continues to work — menu is visual convenience only.
- **Phasing**: Phase 1 = `DebugUIModule` + `BootMenuModule` (boot menu working). Phase 2 = UIModule stage-scoping + debug overlay opt-in for game stages.

## Next Step

Run /spec-feature with this candidate as input.
Suggested parent system: CluicheTest application flow (docs/specs/systems/cluichetest/applicationflow.md)
