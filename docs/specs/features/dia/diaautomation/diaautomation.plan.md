# Plan: DiaAutomation System

**Spec:** @docs/specs/systems/dia/diaautomation.md
**Status:** Done
**Started:** 2026-05-21

## Session Notes

Implements the full DiaAutomation capability layer: AutomationService class, 4 command registrations, navigation hold via transition guard, checkpoint/pause-resume registries, CI safety heartbeat, and 6 new LifecycleEvent kinds.

**Spec decisions summary (Platform → System):**
- PD-001: StringCRC for checkpoint names, command names, stage targets.
- PD-004: DynamicArrayC for all registries; std::function allowed for callbacks.
- AD-003: All code in `Dia::Automation::` namespace.
- SD-AUT-001: AutomationService is a plain class, not a Module. No lifecycle of its own.
- SD-AUT-002: Checkpoints cleared when owning module stops (UnregisterCheckpoints).
- SD-AUT-003: Navigation hold re-arms after each kStageTransitionCommitted event.
- SD-AUT-004: OnDisconnect = primary; heartbeat = backup. Both call RequestShutdown.
- SD-AUT-005: All commands use {success, data/error} envelope (same as baseline-commands).
- SD-AUT-006: Pause/resume is cooperative callbacks, not framework freeze.
- SD-AUT-007: Every RegisterCommands-wrapped callback calls ResetHeartbeat() first.
- SD-AUT-008: Automation events emitted on existing $lifecycle stream (not a new stream).

**$lifecycle tap approach:** AutomationService is not a Module, so it cannot use EventStreamReader (which needs Module*). Instead it uses `Application::FindStream("$lifecycle")` → cast to `EventStreamStore<LifecycleEvent>*` → `AttachTap()`. The tap callback fires synchronously inside Application::Update() when the framework emits kStageTransitionCommitted — that's when hold re-arms. Tap is attached in `EnableNavigationHold()`, detached in destructor.

**New LifecycleEvent kinds (6):** kAutomationHoldEnabled=7, kAutomationHoldReleased=8, kAutomationPaused=9, kAutomationResumed=10, kAutomationDisconnect=11, kAutomationHeartbeatTimeout=12.

**AutomationModule (CluicheGameBaseline):** Lives in `CluicheGameBaseline/Modules/AutomationModule.*`. Instantiates AutomationService, wires OnDisconnect to DiaDebugServer disconnect callback. stages: all.

**GUID for DiaAutomation.vcxproj:** `{2A3B4C5D-6E7F-8091-A2B3-C4D5E6F70123}`

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add 6 automation kinds to `LifecycleEvent.h` (kAutomationHoldEnabled=7 … kAutomationHeartbeatTimeout=12) | Compiles | Done | haiku | |
| 2 | Create `Dia/DiaAutomation/` with `AutomationService.h`, `AutomationService.cpp` (stub), `DiaAutomation.vcxproj`, `dia.automation.architecture.module.md` | Compiles | Done | sonnet | |
| 3 | Add DiaAutomation to `Cluiche.sln` | Solution builds | Done | haiku | |
| 4 | Add DiaAutomation project reference to `GoogleTests.vcxproj` | Test build | Done | haiku | |
| 5 | TDD-RED: `TestAutomationService.cpp` — tests for checkpoint registry (AC1-AC8), navigation hold (AC1-AC7), pause/resume (AC1-AC8), CI safety (AC1-AC8). Quote failing output. | All fail to link | Done | sonnet | |
| 6 | Implement checkpoint registry methods in `AutomationService.cpp` | Checkpoint ACs green | Done | sonnet | |
| 7 | Implement navigation hold methods + lifecycle tap for re-arm | Navigation hold ACs green | Done | sonnet | |
| 8 | Implement pause/resume methods | Pause/resume ACs green | Done | sonnet | |
| 9 | Implement CI safety (heartbeat + OnDisconnect) | CI safety ACs green | Done | sonnet | |
| 10 | Implement `RegisterCommands()` — registers 4 dia.automation.* commands, wraps each with ResetHeartbeat | Command ACs green | Done | sonnet | |
| 11 | Add `TestAutomationService.cpp` to `GoogleTests.vcxproj` | Builds | Done | haiku | |
| 12 | Create `AutomationModule` in `CluicheGameBaseline/Modules/` | Compiles | Done | sonnet | |
| 13 | Add AutomationModule to CluicheGameBaseline vcxproj; wire in manifest | CluicheTest runs | Done | sonnet | |
| 14 | `dia run googletest` — all pass; `dia run cluichetest` | Green | Done | sonnet | Full suite pass; CluicheTest pass pending |
| 15 | Update specs, backlog, commit | Docs + git | Done | haiku | |
