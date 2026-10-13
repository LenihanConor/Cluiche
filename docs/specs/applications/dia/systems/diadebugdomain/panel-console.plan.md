# Implementation Plan: Panel Console

**Spec:** @docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
**Status:** Not Started
**Sign-off (AD-001):** granted 2026-10-13 by the user for every task marked **sign-off** in this plan.

Needs `command-unregister.plan.md` tasks 2 to 4 done first (thread-safe registry, `ListCommandInfos`). Do not start until the spec is `Approved`.

## Rules for every task

- One subagent per task with all context inlined (dispatch skill). Two-stage check after DONE: spec compliance, then code quality.
- Commit after each task with the plan row updated in the same commit (`dia docs plan`).
- Fresh quoted run output for every code task (verify skill). New public API is TDD: quote RED before GREEN.
- Logic goes in `Dia/DiaVisualDebugger/Console/` and is unit-tested without Ultralight. Anything touching `CluicheGameBaseline` or CluicheTest assets needs sign-off (AD-001) and is marked **sign-off**.
- New Dia files are added with `dia docs vcxproj-add DiaVisualDebugger "Console\\<File>" --filter Console`; test files go in `GoogleTests/DiaVisualDebugger/Console/`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Update `mockups/debug-console.html`: command strip inside the panel with result line, history hint, autocomplete list, log view with level filter | visual acceptance by the user | Done | sonnet | Mockup gate: no UI work (task 8) before the user approves it; Mockup approved by the user 2026-10-13; JS syntax-checked with esbuild (not browser-tested by the agent); Mockup approved by the user 2026-10-13; JS syntax-checked with esbuild (not browser-tested by the agent) |
| 2 | **RED/GREEN:** `CommandLineParser` (tokenize, quotes, `ArgumentParser`) and `ConsoleLineAssembler` (join up to 4 chunks, limit 240) | AC-1, AC-2, AC-16 (assembler) | Done | sonnet | Pure logic; `storage` buffer owns the token text; 28 Console* tests pass (19 parser, 9 assembler); added ParseError::InvalidArgument; bare key=val rewritten to --key=val |
| 3 | **RED/GREEN:** `ConsoleExecutor` (parse + `Dia::API::ExecuteCommand` + typed result) and `ConsoleHistory` | AC-3, AC-4 | Not Started | sonnet | Register throwaway test commands in the fixture |
| 4 | **RED/GREEN:** `FuzzyMatcher` and `CommandCompleter` over `ListCommandInfos` | AC-5, AC-6 | Not Started | sonnet | `FuzzyMatcher` is reused by the palette; keep its API generic (strings in, ranked indices out) |
| 5 | **RED/GREEN:** `ConsoleLogRing` as a `Logger` sink: 500 lines, mutex, `DrainSince(cursor)` | AC-7, AC-8 | Not Started | opus | Open Q 2 is resolved (see spec): sink runs under the Logger's mutex, must not log, `kMaxSinks` is 8 and registration fails silently. Add a probe-line check for the silent failure; run under `dia check sanitizer` |
| 6 | Event types `DebugConsoleLineEvent` (Main to Sim) and `DebugConsoleResultEvent` (Sim to Main); `VisualDebuggerModule` drains lines and writes results; `ExecuteConsoleCommand` calls `ConsoleExecutor`; add the two channels to `cluiche_main.diaapp`: `DebugConsoleLine` (`DebugPanelPageModule` writes, `VisualDebuggerModule` reads) and `DebugConsoleResult` (`VisualDebuggerModule` writes, `DebugPanelPageModule` reads), mirroring the existing `DebugPanelCommand` entries | AC-15; `dia run cluichetest` boots | Not Started | sonnet | **sign-off** (baseline + CluicheTest manifest); remove the old inline parsing |
| 7 | `DebugPanelPageModule`/`DebugPanelPage`: bindings `onConsoleLine`, `consoleComplete`, `consoleHistory`; owns history, completer and log ring; pushes `consoleResult` and `appendLogLines` each frame | `dia run cluichetest` boots, no regression in domain cards | Not Started | sonnet | **sign-off** (baseline); register/unregister the ring in `DoStart`/`DoStop` |
| 8 | `debug-panel.html`: strip, result line, history, autocomplete, log view, 240-char input, matching the approved mockup | AC-9 to AC-14 | Not Started | sonnet | **sign-off** (CluicheTest asset); user does the manual checks; remove `MAX_CMD_TEXT = 52` |
| 9 | Full verification: googletest `*Console*`, sanitizer, `dia run cluichetest` and walk AC-9 to AC-12 | all | Not Started | haiku | Report pass/fail and failing names only |
| 10 | Observation opportunity scan; `dia docs spec-done` | none | Not Started | haiku | Suggestions only. CoW copy of `debug-panel.html` is re-synced by f1-harness plan task 15 |

## Order and parallelism

1 can run any time (and in parallel with 2 to 5). 2, 3, 4, 5 touch different files and can run in parallel after the registry work lands (4 needs `ListCommandInfos`). 6 needs 3. 7 needs 4, 5, 6. 8 needs 1 (approved) and 7. 9 and 10 last.

## Risks

- The JS bridge's 63-character string limit is the reason for `onConsoleLine`; if Ultralight's limit differs from the comment in `DebugPanelPage.h`, task 7 should measure it first.
- Task 6 changes the manifest that CluicheTest boots from. Verify `dia run cluichetest` reaches the Boot menu before moving on.
- Two copies of `debug-panel.html` will exist until f1-harness task 15; do not let CoW work edit its copy in the meantime.
