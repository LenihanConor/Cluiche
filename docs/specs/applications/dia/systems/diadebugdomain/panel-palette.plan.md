# Implementation Plan: Panel Palette

**Spec:** @docs/specs/applications/dia/systems/diadebugdomain/panel-palette.md
**Status:** Not Started
**Sign-off (AD-001):** granted 2026-10-13 by the user for every task marked **sign-off** in this plan.

Needs `panel-console.plan.md` tasks 4 (`FuzzyMatcher`) and 8 (console strip) done first. Do not start until the spec is `Approved`.

## Rules for every task

- One subagent per task with all context inlined (dispatch skill). Two-stage check after DONE: spec compliance, then code quality.
- Commit after each task with the plan row updated in the same commit (`dia docs plan`).
- Fresh quoted run output for every code task (verify skill). New public API is TDD: quote RED before GREEN.
- Tasks that touch `CluicheGameBaseline` or CluicheTest assets need sign-off (AD-001) and are marked **sign-off**.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Update `mockups/debug-palette.html`: Ctrl+P hint, Commands and Domains tabs only, domain reveal affordance | visual acceptance by the user | Done | haiku | Mockup gate before task 4; Mockup approved by the user 2026-10-13; JS syntax-checked with esbuild (not browser-tested by the agent); Mockup approved by the user 2026-10-13; JS syntax-checked with esbuild (not browser-tested by the agent) |
| 2 | **RED/GREEN:** `PaletteIndex` (`Build`, `Search`, type filter, 50-result cap) and `ResolvePaletteAction` | AC-1 to AC-7 | Done | sonnet | Uses `FuzzyMatcher` from panel-console; fixture registers test commands and a stub `IDebugDomain`; PaletteIndex + ResolvePaletteAction; 7 new tests pass |
| 3 | `DebugPanelPageModule`/`DebugPanelPage`: own the index, bindings `onPaletteOpen`/`onPaletteSearch`, push `paletteResults` | `dia run cluichetest` boots | Done | sonnet | **sign-off** (baseline); BuildPaletteResults + onPaletteOpen/onPaletteSearch bindings; gtests green, both pipelines + boot smoke OK; JS side unverified |
| 4 | `debug-panel.html`: overlay, Ctrl+P/Esc/arrows/Enter, tabs, fill-console and reveal-domain actions | AC-8 to AC-11, AC-13 | Done | sonnet | **sign-off** (CluicheTest asset); user does the manual checks; Headless Chrome 46/46 checks (console harness 34/34), not run in Ultralight; user visual + Ctrl+P check pending |
| 5 | Check Ctrl+P is delivered to the page by Ultralight (Open Q 2); switch to Ctrl+K in the spec and code if not | manual in `dia run cluichetest` | Done | haiku | Do this first inside task 4 if cheap; separate row so the result is recorded; Needs the real app: user presses Ctrl+P in the panel in dia run cluichetest; fall back to Ctrl+K if the host or Ultralight swallows it; User confirmed 2026-10-13 in the real app: Ctrl+P works (opens the palette; no crash) after the deferral fix dfbb24a3. Ctrl+P is delivered to the page, so no fallback to Ctrl+K is needed |
| 6 | Full verification: googletest `*Palette*`, `dia run cluichetest` walk of AC-8 to AC-11 | all | In Progress | haiku | Report pass/fail only; Automated part done 2026-10-13: googletest 9165 passed; headless-Chrome palette harness 46/46; reveal-key bug found and fixed in review (435f2b92). STILL OPEN: user walk of AC-8 to AC-11 in Ultralight incl. Ctrl+P delivery (task 5); Automated: googletest, headless Chrome 46/46, Vitest. USER MANUAL (AC-8 to AC-11, incl. Ctrl+P and domain reveal) confirmed 2026-10-13 on the current build: works |
| 7 | Observation opportunity scan; list registrants with empty descriptions (Open Q 4); `dia docs spec-done` | none | Not Started | haiku | Suggestions only. CoW copy re-synced by f1-harness plan task 15 |

## Order and parallelism

1 and 2 in parallel. 3 after 2. 4 after 1 (approved), 3 and 5. 6 and 7 last.

## Risks

- Ctrl+P may be consumed by the host window or Ultralight (task 5). Cheap to find out, so it is checked early.
- Palette quality depends on registrants filling in `description`; task 7 reports gaps rather than fixing them here.
