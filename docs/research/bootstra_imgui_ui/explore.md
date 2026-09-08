# Research: Explore — Bootstrap UI to ImGui

**Session date:** 2026-05-24
**Folder:** docs/research/bootstra_imgui_ui/

## Problem Space Overview

CluicheTest's Boot stage currently uses an Ultralight-rendered HTML page (`bootscreen.html` + Webix 5.2.1) to show a stage-selection menu. This page lists available stages, shows test-result badges, and triggers stage transitions via JavaScript-bound C++ methods. The system works but introduces a heavyweight dependency (Ultralight browser engine + HTML/JS tooling) for what is functionally a simple list with status badges and a launch button.

The goal is to replace this boot screen with Dear ImGui — already vendored in `External/imgui/` and wrapped by `DiaImGui` (with `IImGuiBackend` abstraction and existing `SFMLImGuiBackend`). ImGui is a natural fit for a developer-facing test application menu. The DummyStage will retain Ultralight (it's being repurposed as a dedicated "UI Stage" for testing the Ultralight/UI system itself), so Ultralight isn't being removed from the project — just from the boot screen.

Key requirements beyond basic stage selection: the menu must show per-stage status (loaded this run? test passed/failed?), and stages must still be loadable by the E2E automation system (WebSocket commands via `AutomationModule`). The ImGui menu is a visual convenience for developers — automation bypasses it entirely.

## Existing Approaches

- **Current approach (Ultralight + Webix HTML)**: `BootUIPageModule` loads `LaunchUIPage` which binds `Application_LaunchLevel`, `GetStageCount`, `GetStageName`, `GetStageStatus` to JS. HTML renders a Webix list with status badges.
- **ImGui direct rendering**: Submit ImGui commands from a module on the render thread (or a thread-safe submission path). Renders a window with selectable stage list.
- **ImGui with docking branch**: Full docking/viewport support for multi-panel tool UIs (overkill for a simple menu).
- **Console/CLI-based selection**: Text-based stage selection via command-line args (already supported by automation, but not interactive).
- **No boot screen (auto-advance)**: Skip the menu entirely, go straight to a default stage. Incompatible with manual testing workflow.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| ImGui thread ownership | Render on MainPU vs RenderPU vs shared submission | TestStageHUDModule was blocked by this; boot screen is simpler (MainPU-only stage) |
| Menu style | Simple window vs fullscreen overlay vs docked panel | Boot is a dedicated stage — can own the whole screen |
| Status data source | TestResultsRegistry (exists) vs new tracking | Registry already has pass/fail/timeout/running |
| Automation interaction | Menu irrelevant to automation vs menu reflects automation state | Automation uses WebSocket → TransitionTo(); menu just shows results |
| Ultralight coexistence | Keep UIModule global vs make UIModule stage-specific | UIModule currently runs in "all" stages; need to decide if Boot still creates Ultralight |
| ImGui initialization | Global (all stages) vs Boot-stage-only | If ImGui is global, other stages can use it for debug overlays too |
| Stage metadata source | .diagame manifest (static) vs Application::GetStageTransitions() (runtime) | Runtime already filters to navigable stages |

## Known Tradeoffs

- **Pro ImGui boot menu**: Zero JavaScript, no Ultralight init cost on Boot stage, single-language stack, trivial to add new status columns, direct C++ access to TestResultsRegistry and Application API
- **Pro keeping Ultralight boot menu**: Already working, richer styling, consistent with DummyStage (shared UIModule), HTML hot-reload for layout tweaks
- **Thread safety concern**: ImGui NewFrame/Render must run on the same thread. Boot stage runs on MainPU. RenderModule runs on RenderPU. If ImGui renders on MainPU, it can't share the render context with RenderPU unless Boot stage doesn't use RenderPU at all (or ImGui gets its own render pass).
- **Ultralight removal from Boot**: If UIModule stays global but Boot doesn't use it, that's wasted init. If UIModule becomes stage-scoped, DummyStage (future UI Stage) still gets it but Boot doesn't — cleaner.
- **ImGui global vs Boot-only**: Making DiaImGui available globally solves the TestStageHUDModule cross-thread issue for future stages too, but adds complexity. Boot-only is simpler.

## Known Pitfalls (C++ / game engine context)

- ImGui rendering requires a valid render context (OpenGL/bgfx). If RenderPU owns the context, MainPU can't call ImGui draw commands directly without synchronization.
- Boot stage currently has no rendering — it's a "menu-only" stage. Adding ImGui rendering means either: (a) Boot stage gets a render pass on MainPU, or (b) ImGui commands are submitted to RenderPU.
- The SFMLImGuiBackend (`Dia/DiaSFML/SFMLImGuiBackend`) is currently `#ifdef DIA_DEBUG` only. Boot menu should work in Release too.
- TestResultsRegistry is a singleton — thread-safe access from the ImGui render must be verified.
- Stage transitions mid-frame: if a user clicks "Launch" and the transition happens immediately, ImGui's frame might be incomplete. Use `TransitionTo()` (queued, not immediate) to avoid this.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaImGui | Already wraps ImGui lifecycle; `IImGuiBackend` + `SFMLImGuiBackend` exist |
| BootUIPageModule | The module being replaced — handles stage list + launch via LaunchUIPage |
| LaunchUIPage | The C++/JS bridge page being replaced — binds stage query/launch methods |
| TestResultsRegistry | Singleton tracking pass/fail/timeout/running per stage CRC — reusable as-is |
| UIModule (CluicheGameBaseline) | Creates Ultralight system; currently global — may need to become stage-scoped |
| AutomationModule | WebSocket server handling `dia.automation.navigate_to` — independent of UI, stays as-is |
| RenderModule | Runs on RenderPU; owns SFML render window and draw loop |
| Application (DiaApplicationFlow) | Provides `GetStageTransitions()`, `TransitionTo()` — all needed APIs exist |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Stage CRCs already used for transitions; ImGui widget IDs can leverage same CRCs |
| PD-002 ProcessingUnit/Phase/Module | ImGui boot menu must be a Module on a specific PU; lifecycle tied to Boot stage |
| PD-004 No STL in public APIs | ImGui uses ImVector internally but Dia wrapper APIs must use DiaCore containers |
| PD-005 x64 Windows only | Single-platform ImGui backend simplifies things |
| PD-007 C++20 | Can use constexpr, concepts in the menu module |
| PD-010 .diagame root file | Stage list comes from manifest imports — already parsed by ManifestComposerV2 |

## Open Questions for Ideation

- Should ImGui init be global (available to all stages for debug overlays) or scoped to Boot only?
- How to handle the render-thread ownership problem: does Boot stage run a simplified render pass on MainPU (no RenderPU), or does ImGui submit to RenderPU?
- Should UIModule (Ultralight) still initialize during Boot, or should it only start when transitioning to a stage that needs it (DummyStage/UI Stage)?
- What status information should the menu show beyond pass/fail? (e.g., last run timestamp, frame count, checkpoint details)
- Should the menu support filtering or grouping stages (e.g., by category, by test status)?
- Does the `#ifdef DIA_DEBUG` gate on SFMLImGuiBackend need to be lifted, or is the boot menu debug-only too?
- How does this interact with the planned bgfx backend swap? Should we wait or proceed with SFML+OpenGL ImGui now?
