# Feature Spec: ImGui Bootstrap Menu

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheTest | @docs/specs/applications/cluichetest.md |
| System | ApplicationFlow | @docs/specs/systems/cluichetest/applicationflow.md |
| Feature | ImGui Bootstrap Menu | (this document) |

## Research

@docs/research/bootstra_imgui_ui/summary.md

## Summary

Replace CluicheTest's Ultralight/Webix boot screen with an ImGui stage-selection menu. Two new modules: `DebugUIModule` (global, all stages — owns ImGui lifecycle via DiaImGui) and `BootMenuModule` (Boot stage only — renders the stage menu). The existing `BootUIPageModule`, `LaunchUIPage`, and `bootscreen.html` are removed. `UIModule` (Ultralight) becomes stage-scoped so Boot no longer pays Ultralight init cost.

## Goals

1. Simplify the Boot stage by removing the web-stack dependency (Ultralight + Webix HTML + JS bindings)
2. Provide a developer-facing stage-selection menu using ImGui that shows stage status
3. Establish `DebugUIModule` as the global ImGui lifecycle owner, enabling future debug overlays in any stage
4. Scope `UIModule` (Ultralight) to only stages that need player-facing UI (DummyStage / future UI Stage)

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `DebugUIModule` runs globally (all stages) on RenderPU; initializes `DiaImGuiManager` + `SFMLImGuiBackend`; calls NewFrame/Render each update |
| AC2 | `BootMenuModule` runs on Boot stage only on RenderPU; depends on `DebugUIModule`; renders ImGui window titled "CluicheTest — Bootstrap" |
| AC3 | Menu lists all navigable stages from `Application::GetStageTransitions()` with loaded-this-session indicator (● loaded, ○ not loaded) |
| AC4 | Menu shows per-stage test status badge (PASSED / FAILED / NOT RUN) from `TestResultsRegistry` |
| AC5 | "Launch" button or Enter key calls `GetApplication()->TransitionTo(selectedStageCRC)` |
| AC6 | "Automation: Connected" badge displayed when WebSocket automation client is connected |
| AC7 | E2E automation (`dia.automation.navigate_to`) continues to work unchanged — menu is visual convenience, automation bypasses it |
| AC8 | `UIModule` (Ultralight) does NOT initialize during Boot stage — stage-scoped to stages that declare UI needs |
| AC9 | DummyStage retains Ultralight via `UIModule` (future: becomes dedicated UI Stage) |
| AC10 | `BootUIPageModule`, `LaunchUIPage`, `bootscreen.html` removed from codebase |
| AC11 | `SFMLImGuiBackend` works in Release builds (remove `#ifdef DIA_DEBUG` gate) |
| AC12 | Session stats footer shows passed/failed/pending counts |
| AC13 | Boot stage does NOT auto-advance — requires user Launch or automation navigate_to |

## Architecture

### Module Map (Boot Stage)

```
MainPU (global modules):
  ├─ LoggerModule
  ├─ KernelModule              (window, input)
  ├─ AssetServiceModule
  ├─ DebugServerHostModule
  ├─ AutomationModule
  └─ ObservationModule

RenderPU (global modules):
  ├─ RenderModule              (fetch SimToRender, draw to canvas)
  └─ DebugUIModule [NEW]       (ImGui lifecycle: init, NewFrame, Render)

RenderPU (Boot stage modules):
  └─ BootMenuModule [NEW]      (ImGui stage menu, depends on DebugUIModule)

MainPU (DummyStage modules):
  └─ UIModule                  (Ultralight — stage-scoped, NOT global)
```

### DebugUIModule

- **Type**: Global module (all stages), RenderPU
- **Dependencies**: RenderModule (owns GL context and SFML window)
- **Responsibility**: Initialize `DiaImGuiManager` with `SFMLImGuiBackend` in DoStart; call `NewFrame()` at start of each Update; call `Render()` at end of each Update. Any module running on RenderPU can submit ImGui commands between NewFrame and Render. Exposes `IsFrameActive()` for safety.
- **Lifecycle**: Lives for entire application lifetime. Runs on same thread as the GL context (RenderPU dedicated thread). Other RenderPU modules submit ImGui draw commands during their own Update between the NewFrame/Render bookends.

### BootMenuModule

- **Type**: Boot stage module, RenderPU
- **Dependencies**: DebugUIModule (ImGui frame available), AutomationModule (connection status query via ModuleRef cross-PU)
- **Responsibility**: Render the bootstrap ImGui window. Query `Application::GetStageTransitions()` for stage list. Query `TestResultsRegistry` for status. Track loaded-this-session state (set when returning from a stage). Handle Launch button / Enter key → `TransitionTo()`.
- **Lifecycle**: Starts when Boot stage entered, stops when Boot stage exited. On return to Boot (after a stage completes), re-starts and shows updated status.
- **Note**: `TransitionTo()` is thread-safe (queued), so calling from RenderPU is valid.

### UIModule Scoping

- **Before**: UIModule runs in `stages: ["all"]` — Ultralight initializes immediately at application start
- **After**: UIModule runs in `stages: ["DummyStage"]` (or whichever stages declare UI needs via `.diastage` config)
- Boot stage never creates Ultralight system

### Render Path

