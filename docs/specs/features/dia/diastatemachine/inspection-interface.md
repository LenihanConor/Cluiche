# Feature Spec: Inspection Interface

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **inspection-interface** |

**Status:** `Done`

---

## Problem Statement

All three state machine types (Flat, HSM, Pushdown) need to be observable by external tools — logging, editor visualization, and debugging. Without a shared interface, each tool would need type-specific code paths for each machine variant, tripling the integration surface and preventing future machine types from being automatically supported.

---

## Solution Overview

`IStateMachineInspectable` is a pure virtual interface that all state machine types implement. It provides read-only queries for: machine identity, current active state, full state topology, transition topology, and recent transition history. Supporting data structs (`StateInfo`, `TransitionInfo`, `TransitionRecord`) carry the query results using DiaCore containers.

Transition history is stored as a fixed-size ring buffer (default 32 entries, configurable at construction) per machine instance — bounded memory, no allocation during transitions.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `IStateMachineInspectable` is a pure virtual interface with no data members | Compile check |
| AC2 | `GetMachineId()` returns the StringCRC set at construction | Unit test |
| AC3 | `GetCurrentStateId()` returns the active state's StringCRC | Unit test |
| AC4 | `GetAllStates()` populates a `DynamicArrayC<StateInfo>` with all states in the machine | Unit test |
| AC5 | `StateInfo` contains `id`, `isActive`, `isLeaf`, `parentId` fields | Compile check |
| AC6 | `GetAllTransitions()` populates a `DynamicArrayC<TransitionInfo>` with all transitions | Unit test |
| AC7 | `TransitionInfo` contains `sourceStateId`, `targetStateId`, `triggerId`, `hasGuard` | Compile check |
| AC8 | `GetTransitionHistory()` returns recent transitions as `DynamicArrayC<TransitionRecord>` | Unit test |
| AC9 | `TransitionRecord` contains `sourceStateId`, `targetStateId`, `triggerId`, `timestamp` | Compile check |
| AC10 | History ring buffer respects `maxEntries` parameter — oldest entries evicted when full | Unit test |
| AC11 | History ring buffer size configurable at construction (default 32) | Unit test |
| AC12 | No STL containers in any public API | Compile check |
| AC13 | All IDs are `Dia::Core::StringCRC` | Compile check |
| AC14 | `SetTransitionListener(nullptr)` — no listener, zero overhead | Unit test: no callbacks invoked |
| AC15 | `SetTransitionListener(listener)` — listener receives `OnTransition` on each transition | Unit test |
| AC16 | `OnTransitionFailed` called on failed `Fire()` when listener is set | Unit test |
| AC17 | `TransitionEvent` contains guard evaluation results (name + pass/fail) | Unit test |
| AC18 | For HSM, `TransitionRecord.sourceStateId` is the leaf state exited, `targetStateId` is the leaf state entered | Unit test |

---

## Public API

```cpp
namespace Dia::StateMachine {

    struct StateInfo {
        Dia::Core::StringCRC id;
        bool isActive;
        bool isLeaf;
        Dia::Core::StringCRC parentId;  // kInvalidCRC for root/flat states
    };

    struct TransitionInfo {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;  // kInvalidCRC for unconditional
        bool hasGuard;
    };

    struct TransitionRecord {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;
        float timestamp;
    };

    // Transition listener for tracing/debugging — nullable, zero overhead when not set
    struct GuardEvaluation {
        Dia::Core::StringCRC guardName;
        bool passed;
    };

    struct TransitionEvent {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;
        float timestamp;
        float stateResidenceTime;
        const GuardEvaluation* guardResults;  // non-owning, valid only during callback
        int guardCount;
    };

    class ITransitionListener {
    public:
        virtual ~ITransitionListener() = default;
        virtual void OnTransition(const TransitionEvent& event) = 0;
        virtual void OnTransitionFailed(Dia::Core::StringCRC machineId,
                                        Dia::Core::StringCRC currentStateId,
                                        Dia::Core::StringCRC triggerId) = 0;
    };

    class IStateMachineInspectable {
    public:
        virtual ~IStateMachineInspectable() = default;

        virtual Dia::Core::StringCRC GetMachineId() const = 0;
        virtual Dia::Core::StringCRC GetCurrentStateId() const = 0;

        virtual void GetAllStates(
            Dia::Core::DynamicArrayC<StateInfo>& outStates) const = 0;
        virtual void GetAllTransitions(
            Dia::Core::DynamicArrayC<TransitionInfo>& outTransitions) const = 0;
        virtual void GetTransitionHistory(
            Dia::Core::DynamicArrayC<TransitionRecord>& outHistory,
            int maxEntries = 32) const = 0;

        // Transition listener — nullable. Set to nullptr for zero overhead.
        virtual void SetTransitionListener(ITransitionListener* listener) = 0;
    };

} // namespace Dia::StateMachine
```

---

## Implementation Notes

- `parentId` in `StateInfo` is `kInvalidCRC` for flat FSM states and pushdown states (no hierarchy). HSM states populate it with their parent's ID.
- `isLeaf` is always `true` for flat FSM and pushdown. HSM sets it based on whether the state has children.
- Ring buffer implementation: fixed-size array with head/tail indices. No heap allocation. `TransitionRecord` is a POD struct for cache-friendly storage.
- `timestamp` in `TransitionRecord` should be the engine-relative time (e.g., from `DiaCore/Time/`), not wall-clock time.
- **History record semantics:** For flat FSM and pushdown, `TransitionRecord` records the direct state change. For HSM, it records leaf-to-leaf: `sourceStateId` is the leaf state exited, `targetStateId` is the leaf state entered, even if the transition was triggered at an ancestor level. This gives a unified format for tooling.
- **ITransitionListener:** Nullable pointer on `IStateMachineInspectable`. When null, machines skip all listener calls — zero overhead. When set, machines call `OnTransition` after each successful transition and `OnTransitionFailed` after each failed `Fire()`. The `TransitionEvent` struct carries guard evaluation results (names + pass/fail) which the `StateMachineTracer` uses for `kTransitionsAndGuards` verbosity.
- `GuardEvaluation` array in `TransitionEvent` is stack-allocated by the machine during transition dispatch and valid only for the duration of the `OnTransition` callback. Listener must copy if it needs to persist.

---

## Dependencies

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`, time utilities

### Dependent Features
- **flat-state-machine** — implements `IStateMachineInspectable`
- **hierarchical-state-machine** — implements `IStateMachineInspectable`
- **pushdown-automaton** — implements `IStateMachineInspectable`
- **transition-logging** — queries via `IStateMachineInspectable`
- **test-utilities** — queries via `IStateMachineInspectable`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestInspectionInterface.cpp`)

1. Construct a flat FSM — `GetMachineId()` matches construction ID
2. After transition — `GetCurrentStateId()` reflects new state
3. `GetAllStates()` returns correct count and IDs
4. `GetAllTransitions()` returns correct count, source/target/trigger IDs
5. After N transitions — `GetTransitionHistory()` returns N records in order
6. After >32 transitions — oldest entries evicted, most recent 32 retained
7. Custom ring buffer size (e.g., 8) — eviction at 8, not 32
8. `StateInfo.isActive` is `true` only for current state (flat FSM)
9. `StateInfo.parentId` is `kInvalidCRC` for all flat FSM states

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | All state/transition/machine IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` for all output parameters |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-002 | System | States identified by StringCRC | All state identification via StringCRC |
| SD-006 | System | Ring buffer for history | Fixed-size, configurable, no allocation |
