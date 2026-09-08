# Research: Ideate — Bootstrap UI to ImGui

**Input:** docs/research/bootstra_imgui_ui/explore.md

## Candidates

### Candidate 1: Boot-Only ImGui Menu (MainPU Render Pass)

**Home module/system:** New `BootMenuModule` in CluicheTest (replaces `BootUIPageModule`)
**Size:** S (≤1 week)
**Description:** Replace `BootUIPageModule` + `LaunchUIPage` with a new `BootMenuModule` that renders an ImGui window directly on MainPU during the Boot stage. Boot stage gets its own simplified render pass (clear + ImGui draw) that bypasses RenderPU entirely — since Boot has no game rendering, RenderPU can idle or not start until a game stage is entered. UIModule (Ultralight) is made stage-scoped so it only initializes when transitioning to DummyStage/UI Stage.

The menu reads `Application::GetStageTransitions()` for the stage list and queries `TestResultsRegistry` for per-stage status. A "Launch" button calls `TransitionTo(stageCRC)`. ImGui renders via `SFMLImGuiBackend` on MainPU which owns the SFML window during Boot.

**Primary value:** Simplest possible replacement — Boot stage becomes a lightweight C++-only menu with zero web dependencies, and the change is fully scoped to Boot stage modules.

---

### Candidate 2: Global ImGui Layer (All Stages, Boot Gets Menu)

**Home module/system:** Expand `DiaImGui` module; new `ImGuiRenderModule` on RenderPU (global); `BootMenuModule` in CluicheTest
**Size:** M (1–3 weeks)
**Description:** Make DiaImGui a first-class global rendering layer. Add an `ImGuiRenderModule` to RenderPU that owns the ImGui frame lifecycle (NewFrame/Render) every frame across all stages. Modules on other PUs submit ImGui draw commands via a thread-safe command buffer that RenderPU consumes each frame.

Boot stage uses this to render the stage menu. Other stages get ImGui debug overlays for free (solving the TestStageHUDModule cross-thread issue). UIModule (Ultralight) becomes stage-scoped.

**Primary value:** Solves the ImGui thread-safety problem globally — unlocks debug overlays in all stages, not just Boot. The boot menu is one consumer of a general capability.

---

### Candidate 3: Hybrid — ImGui Boot + Ultralight Stays Global

**Home module/system:** New `BootMenuModule` in CluicheTest; UIModule stays as-is
**Size:** S (≤1 week)
**Description:** Same as Candidate 1, but UIModule (Ultralight) remains global and initializes on every stage including Boot — it just isn't used during Boot. The `BootUIPageModule` is removed and replaced with `BootMenuModule` rendering ImGui. Ultralight is still available for DummyStage without any module-scoping refactoring.

This is the minimal-change version: only remove the HTML page and its binding module, add an ImGui menu module, leave everything else untouched.

**Primary value:** Minimum blast radius — no changes to UIModule scoping or the Ultralight lifecycle. Fast to implement, easy to revert.

---

### Candidate 4: ImGui Boot + Stage-Scoped UIModule + ImGui Debug Opt-In

**Home module/system:** `BootMenuModule` in CluicheTest; `UIModule` refactored to stage-scoped; `DiaImGui` expanded with opt-in per-stage debug overlay
**Size:** M (1–3 weeks)
**Description:** Three-part change: (1) Replace boot HTML with ImGui menu on MainPU, (2) Make UIModule stage-scoped so Ultralight only initializes for stages that declare UI needs in their `.diastage` config, (3) Add an optional `ImGuiDebugModule` that stages can include for debug overlays (renders on RenderPU with a thread-safe submission path).

The boot menu is the immediate deliverable. Stage-scoped UIModule cleans up the architecture (Boot stage no longer pays for Ultralight init). The debug overlay opt-in is a bonus that replaces the disabled TestStageHUDModule.

