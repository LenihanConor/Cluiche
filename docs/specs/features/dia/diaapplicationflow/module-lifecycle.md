# Feature Spec: Module Lifecycle

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Module Lifecycle | (this file) |

## Problem Statement

DiaApplicationFlow needs a base Module class with a well-defined lifecycle contract that the framework drives. Modules must support async startup (asset loading spans multiple frames), graceful shutdown, and framework-enforced timeouts — all without modules knowing about the framework's stage/transition internals.

## Acceptance Criteria

1. Module exposes `DoStart()` returning `StartResult { kReady, kLoading, kFailed }`
2. Framework calls `DoStart()` each frame until the module reports kReady or kFailed
3. Module exposes `DoUpdate(float deltaTime)` — called each frame while state is Active
4. Module exposes `DoStop()` returning `StopResult { kDone, kStopping }`
5. Framework calls `DoStop()` each frame until the module reports kDone
6. Per-module `start_timeout_ms` and `stop_timeout_ms` configurable in manifest
7. Timeout triggers the error handling policy (assert in debug, rollback in release, shutdown on boot — per system SD-009)
8. Module state machine: `Inactive → Starting → Active → Stopping → Inactive` (+ `Failed` terminal state)
9. Framework drives all state transitions — modules never call their own Start/Stop
10. Module base class provides `GetInstanceId()`, `GetProcessingUnit()`, and `TransitionTo()` (convenience that calls up to Application)
11. Module constructor takes only `instanceId` (StringCRC) — all initialization happens in DoStart

## Public API

```cpp
namespace Dia::ApplicationFlow {

    enum class StartResult { kReady, kLoading, kFailed };
    enum class StopResult { kDone, kStopping };

    enum class ModuleState {
        kInactive,
        kStarting,
        kActive,
        kStopping,
        kFailed
    };

    class Module {
    public:
        explicit Module(const StringCRC& instanceId);
        virtual ~Module() = default;

        // Framework queries
        const StringCRC& GetInstanceId() const;
        ProcessingUnit* GetProcessingUnit() const;
        ModuleState GetState() const;

        // Convenience — delegates to Application::TransitionTo
        void TransitionTo(const StringCRC& stageId);

    protected:
        // User overrides — pure behavior
        virtual StartResult DoStart() = 0;
        virtual void DoUpdate(float deltaTime) = 0;
        virtual StopResult DoStop() = 0;

    private:
        // Framework-only (friend class ProcessingUnit / Application)
        void SetProcessingUnit(ProcessingUnit* pu);
        void FrameTick(float deltaTime);  // Framework calls this — drives state machine
        void BeginStart();
        void BeginStop();

        StringCRC mInstanceId;
        ProcessingUnit* mProcessingUnit = nullptr;
        ModuleState mState = ModuleState::kInactive;
        float mStateElapsedMs = 0.0f;
    };
}
```

## Module State Machine

```
                    BeginStart()
    Inactive ──────────────────────► Starting
                                        │
                          DoStart() called each frame
                                        │
                    ┌───────────────────┼───────────────────┐
                    │ kReady            │ kLoading           │ kFailed
                    ▼                   │ (loop)             ▼
                 Active                 │                  Failed
                    │                   │
             BeginStop()                │ timeout
                    │                   ▼
                    ▼              [Error Policy]
                Stopping
                    │
          DoStop() called each frame
                    │
         ┌──────────┼──────────┐
         │ kDone    │ kStopping│
         ▼          │ (loop)   │ timeout
      Inactive      │          ▼
                    │     [Error Policy]
```

## Timeout Handling

- `start_timeout_ms`: Clock starts when BeginStart() is called. If module still returns kLoading after this duration, trigger error policy.
- `stop_timeout_ms`: Clock starts when BeginStop() is called. If module still returns kStopping after this duration, trigger error policy.
- Default values (if not specified in manifest): `start_timeout_ms = 10000`, `stop_timeout_ms = 5000`
- Timeout measurement uses wall-clock delta accumulation (not game time — game time may be paused)

## Error Policy (consumed from system decision SD-009)

| Context | Action on timeout/failure |
|---------|--------------------------|
| Boot stage | Shutdown application |
| Debug build (any stage) | DIA_ASSERT with diagnostic message |
| Release build (non-boot) | Rollback to previous stage |

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Module.h` | New — Module base class, enums |
| `Dia/DiaApplicationFlow/Module.cpp` | New — state machine implementation |
| `Dia/DiaApplicationFlow/ApplicationModule.h` | Delete (v1) |
| `Dia/DiaApplicationFlow/ApplicationModule.cpp` | Delete (v1) |
| `Dia/DiaApplicationFlow/ApplicationStateObject.h` | Delete (v1 — lifecycle state folded into Module) |
| `Dia/DiaApplicationFlow/ApplicationStateObject.cpp` | Delete (v1) |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update file list |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update filters |

## Dependencies

- **DiaCore/CRC/StringCRC** — instance ID type
- **DiaCore/Core/DIA_ASSERT** — debug assertion on timeout
- **DiaLogger** — log channel for lifecycle events (per CluicheTest SD-007)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — Module instanceId is StringCRC |
| PD-002 | Platform | PU/Phase/Module architecture | Compliant — Module is one of the three pillars (Phase replaced by Stage at system level, not this feature's concern) |
| PD-004 | Platform | No STL in public APIs | Compliant — no STL in Module public interface. Internal state uses primitives (float, enum, pointer) |
| PD-005 | Platform | x64 only | Compliant — no platform-specific code |
| PD-006 | Platform | VS project files source of truth | Compliant — vcxproj updated manually |
| PD-007 | Platform | C++20 required | Compliant — uses enum class, constexpr, explicit |
| PD-008 | Platform | Directory.Build.props owns build settings | Compliant — no build setting overrides |
| AD-001 | Dia App | YAML frontmatter module docs | Compliant — dia.applicationflow.architecture.module.md will be updated |
| AD-002 | Dia App | No STL in public APIs | Compliant — same as PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — timeout values come from manifest config, not code |
| SD-009 | DiaAppFlow | Assert debug / rollback release / shutdown boot | Compliant — error policy implemented per this decision |
| SD-013 | DiaAppFlow | DoStart returns StartResult | Compliant — this feature defines and implements that contract |
| SD-017 | DiaAppFlow | Clean break, no backward compatibility | Compliant — v1 files deleted, fresh implementation |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | State Machine | Should Failed be a terminal state or allow retry (re-enter Starting)? | Terminal. Retry = re-enter stage (SD-012). Failed module stays failed until stage transition resets it. |
| 2 | Timeout | Should timeout reset if DoStart returns kLoading with "progress" (i.e., a watchdog pattern)? | No. Simple elapsed timer. If modules need longer, increase timeout in config. Progress reporting is a future extension. |
| 3 | API | Should DoUpdate receive any context beyond deltaTime (e.g., frame number, total time)? | No. deltaTime is sufficient. Modules that need frame count or total time depend on TimeServerModule. |
| 4 | Threading | Is FrameTick() called from the PU's thread or the main thread? | PU's thread. Each PU ticks its own modules. Module code runs on the owning PU's thread. |
| 5 | Destruction | When is a Module destroyed? On stage exit, or kept pooled? | Modules are created once at Application::Start() and destroyed at shutdown. Stage transitions call BeginStart/BeginStop — not construct/destroy. |
| 6 | Defaults | Are 10s start / 5s stop defaults correct for a game? | Yes for development. Release builds with large asset loads can override per-module. 10s is generous enough to catch real hangs without false positives. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