ImGui renders through the existing SFML/OpenGL context on RenderPU — same thread that owns the GL context. DebugUIModule calls ImGui NewFrame/Render as part of the RenderPU update cycle, after RenderModule draws game content from SimToRender. ImGui overlays on top of the game frame (or on a cleared background during Boot when no game content is being rendered).

### Stage Transition (SD-003 Superseded)

Boot stage no longer auto-advances. Two paths to leave Boot:
1. **Manual**: User selects stage + clicks Launch / presses Enter
2. **Automation**: `dia.automation.navigate_to` command via WebSocket calls `TransitionTo()` directly

When a stage completes and transitions back to Boot, BootMenuModule re-starts and shows updated loaded/status indicators.

## Tasks

| # | Task | Dependencies |
|---|------|--------------|
| 1 | Remove `#ifdef DIA_DEBUG` gate from `SFMLImGuiBackend` — ImGui available in all configs | — |
| 2 | Implement `DebugUIModule` (global, RenderPU): init DiaImGuiManager, NewFrame/Render per update | T1 |
| 3 | Implement `BootMenuModule` (Boot stage, RenderPU): ImGui window, stage list, status, Launch | T2 |
| 4 | Scope `UIModule` to stage-specific (remove from "all", add to DummyStage manifest) | — |
| 5 | Remove `BootUIPageModule`, `LaunchUIPage`, `bootscreen.html`, related Webix assets | T3 |
| 6 | Update `cluiche_main.diaapp` manifest: add DebugUIModule (global), BootMenuModule (Boot), update UIModule stages | T2, T4 |
| 7 | Supersede SD-003 in system spec (Boot no longer auto-advances) | T3 |
| 8 | Verify E2E automation still works (`dia.automation.navigate_to` from Boot) | T3, T6 |

## Binding Decisions Compliance

| ID | Decision (plain language) | Compliance |
|----|---------------------------|------------|
| PD-001 | Use StringCRC for all IDs | Stage CRCs from `GetStageTransitions()` used for transition; module instance IDs are StringCRC |
| PD-002 | ProcessingUnit/Phase/Module architecture | Both new modules are proper DiaApplicationFlow Modules on RenderPU with correct lifecycle |
| PD-004 | No STL in public APIs | Module interfaces use DiaCore containers; ImGui internal STL is encapsulated |
| PD-005 | x64 Windows only | Single-platform ImGui backend (Win32 + OpenGL via SFML) |
| PD-007 | C++20 required | Module code compiled with /std:c++20 |
| PD-008 | Directory.Build.props owns build paths | No per-project output path overrides |
| PD-010 | .diagame root, .diastage for stages | Stage-scoping of UIModule uses existing manifest config mechanism |
| AD-001 | Three PUs (Main/Render/Sim) | Maintained — DebugUIModule and BootMenuModule on RenderPU (owns GL context) |
| AD-003 | Entry point in Main.cpp | No change to Main.cpp bootstrap (manifest-driven module registration) |
| AD-005 | Application is testbed, not shipped product | ImGui developer menu is appropriate for a testbed |
| SD-001 | Three PUs: Main (infra), Sim (game), Render (draw) | DebugUIModule is rendering infrastructure on RenderPU — correct placement (needs GL context) |
| SD-002 | Main never changes stage | BootMenuModule calls TransitionTo() which is app-wide (not PU-specific) — compliant |
| SD-003 | Boot auto-advances | **SUPERSEDED** by this feature — Boot now requires manual or automation trigger |
| SD-005 | Stage-specific modules from .diastage | BootMenuModule is stage-scoped via manifest config |
| SD-007 | All modules log DoStart/DoStop | DebugUIModule and BootMenuModule will log at entry/exit |
| SD-008 | Canvas/Window pre-created, passed as bootstrap resource | DebugUIModule accesses render context via RenderModule (same PU) — no change to bootstrap |
| SD-010 | KernelModule calls RequestShutdown on window close | No change — still works |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Render Path | Does ImGui rendering conflict with RenderPU drawing to the same SFML window? | Resolved: DebugUIModule runs on RenderPU (same thread as GL context). No cross-thread conflict. ImGui renders after game content as an overlay. |
| 2 | DebugUIModule | Should DebugUIModule expose an API for other modules to check "is ImGui frame active" before submitting commands? | Yes — a simple `IsFrameActive()` accessor prevents modules from calling ImGui outside the NewFrame/Render window. |
| 3 | UIModule Scoping | If UIModule moves to DummyStage-only, does any other existing module depend on it during Boot? | Confirmed: BootUIPageModule is the only Boot consumer of UIModule. No other module references Ultralight during Boot. |
| 4 | Loaded Indicator | How is "loaded this session" tracked — BootMenuModule instance state or TestResultsRegistry? | BootMenuModule tracks a per-stage bitfield in its own state. Set when Application transitions back to Boot from that stage. Resets each application run. |
| 5 | SD-003 Supersede | Does superseding SD-003 affect E2E tests that assume Boot auto-advances? | Confirmed no impact. E2E tests use `dia.automation.navigate_to` explicitly — they don't rely on auto-advance. |

## Status

`Approved` — 2026-05-24. Plan: [imgui-bootstrap-menu.plan.md](imgui-bootstrap-menu.plan.md)