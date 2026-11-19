# Plan: Launch Presets (Stage Catalogue Feature 2)

**Spec:** @docs/specs/applications/dia/systems/stagecatalogue/launch-presets.md
**Status:** In Progress
**Prereq:** Feature 1 and Feature 3 are Done. Feature 4 (boot menu view) is not built; Task 10 only adds preset rows to `BootMenuLogic` (unit-tested), the view wiring follows with F4.

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| `Module::OnStageLaunch(const char*)`, `IApplicationControl::TransitionTo(stage, preset)`, `GetCurrentPreset()`, `Application::SetInitialLaunch(stage, preset)`, optional preset argument on `AutomationService::ReleaseNavigationHold` | DiaApplicationFlow, DiaAutomation | Given 2026-11-18 |

## Parallel structure

- **Track P (Python):** Tasks 1, 9. Touch only `Dia/DiaCLI`.
- **Track C (C++):** Tasks 2-8, 10, 11, in order (shared headers, vcxproj, `Application.cpp`).
- Tasks 1 and 2 may run concurrently. Join at Task 12 (E2E) and 13 (close-out).

## Open Design Questions: resolved for this plan (no spec edit)

- Merge: shallow, done by each module (the spec default). The application passes only the override JSON.
- Task 4 decides the exact call site by reading `Application.cpp` near the two `BeginStart` calls (~lines 399, 1171).

## Implementation Patterns

