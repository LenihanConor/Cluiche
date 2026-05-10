# Feature Spec: Stage System

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Stage System | (this file) |

## Problem Statement

The framework needs a mechanism to switch which modules are active at runtime. Stages are config-declared sets of modules — transitioning between stages starts new modules and stops old ones via a diff algorithm, without disrupting retained ("all") modules.

## Acceptance Criteria

1. Stages declared in manifest as a string array of stage names
2. Each module declares which stages it belongs to (array of stage names or `"all"`)
3. `"all"` is a reserved keyword meaning "always active, never stopped during transitions"
4. `Application::TransitionTo(stageId)` queues an app-wide stage transition
5. Transitions execute at start of next frame (never mid-update)
6. Only one pending transition queued — latest call wins (overwrites previous pending)
7. If TransitionTo called during an in-progress transition, it queues and executes after current completes
8. Diff algorithm computes: RETAIN (in both stages), STOP (not in target), START (new in target)
9. Stop phase: outgoing modules stopped in reverse dependency order
10. Start phase: incoming modules started in dependency order
11. Retained modules continue updating throughout transition
12. `initial_stage` in manifest defines which stage begins on Application::Start()
13. `auto_stages` in manifest lists stages that auto-advance when all modules report kReady
14. Auto-advance triggers TransitionTo for the next stage in the stages array
15. `Application::GetCurrentStage()` returns current active stage ID
16. Transition complete when all START modules reach Active state

## Public API

```cpp
namespace Dia::ApplicationFlow {

    class Application {
    public:
        // Stage transitions (app-wide, queued, executes start of next frame)
        void TransitionTo(const StringCRC& stageId);
        StringCRC GetCurrentStage() const;
        bool IsTransitioning() const;
    };
}
```

## Transition Algorithm

```
1. TransitionTo(targetStage) called — store as pending
2. Start of next frame: if pending transition exists, begin
3. Compute diff against current stage:
   - RETAIN: modules with stages containing both current AND target (or "all")
   - STOP:   modules in current stage but NOT in target (and not "all")
   - START:  modules in target stage but NOT in current (and not "all")
4. For each PU, stop STOP modules (reverse dependency order)
   - Call BeginStop() on each
   - Wait for all to reach Inactive (or timeout → error policy)
5. For each PU, start START modules (dependency order)
   - Call BeginStart() on each
   - Wait for all to reach Active (or timeout → error policy)
6. During steps 4-5, RETAIN modules continue receiving DoUpdate()
7. On all START modules Active: transition complete, set current = target
8. If auto_stage: immediately queue TransitionTo for next stage in array

On failure during step 5 (module kFailed or timeout):
  - Boot stage: shutdown application
  - Debug: DIA_ASSERT
  - Release: rollback — stop START modules, restart STOP modules (return to previous stage)
```

## Auto-Advance Rules

- A stage listed in `auto_stages` triggers TransitionTo automatically when all its modules are Active
- The target of auto-advance is the next stage in the `stages` array after the current one
- If no next stage exists (current is last in array), no auto-advance occurs
- Auto-advance is checked once per frame after transition completes

## Data Model

From manifest:
```json
{
    "stages": ["Boot", "DummyStage", "StupidStage"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot"]
}
```

Internal representation:
```cpp
namespace Dia::ApplicationFlow {
    struct StageDefinition {
        StringCRC id;
        bool isAuto;
    };

    enum class TransitionPhase {
        kNone,
        kStoppingOutgoing,
        kStartingIncoming,
        kComplete
    };
}
```

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Application.h` | New — Application class (stage transition API) |
| `Dia/DiaApplicationFlow/Application.cpp` | New — transition algorithm, auto-advance |
| `Dia/DiaApplicationFlow/StageDefinition.h` | New — stage data types |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update file list |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update filters |

## Dependencies

- **Module Lifecycle** (this system) — drives BeginStart/BeginStop on modules
- **DiaCore/CRC/StringCRC** — stage IDs
- **DiaCore/Containers/DynamicArrayC** — stage list, module diff sets
- **DiaLogger** — log transition events (entering, leaving, complete, failed)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — stage IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC for stage lists, no std:: in public API |
| PD-007 | Platform | C++20 required | Compliant — enum class, constexpr |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — stages, initial_stage, auto_stages all from manifest |
| SD-002 | DiaAppFlow | Stages replace Phases | Compliant — this feature IS that replacement |
| SD-004 | DiaAppFlow | TransitionTo is app-wide | Compliant — all PUs transition together |
| SD-005 | DiaAppFlow | Transitions at start of next frame | Compliant — queued, processed at frame boundary |
| SD-009 | DiaAppFlow | Assert/rollback/shutdown error policy | Compliant — failure during transition follows policy |
| SD-011 | DiaAppFlow | Shutdown is framework-level, not a stage | Compliant — no shutdown stage; RequestShutdown is separate |
| SD-012 | DiaAppFlow | Hot reload = re-enter current stage | Compliant — TransitionTo(currentStage) causes STOP all non-"all" then START them again |
| SD-017 | DiaAppFlow | Clean break | Compliant — no Phase concept, fresh implementation |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Diff | When TransitionTo(currentStage) is called (hot reload), are "all" modules affected? | No. "all" modules are always RETAIN. Hot reload stops and restarts only stage-specific modules. |
| 2 | Ordering | Should stop and start happen sequentially (all stops complete before any start), or interleaved per-PU? | Sequential globally: ALL stops across all PUs complete, THEN all starts across all PUs begin. Simplifies reasoning about shared streams during transition. |
| 3 | Auto-advance | What if an auto-stage has zero modules to start (all are "all")? | Transition completes immediately (nothing to stop, nothing to start). Auto-advance fires on the same frame. |
| 4 | Rollback | During rollback, what if a STOP module (being restarted) also fails? | Shutdown application. Double failure = unrecoverable. Log diagnostic. |
| 5 | Thread safety | Is TransitionTo thread-safe (callable from any PU's thread)? | Yes. Stores pending stage atomically. Transition logic only executes on the main frame boundary (Application::Update on main thread). |
| 6 | Validation | Should TransitionTo to an unknown stage ID be caught at call time or transition time? | Call time — DIA_ASSERT in debug, no-op with error log in release. Stage list is known at Application::Start(). |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
