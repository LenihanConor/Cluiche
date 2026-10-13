# Implementation Plan: F-1 Harness (system)

**Spec:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Status:** In Progress
**Sign-off (AD-001):** granted 2026-10-13 by the user for every task marked **sign-off** in this plan (task 13 included).

System plan: tasks are the feature specs and the cross-cutting tooling work. Each feature has its own plan with the detailed tasks.

## Rules for every task

- Each task is a separate subagent with all context inlined (dispatch skill). Two-stage check after DONE: spec compliance, then code quality.
- Commit after each task, with the plan row updated in the same commit (`dia docs plan`).
- Fresh quoted run output for every code task (verify skill). New public API and behaviour follow TDD (RED quoted before GREEN).
- Build and run only through `dia run` / `dia launch`. Report pass/fail, failing tests and errors only.
- All edits outside `Cluiche/CoW/**`, `Cluiche/Assets/CoW/**` and `docs/specs/applications/cow/**` were signed off in the spec's Change Sign-off table (AD-001). Anything not in that table needs a new sign-off before it is edited.

## Phases

| Phase | Work | Spec / plan | Depends on |
|-------|------|-------------|------------|
| A | BootMenuModule promotion + `StageStatusModule` | bootmenu-promotion.md / .plan.md (7 tasks) | none |
| B | CoW app skeleton (boots to the Boot menu) | cow-app-skeleton.md / .plan.md | A for the menu; B1 and B2 can start immediately |
| C | Boot choice: Play entry only | boot-choice.md (1 task, folded into B6) | B |
| D | Tooling: `scaffold stage --app`, `new-cow-stage` skill, `dia scaffold app` (spec first) | see tasks below | B (the hand-built skeleton is the reference) |
| E | DiaMessageBus module doc status fix | below | none |
| F | CoW debug host wiring (manifest + panel copy; no new Dia code) | tasks 13 to 15 | B |
| Deferred | Bus live verification, bus traffic recorder | features TBD | B |

## Tasks