- Namespaces `Dia::ApplicationFlow::`, `Dia::Game::`, `Cluiche::AppFlow::`. Fixed-size Dia strings and `DynamicArrayC`; no STL in public headers. Never `str = cstr` on `String<N>` (it formats); over-limit values fail or are ignored with a log, never truncated.
- Override JSON crosses APIs as `const char*`; stored as JSON text.
- Names hash to `StringCRC`; strings kept for display and the file format.
- Observation (AC 15) is part of each task: `DIA_LOG_*` for preset launch requested / `OnStageLaunch` delivered / unknown preset refused / initial launch issued or skipped.
- Test helpers ship in `<Module>/Testing/`; tests in `Cluiche/Tests/GoogleTests/`.
- Add files with `dia docs vcxproj-add` (PowerShell or literal paths).
- Scoped runs while iterating, `--shards 2` or more. One full run at the end (Task 13).
- `dia run|launch` only; never the exe directly. Kill leftover `CluicheTest.exe`/`CoW` before E2E.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **P:** RED → GREEN: `presets` validation in `cli_validate.py` (≤16, unique kebab-case name ≤31, description ≤120, `modules` object of objects, key must be an instance declared in the stage's own `.diaapp` and stage-scoped, `all`-scoped is an error that says so, >2 KB per preset error) | AC 1-2: pytest per rule and message | Done | sonnet | RED-GREEN; RED proven by reverting file (19 fail); 94 pytest pass; validate manifest 80/0. 2 KB measured on compact JSON of whole preset entry |
| 2 | **C:** RED → GREEN: `DiaStageManifest::presets` (name, description, per-module id + override JSON text); `JsonDiaStageSerializer` round trip, override kept as JSON text; limits fail, never truncate | AC 1, 3: round trip, wrong type, 17 presets, over 2 KB | Done | sonnet | RED-GREEN. DiaGame; RED 18 fail; GREEN 18 pass x2 shards. 4 modules/preset cap, override char[2049]; DiaStageManifest now ~130 KB (only used as one local in StageCatalogueLoader: stack risk, keep off recursion/copies). File IO uses 256 KB heap buffer |
| 3 | **C:** RED → GREEN: `ManifestComposerV2::MergeStage` reads `presets` into `StageDeclaration`; malformed `presets` logged and ignored, stage loads | AC 4: composer test with fixture; malformed ignored | Done | sonnet | RED-GREEN. DiaApplicationFlow; RED 6 fail+SEH; 96/97 pass (ManifestSaver known flake). Shared preset table (64) + 32 KB override pool in ApplicationManifestV3 (+65 KB, manifest ~1.5 MB heap-only); StageDeclaration holds presetFirst/Count; accessors FindPreset/HasPreset/GetPresetOverrideJson on manifest |
| 4 | **C:** RED → GREEN: `Module::OnStageLaunch(const char*)` default no-op; `Application` calls it before `BeginStart` on every module starting for the new stage, with that module's override or `""` | AC 6: fake app: only starting modules, before BeginStart, not for an `all` module already running, `""` with no preset, right override per module | Done | sonnet | RED-GREEN. Log delivery (module, bytes). Update module doc; RED compile errors; 6 new tests pass. OnStageLaunch runs on main thread (BeginStart is main-thread), protected like OnConfigure. TransitionToInternal(stage, preset) is the base for Task 5. TypeDiscoveryService.* fails when 2 shards share a temp file (shard collision, unrelated, same family as ManifestSaver flakes) |
| 5 | **C:** RED → GREEN: `IApplicationControl::TransitionTo(stage, preset)`; unknown preset logs Error and does not transition; `GetCurrentPreset()`; existing `TransitionTo(stage)` = no preset; update all implementers/fakes | AC 5, 7: transition with preset, unknown preset no-op, GetCurrentPreset | Done | sonnet | RED-GREEN. Fixes any test fake that implements the interface; RED compile errors; 233/235 pass x2 shards. Application is the only IApplicationControl implementer. Preset on a no-op same-stage transition is ignored (existing behaviour). CluicheTest/CoW not built separately yet |
| 6 | **C:** RED → GREEN: `RenderToSimNavRequest::preset` (default empty); `SimNavigationHandlerModule` passes it; `AutomationService::ReleaseNavigationHold` optional preset argument | AC 8: preset passthrough, including through a hold; HUD navigation unchanged | Pending | sonnet | RED-GREEN. DiaAutomation + Baseline |
| 7 | **C:** RED → GREEN: `stages.launch {stage, preset?}` in `StageCatalogueModule`; `StageLaunchRequest` EventStream MainPU→SimPU (capacity 4) read by `SimNavigationHandlerModule`; declare stream in `cow_main.diaapp`, `cluiche_main.diaapp`; catalogue entry and snapshot carry preset names and descriptions into `stages.list`/`stages.get` | AC 9, 11: refused outside Boot, unknown stage or preset refused, request queued, `{ok:true}`; preset names in snapshot, no override JSON | Pending | sonnet | RED-GREEN. Log "preset launch requested (source)". `dia validate manifest` passes |
| 8 | **C:** RED → GREEN: `Application::SetInitialLaunch(stage, preset)`; exe entry parses `--stage`/`--preset` (CoW and CluicheTest `main`); `SimNavigationHandlerModule` issues the transition once when Boot is ready unless a hold is active; unknown stage logs Error and stays at Boot | AC 13: issued once; skipped under hold; unknown stage | Pending | sonnet | RED-GREEN. Log issued/skipped |
| 9 | **P:** RED → GREEN: `dia run`/`dia launch` `--stage`/`--preset`: validate against the game's files before build/launch (typo exits non-zero and lists valid names), pass `--stage S [--preset P]` through to the exe (reuse DiaViewLaunch F4 passthrough if present), `--preset` without `--stage` is an error | AC 12: pytest for validation errors with valid-name list, passthrough args, preset-without-stage | Pending | sonnet | RED-GREEN. Read `cli/run.py`, `cli/launch.py`. `dia preset` unchanged |
| 10 | **C:** RED → GREEN: `BootMenuLogic` exposes preset sub-rows under each stage row (description as tooltip; click stage row = no preset, sub-row = preset) | AC 10: logic unit tests | Pending | sonnet | RED-GREEN. View wiring belongs to F4 |
| 11 | **C:** RED → GREEN: S-1 `WorldGenerationModule::OnStageLaunch` applies `preset`, `seed`, `size` over its manifest config (shallow); `s1world_generation_testbed_stage.diastage` declares at least two presets | AC 14: module test; `dia validate manifest` passes | Pending | sonnet | RED-GREEN |
| 12 | E2E: `dia run cow --stage S1WorldGenerationTestbedStage --preset <p>` reaches the stage; `cow.worldgen` query shows the preset's seed and size; `dia query stages.launch` from Boot does the same | The three E2E steps | Pending | sonnet | Kill leftover exes first; `dia test e2e` does not build, so compile-code first |
| 13 | Close-out gap audit and edge tests (no mutation checks). One full `dia run googletest --shards 2`. Observation scan (AC 15 logs present). Spec and plan Done, backlog updated | Full suite pass; gaps listed and covered | Pending | sonnet | Sanitizer only on request |

## Notes

- Report results only (pass/fail, failing names, errors) per the build-output rule.
- Commit after each task; update this plan in the same commit.
- The plan Status values for tasks: Pending → In Progress → Done (`dia docs plan` rejects Pending → Done).
