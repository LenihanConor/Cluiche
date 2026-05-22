# Plan: Baseline Commands

**Spec:** @docs/specs/features/dia/diaapplicationflow/baseline-commands.md
**Status:** Done
**Started:** 2026-05-21
**Depends on:** transition-guards (heldByGuards field in TransitionInfo)

## Session Notes

Adds DiaAPI JSON callback path and two baseline app commands: `dia.app.quit` and `dia.app.report`.

**Spec decisions summary (Platform → System):**
- PD-001: Command names as StringCRC. `dia.app.quit`, `dia.app.report`, category `dia.app` all StringCRC.
- PD-004: `CommandInfoJson` uses StringCRC + raw const char*, no STL containers. `CommandCallbackJson` is std::function (same pattern as existing CommandCallback).
- SD-API-001: Commands identified by StringCRC.
- SD-API-005: JSON commands stored in same global registry state.
- Name validation relaxed to `[a-z0-9._-]+` — dotted namespace grammar.
- JSON dispatch path is parallel to CLI dispatch, not a replacement.
- Consistent envelope: `{"success": true, "data": {...}}` / `{"success": false, "error": "..."}`.
- Commands registered in `Application::Start()` before any module's `DoStart` — DiaDebugServer cannot be connected yet, no race.
- `dia.app.report` uses `IApplicationInspectable` interface only (SD-016).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Relax `ValidateCommandName` to `[a-z0-9._-]+`. Update existing validation test. | Dotted names pass; existing tests pass | Done | haiku | |
| 2 | TDD-RED: write `TestCommandRegistryJson.cpp` covering AC1–AC4, AC11. Quote failing output. | Fail to compile | Done | sonnet | TDD red gate |
| 3 | Add `CommandCallbackJson`, `CommandInfoJson`, `RegisterCommandJson`, `ExecuteCommandJson` to `CommandRegistry.h` + implement in `.cpp`. | AC1-AC4, AC11 green | Done | sonnet | |
| 4 | Update `CommandDispatcher::ExecuteDiaAPICommand` to route via `ExecuteCommandJson` first. | AC4 end-to-end | Done | sonnet | |
| 5 | Add DiaAPI project reference to `DiaApplicationFlow.vcxproj`. | Compiles | Done | haiku | |
| 6 | TDD-RED: write `TestBaselineCommands.cpp` covering AC5–AC10. Quote failing output. | Fail — commands not registered | Done | sonnet | TDD red gate |
| 7 | Implement `Application::RegisterBaselineCommands()`; call from `Start()`. Add `ModuleStateToString` helper. | AC5-AC10 green | Done | sonnet | |
| 8 | Add both test files to `GoogleTests.vcxproj`. | Builds | Done | haiku | |
| 9 | Run `dia run googletest`; confirm all pass. Run `dia run cluichetest`. Quote output. | AC12 verification gate | Done | sonnet | All 6 AC5-AC10 pass; cluichetest passes. Root bug: Shutdown() not clearing jsonCommands when not initialized → dead lambdas across tests. Fixed: always call Shutdown() unconditionally in SetUp/TearDown. |
| 10 | Add feature row to `diaapplicationflow.md`. Update module doc. Commit. | Doc + git | Todo | haiku | |
