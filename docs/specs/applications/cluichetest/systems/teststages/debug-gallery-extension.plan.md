**Spec:** @Cluiche/CluicheTest/Modules/TestStages/DebugGalleryTestStageModule.h (no new spec — extension to existing Done stage)
**Status:** Todo

## Context

`DebugGalleryTestStageModule` currently registers 12 `IDebugDomain` instances (+ 2 auto-registered by `VisualDebuggerModule` = 14 total). The 9 new visual debugger modules (3 Navigation + 6 AI/Behavior) need to be added following the identical pattern: forward-declare + fixture member + domain member in `.h`; construct fixtures + domains in `OnStart`; `RegisterDomain` in first `OnUpdate`; `UnregisterDomain` + `reset()` in `OnStop`.

Navigation domains (`SteeringVisualDebugger`, `PathfindingVisualDebugger`, `FlowFieldVisualDebugger`) have world-space drawers but minimal-fixture construction is fine for gallery purposes — zero agents / empty path produce valid (empty) JSON state, which is sufficient for panel validation.

Panel-only domains (`StateMachineVisualDebugger`, `RulesVisualDebugger`, `HTNVisualDebugger`, `AIBudgetVisualDebugger`, `BlackboardVisualDebugger`, `MailboxVisualDebugger`) need only a default-constructed or minimally initialised fixture reference.

## Dependencies

All 9 visual debugger modules must be built (their plans Done) before this plan starts:
- `diasteeringvisualdebugger.plan.md`
- `diapathfindingvisualdebugger.plan.md`
- `diaflowfieldvisualdebugger.plan.md`
- `diastatemachinevisualdebugger.plan.md`
- `diarulesvisualdebugger.plan.md`
- `diahtnvisualdebugger.plan.md`
- `diaaibudgetvisualdebugger.plan.md`
- `diablackboardvisualdebugger.plan.md`
- `diamailboxvisualdebugger.plan.md`

## API Decisions

- All 9 new domain instances owned as `std::unique_ptr<T>` members inside `#ifdef DIA_DEBUG`, matching the existing 12
- Navigation fixture objects: `SteeringSystem` default-constructible; `PathGrid(8,8,1.0f)` + `PathResult` (empty result is valid); `FlowField(8,8,1.0f)` (no computed field is valid for gallery)
- Panel-only fixture objects: all default-constructible; no inline JSON rule loading needed for gallery
- Domain log message updated from `"12 domains registered"` → `"21 domains registered"` (12 existing + 9 new)
- `ProjectRef` entries added for all 9 new vcxprojs in `CluicheTest.vcxproj`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add forward declarations for 9 new fixture namespaces + 9 new domain namespaces to `DebugGalleryTestStageModule.h`; add 9 `std::unique_ptr<T>` fixture members + 9 `std::unique_ptr<T>` domain members inside `#ifdef DIA_DEBUG` | Build succeeds (header only change) | Todo | haiku | Follow existing block layout — fixture members first, then domain members |
| 2 | Add 9 domain `#include` lines + 9 fixture `#include` lines in `.cpp`; construct fixtures in `OnStart` (`SteeringSystem` default; `PathGrid(8,8,1.0f)` + empty `PathResult`; `FlowField(8,8,1.0f)`; remaining 6 AI fixtures default-constructed); construct all 9 domain instances | Build succeeds; `OnStart` runs without assert | Todo | sonnet | Pattern: fixture constructed first, domain constructed referencing it — mirror existing IK/Rig/Animation block |
| 3 | Add 9 `vd->RegisterDomain(...)` calls in `OnUpdate`; update log message to `"21 domains registered"` | `dia run cluichetest` → DebugGallery → tilde key → 21 domain cards visible in panel | Todo | haiku | Insert after `vd->RegisterDomain(*mMesh3DDomain)` |
| 4 | Add 9 `vd->UnregisterDomain(...)` + 9 `.reset()` calls in `OnStop` in correct order (domains before fixtures); update log to `"21 domains unregistered"` | Clean stop, no assert on re-enter | Todo | haiku | Unregister domains in reverse registration order; reset domains before their fixtures |
| 5 | Add 9 `<ProjectReference>` entries to `CluicheTest.vcxproj` + corresponding filter entries in `.vcxproj.filters` for all 9 new debugger vcxprojs | `msbuild` succeeds; no unresolved external | Todo | haiku | Use `dia docs vcxproj-add` for each — or add all 9 in one manual edit following existing reference block |
| 6 | `dia run cluichetest` → navigate to `DebugGalleryTestStage` → press tilde → confirm 21 domain cards appear; tab through Navigation and AI/Behavior groups; confirm no crash after 300 frames | All 21 cards render; stage returns to Boot at frame 300 | Todo | sonnet | Manual verification pass — quote panel card count from log output |
