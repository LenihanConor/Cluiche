# Feature Spec: Inspectable Interface

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Inspectable Interface | (this file) |

## Problem Statement

Debug tools, the editor, and test harnesses need read-only access to application runtime state (current stage, module states, stream info, transition progress) without coupling to framework internals.

## Acceptance Criteria

1. `IApplicationInspectable` interface exposes runtime state queries
2. `Application::GetInspectable()` returns the interface
3. Queries: current stage, is transitioning, active modules per PU, all stages, stream info
4. Module state info includes: instance_id, type_id, current ModuleState
5. Stream info includes: id, type string, from PU, to PU
6. Transition progress queryable: which modules are starting/stopping
7. Interface is read-only — no mutation methods
8. Consumers: DiaDebugServer (WebSocket adapter), DiaApplicationFlowEditor (live mode), DiaTestHarness (wait commands), GoogleTests
9. Thread-safe reads (can be called from any thread — returns snapshot)

## Public API

```cpp
namespace Dia::ApplicationFlow {

    struct ModuleStateInfo {
        StringCRC instanceId;
        StringCRC typeId;
        ModuleState state;
    };

    struct StreamInfo {
        StringCRC id;
        StringCRC type;
        StringCRC fromPU;
        StringCRC toPU;
        bool multiWriter;
    };

    struct TransitionInfo {
        bool inProgress;
        StringCRC fromStage;
        StringCRC toStage;
        DynamicArrayC<StringCRC, 32> modulesStarting;
        DynamicArrayC<StringCRC, 32> modulesStopping;
    };

    class IApplicationInspectable {
    public:
        virtual ~IApplicationInspectable() = default;

        virtual StringCRC GetCurrentStage() const = 0;
        virtual bool IsTransitioning() const = 0;
        virtual TransitionInfo GetTransitionInfo() const = 0;

        virtual void GetAllStages(DynamicArrayC<StringCRC, 16>& out) const = 0;
        virtual void GetProcessingUnits(DynamicArrayC<StringCRC, 4>& out) const = 0;

        virtual void GetActiveModules(
            const StringCRC& puId,
            DynamicArrayC<ModuleStateInfo, 64>& out) const = 0;

        virtual void GetStreamInfo(
            DynamicArrayC<StreamInfo, 16>& out) const = 0;

        virtual bool IsShuttingDown() const = 0;
    };
}
```

## Thread Safety

- All getters return copies/snapshots of internal state
- `GetTransitionInfo()` acquires a read lock on transition state
- Module states are atomic enum values — safe to read without lock
- Callers do not hold references into framework internals

## Consumer Patterns

| Consumer | Usage |
|----------|-------|
| DiaDebugServer | Adapts IApplicationInspectable queries to DiaAPI JSON responses (`get_app_state`, `get_active_modules`) |
| DiaApplicationFlowEditor (live) | Polls at 10Hz for stage/module state to overlay on static view |
| DiaTestHarness | `wait_stage_ready` blocks until `IsTransitioning() == false && GetCurrentStage() == target` |
| GoogleTests | Assert module states after programmatic transitions |

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/IApplicationInspectable.h` | New — interface definition |
| `Dia/DiaApplicationFlow/Introspection/ApplicationIntrospector.h` | Rewrite — v2 implementation |
| `Dia/DiaApplicationFlow/Introspection/ApplicationIntrospector.cpp` | Rewrite |
| `Dia/DiaApplicationFlow/Application.h` | Add GetInspectable() method |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update |

## Dependencies

- **Module Lifecycle** (this system) — ModuleState enum
- **Stage System** (this system) — current stage, transition state
- **Streams** (this system) — stream metadata
- **DiaCore/CRC/StringCRC** — IDs
- **DiaCore/Containers/DynamicArrayC** — output arrays

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — all IDs in info structs are StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC for all outputs |
| PD-007 | Platform | C++20 | Compliant |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-016 | DiaAppFlow | IApplicationInspectable for debug/editor/test | Compliant — this feature implements that decision |
| SD-017 | DiaAppFlow | Clean break | Compliant — v1 Introspector rewritten |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should inspectable expose module config (the opaque JSON blob)? | No. Config is a load-time concern. Runtime inspection is about state, not configuration. Editor reads config from the file directly. |
| 2 | Performance | Should GetActiveModules return ALL modules or only currently-running ones? | All modules in the PU with their current state. Consumer filters by state if needed. |
| 3 | Threading | Should there be a "subscribe to changes" mechanism or just polling? | Polling only. Keeps the interface simple. Editor polls at 10Hz — fast enough for visualization. Test harness polls in a tight loop with sleep. |
| 4 | Extension | Should this interface be extensible (virtual + override in Application) or fixed? | Fixed interface implemented by Application directly. No plugin extension point needed. If new queries are needed, add to the interface (compile-time, not runtime extension). |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