**Primary value:** Clean architecture — each stage declares what UI systems it needs. Boot gets ImGui only, DummyStage gets Ultralight only, future stages can mix.

---

### Candidate 5: Command-Line + ImGui Fallback

**Home module/system:** `BootMenuModule` in CluicheTest; enhanced CLI args in Main.cpp
**Size:** S (≤1 week)
**Description:** Add command-line argument `--stage <name>` that skips Boot entirely and transitions directly to the named stage (for automation and quick iteration). When no `--stage` is provided, show the ImGui boot menu. This makes the ImGui menu optional — automation never sees it, developers use it for interactive sessions.

The ImGui menu is identical to Candidate 1 (MainPU render during Boot), but the CLI shortcut adds automation convenience beyond the existing WebSocket path.

**Primary value:** Developers can skip the menu entirely for rapid iteration; automation has a simpler code path than WebSocket for single-stage runs.

---

### Candidate 6: Data-Driven ImGui Menu from .diagame

**Home module/system:** `BootMenuModule` in CluicheTest; menu layout driven by manifest metadata
**Size:** M (1–3 weeks)
**Description:** The ImGui boot menu is generated entirely from `.diagame` manifest data. Each `.diastage` import gets optional metadata (category, description, expected_duration, test_type) that the menu uses to group and present stages. The menu auto-discovers stages without hardcoded knowledge.

Status tracking is enriched: beyond pass/fail, the menu shows "loaded this session" (via a session-scoped bitfield), "last checkpoint reached", and "duration" pulled from `TestResultsRegistry` and `ObservationModule` data.

**Primary value:** As new stages are added to the `.diagame`, the boot menu picks them up automatically with rich categorization — zero code changes needed for new stages.

---

### Candidate 7: ImGui Boot Menu + Test Runner Controls

**Home module/system:** `BootMenuModule` in CluicheTest; extended TestResultsRegistry
**Size:** M (1–3 weeks)
**Description:** Beyond stage selection, the ImGui menu becomes a lightweight test runner UI. Features: "Run All" button that sequentially loads each stage, runs its test, records results, and returns to Boot. Per-stage controls: "Run", "Skip", "Re-run Failed". Status shows pass/fail/skipped/running with timing. Export results button writes a JSON report.

This positions the Boot menu as a developer-friendly alternative to the E2E automation system for quick validation runs.

**Primary value:** Developers get one-click "run all stages" without needing the E2E Python harness — useful for quick smoke testing during development.

---

### Candidate 8: Fullscreen ImGui Dashboard

**Home module/system:** `BootMenuModule` in CluicheTest; uses ImGui docking branch features
**Size:** L (1–2 months)
**Description:** The Boot stage becomes a full ImGui dashboard with multiple panels: stage list (with filters/search), test results history (graphs over multiple runs), observation/metrics summary from last session, and a log viewer pulling from DiaObservation output. Uses ImGui's docking branch for resizable panels.

This is the "Boot as a developer cockpit" approach — not just a launcher but a project health dashboard.

**Primary value:** Maximum developer insight at a glance — see engine health, test trends, and observation data before deciding which stage to investigate.

## Coverage Map

The candidates span the design axes from explore.md:

- **Thread ownership**: C1/C3/C5/C6 use MainPU-only (Boot has no RenderPU work). C2/C4 solve the global cross-thread problem.
- **Menu complexity**: C1/C3 are minimal lists. C5 adds CLI bypass. C6 adds manifest-driven categories. C7 adds test runner. C8 is a full dashboard.
- **Ultralight coexistence**: C3 keeps it global (no refactor). C1/C4/C5/C6/C7 scope it to non-Boot stages.
- **Scope**: S (C1, C3, C5) → M (C2, C4, C6, C7) → L (C8). Range from 3-day swap to multi-week platform work.
- **Automation**: All maintain WebSocket automation. C5 adds CLI shortcut. C7 adds in-app test running.
