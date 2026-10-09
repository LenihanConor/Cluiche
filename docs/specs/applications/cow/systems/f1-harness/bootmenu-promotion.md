# Feature Spec: BootMenuModule Promotion

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/F1_Harness.md` (Test 2 Stage Launch and Return, Test 3 Promotion Parity)
**Origin:** @docs/specs/applications/cluichetest/systems/applicationflow/imgui-bootstrap-menu.md (the CluicheTest feature this module came from)
**Status:** `Approved`

## Summary

Move `BootMenuModule` from CluicheTest into CluicheGameBaseline so any Cluiche game gets a testbed launcher. Replace its direct dependency on `CluicheTest::TestResultsRegistry` with a new baseline `StageStatusModule` that the menu finds through an optional `ModuleRef`. CluicheTest keeps its behaviour through a CluicheTest-side bridge; CoW adds no status module and the menu simply omits the badges.

This replaces the `IStageStatusProvider` interface proposed in the F-1 system spec. `ModuleRef<T>` only finds concrete sibling modules, so a concrete module is the idiomatic route and needs no singleton or setter.

## Goals

1. `BootMenuModule` compiles and runs in CluicheGameBaseline with no CluicheTest type.
2. CluicheTest's stages launch and return through it exactly as before (badges, loaded markers, counts).
3. A game with no `StageStatusModule` gets a working menu without status UI.
4. Window title and project line are per-game configuration, not hard-coded to CluicheTest.

## Non-Goals

1. Boot choice and `--play` handling (separate F-1 feature).
2. Shipping-build gating (separate F-1 feature).
3. Changing launch semantics, the nav-request channel, or automation behaviour.
4. Removing `TestResultsRegistry` (it stays in CluicheTest).

## Design

### Findings (current code)

- `BootMenuModule` uses `TestResultsRegistry` at four sites: restoring "loaded" markers in `DoStart`, per-stage badges (`GetStatusLabel`), the Passed/Failed/Pending footer, and `IsLoadedThisSession`.
- Registry states are test-shaped (`kNotRun/kRunning/kPassed/kFailed/kTimeout`).
- Hard-coded text: window title `"CluicheTest — Bootstrap"` and `"Project: CluicheTest v0.1"`.
- Already baseline-resident: `DebugUIModule`, `DebugServerHostModule`, `RenderToSimNavRequest`, `AutomationModule`.
- Registered in `cluiche_main.diaapp` as `BootMenu` on the Boot stage; the `BootMenuNavRequest` channel is declared there.
- Module manifest entries support a `config` block.

### StageStatusModule (new, baseline, RenderPU)

Holds per-stage entries keyed by stage `StringCRC`:

```
struct StageStatus { const char* label; Severity severity; bool visited; };
enum class Severity { kNone, kOk, kWarn, kFail };
```

Public: `Set(stage, status)`, `Get(stage) -> const StageStatus*`. Generic: it knows nothing about tests.

### BootMenuModule changes

- Holds `ModuleRef<StageStatusModule>` (optional). Absent or inactive means no badge column and no counts footer.
- Badge text and colour come from `label` and `severity`. The footer shows counts by severity.
- Visited markers: the menu marks a stage visited when it sends the nav request, and on `DoStart` also restores any stage whose provider entry has `visited`. This preserves today's return-to-Boot behaviour.
- Title and project line read from module `config` (`title`, `project`), with defaults `"Boot Menu"` and an empty project line.

### CluicheTest bridge

A CluicheTest-side RenderPU Boot-stage module (`TestStatusBridgeModule`) maps each `TestResultsRegistry` result into `StageStatusModule` (`label` is the existing text: PASSED / FAILED / TIMEOUT / RUNNING / NOT RUN; severity: passed to `kOk`, failed and timeout to `kFail`, running and not-run to `kNone`; `visited` is true for any state other than not-run) on start and each update. `cluiche_main.diaapp` gains the bridge, makes `BootMenu` depend on it so it starts first (the menu restores visited markers in `DoStart`), and sets the title/project config to `"CluicheTest — Bootstrap"` and `"Project: CluicheTest v0.1"`.

### Parity with current CluicheTest behaviour

| Behaviour | Current | After promotion | Same? |
|-----------|---------|-----------------|-------|
| Stage list, selection, Launch button / Enter, nav request | `GetStageTransitions("Boot")`, `BootMenuNavRequest` | Unchanged | Yes |
| Window title, project line | Hard-coded | Same strings via `config` | Yes |
| "Automation: Connected" badge | `DebugServerHostModule` | Unchanged | Yes |
| Badge text | PASSED / FAILED / TIMEOUT / RUNNING / NOT RUN | Same text via `label` | Yes |
| Badge colour | Green passed; red failed and timeout; grey otherwise (incl. running) | Same via severity (`kOk`, `kFail`, `kNone`) | Yes |
| Visited marker on return to Boot | Any state except not-run | Same via `visited` | Yes |
| Passed/Failed/Pending footer | Running counts as Failed (any non-passed, non-not-run state) | Running is `kNone`, so counts as Pending | **Edge case differs**: only visible if a stage is `kRunning` while Boot is showing, which does not happen in normal flow |
| Visited marker on launch | Not set (menu stops on launch) | Also set on launch | Invisible: Boot stops on launch |
| Footer with no registry | Shows 0/0/Pending N | Hidden when no `StageStatusModule` | CluicheTest unaffected; CoW differs by design |
| Threading | Registry read on RenderPU | Bridge reads it on RenderPU | Yes |

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `BootMenuModule` lives under `Cluiche/CluicheGameBaseline/Modules/` and its sources reference no CluicheTest header or type. |
| AC2 | `StageStatusModule` stores and returns per-stage `StageStatus`; unknown stages return null. |
| AC3 | With no `StageStatusModule` present, the menu lists all Boot transitions, launches any, and shows no badges or counts. |
| AC4 | With a populated `StageStatusModule`, badges show label and severity colour, and the footer counts match the entries. |
| AC5 | Returning to Boot restores visited markers for stages whose entry has `visited`. |
| AC6 | Window title and project line come from the module `config`, with defaults when absent. |
| AC7 | CluicheTest: all existing stages launch and return through the promoted menu; title, badge text and colour, visited markers and counts match pre-promotion behaviour (the running-stage footer edge case in the parity table is the only accepted difference). |
| AC8 | Stage launch and return repeated for each stage leaves no menu state that carries into the next stage (no stale selection or subscription). |
| AC9 | E2E automation `navigate_to` still works unchanged. |

## Tasks

| # | Task | Test | Status | Model |
|---|------|------|--------|-------|
| 1 | Add `StageStatusModule` to the baseline (header, cpp, vcxproj, module doc) | AC2 | Draft | sonnet |
| 2 | Move `BootMenuModule` to the baseline; remove registry use; optional `ModuleRef`; config-driven title | AC1, AC3, AC4, AC5, AC6 | Draft | sonnet |
| 3 | Add `TestStatusBridgeModule` to CluicheTest; update `cluiche_main.diaapp` and `CluicheTest.vcxproj` | AC7 | Draft | sonnet |
| 4 | Run CluicheTest through every stage and return; run GoogleTests | AC7, AC8, AC9 | Draft | haiku |

Per CLAUDE.md, tasks 1 and 2 follow TDD (RED before GREEN), and the baseline needs a test helper in `<Module>/Testing/`.

## Binding Decisions

| Source | Constraint |
|--------|------------|
| AD-001 | Tasks 1 to 3 edit CluicheGameBaseline and CluicheTest and need explicit sign-off before implementation. |
| AD-004 | The menu is promoted, not copied; no CluicheTest type may remain (F-1 failure condition). |
| No-singletons rule | Status reaches the menu through `ModuleRef`, not a global. |

## Open Design Questions

1. The bridge reads `TestResultsRegistry`, an existing singleton (counted debt). Keeping it there is the least change, but the bridge could later receive results by stream instead.
2. Does the Passed/Failed/Pending footer belong in the generic menu, or only render when a `StageStatusModule` exists? Current plan: only when it exists.