| # | Task | Phase | Status | Model | Notes |
|---|------|-------|--------|-------|-------|
| 1 | Execute the promotion plan (its 7 tasks) | A | Done | sonnet | Sign-off: baseline + CluicheTest (approved); Done |
| 2 | Execute skeleton tasks B1 and B2 (DiaCLI `cow` target, `sln-add` path option) | B | Done | sonnet | DiaCLI only; independent of A, can run in parallel with task 1 (different files); Done |
| 3 | Execute skeleton tasks B3 to B5 (project and solution, manifests and `FrontEnd` stub, `Main.cpp`) | B | Done | sonnet | Needs task 1 complete (menu in baseline) and task 2; Done |
| 4 | Execute skeleton task B6 (verify end to end, including the boot-choice check and Visual Studio startup) | B, C | Done | haiku | Visual Studio F5 part is done by the user; Done |
| 5 | Mark boot-choice spec Done; update system spec feature rows | C | Done | haiku | `dia docs spec-done`; Done |
| 6 | **RED/GREEN:** `dia scaffold stage --app` (parameterise CluicheTest-specific names, paths and the 9 touch points) with tests | D | Done | sonnet | Uses `FrontEnd` as the reference. Existing CluicheTest scaffold output must be unchanged; 22 new tests, 148 scaffold tests pass; Probe stage generated into CoW: pipeline 3/3 pass, Boot menu listed 2 stages (then reverted) |
| 7 | Implement the `new-cow-stage` skill per [new-cow-stage-skill.md](new-cow-stage-skill.md) | D | Done | haiku | After task 6. Spec Approved; trial run needs a locked testbed design; Skill + command written (AC1-3). Trial (spec task 2, AC4/AC5) waits for a locked testbed design |
| 8 | `dia scaffold app` feature spec: [scaffold-app.md](../../../dia/systems/diacli/scaffold-app.md) | D | Spec Approved | opus | Written by interview. Includes the data-driven target registry (signed off) |
| 9 | Implement `dia scaffold app` per its spec (its 5 tasks); prove by generating `ScratchApp` outside the committed tree and booting it | D | Done | sonnet | Needs task 8 approved and the registry refactor signed off. Its task 1 may run before the skeleton and make skeleton B1 toml-only. Do not commit the throwaway app; Done |
| 10 | Verify the DiaMessageBus tests pass, then correct `status: stub` in `dia.messagebus.architecture.module.md` | E | Not Started | haiku | Doc only; confirm with a fresh `dia run googletest --filter="*MessageBus*"` first |
| 11 | Observation opportunity scan across all touched files; log suggestions in the notes | A to D | Not Started | haiku | Does not block Done |
| 12 | Mark the F-1 system spec Done only when all child features are Approved or Done; record the deferred features | all | Not Started | haiku | System spec rule |
| 13 | CoW debug host wiring. In `Assets/CoW/cow_main.diaapp` add, all with stages `["all"]`: `Camera2DModule` (no deps; `VisualDebuggerModule` holds a `ModuleRef` to it), `UIModule` (deps `ObservationModule`, `KernelModule`; channels `SimToMain` reads, `UIToSim` writes), `VisualDebuggerModule` (dep `Camera2DModule`; channels `SimDebug` writes, `DebugLayerManager` provides, `DomainRegistry` writes, `DebugPanelCommand` reads, `DebugPanelToggle` writes, `DebugPanelSetVisibility` writes), `DebugPanelPageModule` (deps `ObservationModule`, `UIModule`; `DomainRegistry` reads, `DebugPanelCommand` writes, `DebugPanelToggle` reads, `DebugPanelSetVisibility` reads), `AssetRuntimeVisualDebuggerModule` on RenderPU (dep `DebugUI`; consumes `DebugLayerManager` and `AssetRuntimeService`). In `cow.diagame` add path alias `ui_common` = `./global/Presentation/UI` (the panel loads `ui_common/debug-panel.html`; CoW only defines `root` today). Copy `debug-panel.html` to `Assets/CoW/Global/Presentation/UI/`, add catalogue entry `ui.debug_panel_page` (type `ui`, scope `global`) and make `build-assets` deploy it. | `dia validate manifest`; `dia pipeline --target cow`; `dia run cow`: `~` opens the panel, `debug.layer.list` runs, panel auto-opens on start | Done | sonnet | **sign-off** (baseline/asset rules). Needs no new Dia code, can start now. **Deviation found at runtime:** `DebugServerHostModule` (Inspector, port 9002) was left out. A ServiceStream provider needs a consumer (`SERVICE_STREAM_ORPHAN_PROVIDER`), and every `DebugServerService` consumer is an inspector for a system CoW does not have yet; `dia validate manifest` does not run this check, `dia launch` does. Add it back with the first CoW inspector module. `AssetRuntimeVisualDebuggerModule` was added to consume `DebugLayerManager` (and gives an Asset Runtime domain in the panel). Channel lists are from CluicheTest's manifest; `UIModule` reads `SimToMain`, so check the validator for a missing writer and resolve it rather than dropping the channel. Copy from CluicheTest's current `debug-panel.html` (Webix removal is another agent's work and does not touch this file); Verified: manifest validates, pipeline cow 3/3 pass, CoW boots with VisualDebugger/UI/DebugPanelPage/AssetRuntimeVisualDebugger Active, panel auto-opens, same warnings as baseline. Not yet eyeballed by user (~ panel, debug.layer.list). Inspector host deferred. |
| 14 | CoW message bus in the manifest: `MessageBusModule` and one message type so the bus domain has something to show | `dia run cow`: bus domain card appears in the panel | Done | sonnet | **Coordinate with the bus-traffic-recorder agent**: it may already add this. Check `git status`/`cow_main.diaapp` before starting; the `bus_traffic.*` commands the old console spec cited do not exist yet and arrive with the recorder wiring; Needed new code, not just manifest edits (user sign-off 2026-10-13): Dia MessageBusModule gained an instanceId ctor (+2 tests); new baseline MessageBusHostModule (DIA_MODULE, registers MessageBusDebugDomain with VisualDebuggerModule); new CoW BusProbeModule posts/receives one BusProbeEvent per tick. Manifest: MessageBusHostModule + BusProbeModule (stages all). Verified: DiaMessageBus tests pass, pipeline cow 3/3, CoW boots with both modules Active and stops cleanly. Bus card in the ~ panel confirmed by user. Bus recorder CoW wiring (Test 6) still open; Recorder wired 2026-10-13: CoW BusRecorderModule (Debug only; FileBusRecorder to cow_bus_traffic.dbtr in the working dir, tick = SimPU tick, flush every 30 ticks, closed on stop). Test 6 verified: after the process exited the file held 110 records, version 1, no truncation, sequence contiguous 0..109, ticks non-decreasing |
| 15 | Re-sync CoW's `debug-panel.html` copy after panel-console task 8 and panel-palette task 4 land; add a note to dia-needs/backlog debt if the two copies have diverged | `dia run cow`: console and Ctrl+P work | Done | haiku | Blocked by the two Dia plans; decision 2026-10-13 was copy, not share; Copied CluicheTest debug-panel.html (console + Ctrl+P palette) to Assets/CoW; md5 identical to source and to deployed file; pipeline cow 3/3; CoW boots, DebugPanelPageModule Active, ConsoleLogRing registered. Copies will drift again: re-sync on any panel change. Panel not yet eyeballed in CoW by user |

## Order and parallelism

```
Task 1 (promotion) ──────────────┐
Task 2 (DiaCLI target, sln-add) ─┴─> Task 3 (skeleton) -> Task 4 (verify) -> Task 5
                                               │
                                               └─> Task 6 -> Task 7
                                               └─> Task 8 -> Task 9
Task 10 any time
Task 11, 12 last
```

Tasks 1 and 2 touch different files (baseline and CluicheTest versus DiaCLI and `pipeline.toml`) and may run in parallel. Everything else is sequential.

## Risks

- Module `config` may not reach `BootMenuModule` (promotion plan, task 4). If not, use a default and tell the user; do not change Dia.
- Trimming `cluiche_main.diaapp` and `assets.catalogue.json` for CoW is trial and error. Each trim is validated with `dia validate manifest` and a launch before the next.
- `dia scaffold app` is only as good as the hand-built skeleton, so it waits for it.
- The user does the Visual Studio F5 check.

## Deferred (not in this plan)

Bus live verification, the bus traffic recorder (Dia or CoW-Shared still to decide), the debug command host, and shipping gating (dropped).
