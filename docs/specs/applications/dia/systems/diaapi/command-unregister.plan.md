# Implementation Plan: Command Unregister

**Spec:** @docs/specs/applications/dia/systems/diaapi/command-unregister.md
**Status:** Not Started
**Sign-off (AD-001):** granted 2026-10-13 by the user for every task marked **sign-off** in this plan.

Prerequisite for panel-console and panel-palette. Do not start until the spec is `Approved`.

## Rules for every task

- One subagent per task with all context inlined (dispatch skill). Two-stage check after DONE: spec compliance, then code quality.
- Commit after each task with the plan row updated in the same commit (`dia docs plan`).
- Fresh quoted `dia run googletest` output for every code task (verify skill). New behaviour is TDD: quote RED before GREEN.
- Edits to Dia need explicit sign-off per change (AD-001 in the f1-harness spec). Tasks marked **sign-off** wait for it.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Survey: list every caller of `GetCommand`, `ListCommands`, `GetCommandsByCategory` (incl. `HelpSystem`) and every `RegisterCommand*` site with the lifetime of its owner | none (read only) | Done | haiku | Done during planning 2026-10-13. Results are in the spec's Adoption table: all callers use `auto`; `CommandDispatcher` (DebugServer thread) holds a raw pointer across the call; stage-scoped registrants: `Geometry2DTestStageModule` (proven), `EntityInspectorModule` (check stages) |
| 2 | **RED/GREEN:** `UnregisterCommand` / `UnregisterCommandJson`, including the pending list; re-register after unregister | AC-1 to AC-5 (`TestCommandRegistry.cpp`) | Done | sonnet | **sign-off**; extend the existing `CommandRegistryTest` fixture; 7 tests added (AC-1..5); CommandRegistry* 42 passed |
| 3 | **RED/GREEN:** thread safety: `std::shared_mutex` in the registry, shared lock held across the callback, `thread_local` in-callback guard that asserts on register/unregister | AC-7 to AC-9 | Done | opus | **sign-off**; concurrency design, AC-8 needs a callback that blocks on an event; shared_mutex + thread_local callback depth (nested execute reuses lock, mutations rejected/assert); 4 tests added; full googletest 9070 passed; ASan build fails to link (DiaProtobuf/DiaBgfx3D annotate_string mismatch, pre-existing) |
| 4 | **RED/GREEN:** `CommandList` alias (cap 256), `CopyCommandInfo`, `ListCommandInfos` (value copies for cross-thread callers); migrate `DebugServer/CommandDispatcher.cpp` off the raw `GetCommand` pointer | AC-6 | Done | sonnet | **sign-off** (Dia + DiaDebugServer); resolves Open Q 3 (cap value); CommandList cap 256, CopyCommandInfo/ListCommandInfos, dispatcher+list_commands migrated; 102 tests pass |
| 5 | Adopt per the spec's Adoption table: `Geometry2DTestStageModule` (must), `EntityInspectorModule` (if not stage `all`), `AssetRuntimeDebugCommands` and `AutomationService` (check lifetime, tidy only) | existing suites + `dia run cluichetest`: enter Geometry2DTestStage, back to Boot, enter again, command still works (no duplicate-registration error in the log) | Done | sonnet | **sign-off** (CluicheTest + baseline + Dia); skip `Application` and `DebugLayerManager` (process lifetime); Unregister on stop added: Geometry2DTestStageModule (2 cmds), EntityInspectorModule (2 cmds, stage-scoped to EntityTestStage). Left: AssetRuntimeDebugCommands (test-only callers), AutomationService (process lifetime), Application/DebugLayerManager. Verified: gtest 102 pass, cluichetest pipeline pass, boot smoke no duplicates. NOT verified: manual stage re-entry check pending (E2E geometry2d_stage/smoke.py blocked: 'dia launch --automation' fails 'Failed to compose manifest').; Compile + CluicheTest/CoW boot verified by agent and coordinator. Stage re-entry (Boot, Geometry2D, Boot, Geometry2D) checked manually by the user 2026-10-13: works, no duplicate-registration problem |
| 6 | Run `CommandRegistry*` suite and the sanitizer build | AC-7, AC-10 | Done | haiku | `dia run googletest --filter="CommandRegistry*"`, then `dia check sanitizer`; report pass/fail only; CommandRegistry* suite passes (46+). Whole googletest suite run under ASan 2026-10-13: 9183 passed, 0 AddressSanitizer findings; CommandRegistryTest.ConcurrentRegisterUnregisterExecuteList (AC-7 stress) and UnregisterWaitsForInFlightCallback pass under ASan. ASan does not detect data races (no TSan on MSVC) |
| 7 | Observation opportunity scan; `dia docs spec-done` | none | In Progress | haiku | Suggestions only, not blocking; Observation scan 2026-10-13 (suggestions only). LOGS: Unregister* and Register* success paths are silent (only errors log); add DIA_LOG_DEBUG on add/remove so stage re-entry is visible in logs. TRACES/PROFILING: none; the shared lock is held for the whole command callback, so a slow command stalls every Register/Unregister: add a profile scope or a warning when a callback holds the lock longer than ~100 ms (spec Open Q 1). METRICS: command count gauge, unregister count, duplicate-registration rejections, lock hold time. HEALTH: none; a duplicate-registration rejection is the failure mode that signalled the original leak. Task still open for spec-done only |

## Order and parallelism

1 is done. 2, 3, 4 touch the same two files, so run them in sequence. 5 after 2. 6 after 2 to 5.

## Risks

- Holding the shared lock across callbacks (Open Q 1) can stall registration behind a slow command. Debug commands are short today; revisit if any takes more than ~100 ms.
- Task 3 is the only task where a wrong design is hard to see from tests. Review it with opus before merging, and run AC-7 under ASan/TSan.
