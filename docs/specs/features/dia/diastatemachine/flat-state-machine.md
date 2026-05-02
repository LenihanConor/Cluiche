# Feature Spec: Flat State Machine

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **flat-state-machine** |

**Status:** `Approved`

---

## Problem Statement

Many game systems need simple state management — AI agents cycling between Idle/Patrol/Chase, UI elements toggling between states, gameplay triggers with clear on/off behavior. These machines have flat topology (no nesting) and are often instantiated in high volume (hundreds or thousands of AI agents). They need to be lightweight, cache-friendly, and zero-allocation during operation, while still supporting the shared inspection interface for tooling.

---

## Solution Overview

`FlatStateMachine<TContext>` is a template class that manages a flat set of states with event-triggered transitions. States have entry/exit/update actions (raw function pointers for zero overhead). Transitions have optional guard predicates. Transitions are triggered by `Fire(StringCRC)` — the machine scans transitions from the current state matching the trigger, evaluates guards in registration order (first-match wins), and executes the transition.

Supports wildcard transitions via `kAnyState` sentinel — transitions that can fire from any state without per-state duplication. Wildcard transitions are evaluated after state-specific transitions (lower priority).

Constructed from a `StateMachineDefinition&&` (move semantics — machine owns its definition) and a `TContext&` reference. Implements `IStateMachineInspectable`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Construct `FlatStateMachine` from `StateMachineDefinition` + context — enters initial state, calls OnEnter | Unit test |
| AC2 | `Fire(triggerId)` transitions to target state when trigger matches and guard passes | Unit test |
| AC3 | `Fire(triggerId)` returns `false` when no matching transition from current state | Unit test |
| AC4 | `Fire()` calls OnExit on old state, then OnEnter on new state, in that order | Unit test with ordered callback tracking |
| AC5 | `Update(deltaTime)` calls the current state's OnUpdate with deltaTime and context | Unit test |
| AC6 | Guard predicates receive `const TContext&` and return `bool` | Compile check + unit test |
| AC7 | Multiple transitions from same state with same trigger — first guard that passes wins (SD-007) | Unit test with two guarded transitions |
| AC8 | Wildcard transition (`kAnyState` source) fires from any state | Unit test: fire trigger from 3 different states, all transition |
| AC9 | State-specific transitions take priority over wildcard transitions for same trigger | Unit test: state-specific guard passes → wildcard not evaluated |
| AC10 | Max transitions per frame (default 8, SD-008) — DIA_ASSERT on exceeded in debug | Unit test with chain transitions |
| AC11 | `IsInState(stateId)` returns correct value | Unit test |
| AC12 | `GetCurrentStateId()` returns current state via `IStateMachineInspectable` | Unit test |
| AC13 | Transition history populated after each transition | Unit test via `GetTransitionHistory()` |
| AC14 | No heap allocation during `Fire()` or `Update()` | Code review / profiling |
| AC15 | `Fire()` on nonexistent trigger logs at `kDebug` level when tracer attached, silent otherwise | Unit test |
| AC16 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::StateMachine {

    static constexpr Dia::Core::StringCRC kAnyState{"*"};

    template<typename TContext>
    class FlatStateMachine : public IStateMachineInspectable {
    public:
        using ActionFn = void(*)(TContext&);
        using GuardFn = bool(*)(const TContext&);
        using UpdateFn = void(*)(TContext&, float deltaTime);

        explicit FlatStateMachine(Dia::Core::StringCRC machineId,
                                  StateMachineDefinition&& definition,
                                  TContext& context);

        // State query
        Dia::Core::StringCRC GetCurrentStateId() const override;
        bool IsInState(Dia::Core::StringCRC stateId) const;

        // Triggering transitions
        bool Fire(Dia::Core::StringCRC triggerId);

        // Per-frame update (calls current state's OnUpdate)
        void Update(float deltaTime);

        // IStateMachineInspectable implementation
        Dia::Core::StringCRC GetMachineId() const override;
        void GetAllStates(
            Dia::Core::DynamicArrayC<StateInfo>& outStates) const override;
        void GetAllTransitions(
            Dia::Core::DynamicArrayC<TransitionInfo>& outTransitions) const override;
        void GetTransitionHistory(
            Dia::Core::DynamicArrayC<TransitionRecord>& outHistory,
            int maxEntries) const override;
    };

} // namespace Dia::StateMachine
```

---

## Implementation Notes

- `Fire()` scan order: iterate transitions from current state matching trigger ID. If no state-specific transition's guard passes, iterate wildcard transitions matching trigger ID. First match wins (SD-007).
- Action function pointers are stored as `void(*)(void*)` in the definition (type-erased). The machine invokes them by passing `static_cast<void*>(&context)`. Callback authors cast back: `void MyOnEnter(void* ctx) { auto& c = *static_cast<MyCtx*>(ctx); }`. The typed aliases (`ActionFn`, `GuardFn`, `UpdateFn`) on the template class are documentation-only convenience typedefs (SD-003).
- Guard function pointers are `bool(*)(const TContext&)` — must not throw. DIA_ASSERT on throw in debug; treated as `false` in release (SD-014).
- `Fire()` increments a per-frame transition counter. Resets at each `Update()` call. If counter exceeds max (default 8), DIA_ASSERT in debug, return `false` in release (SD-008).
- **OnUpdate and mid-update transitions:** `Update()` calls the current state's OnUpdate. If OnUpdate calls `Fire()` and the machine transitions, the old state's OnUpdate has already completed (it called Fire). The new state's OnUpdate is NOT called until the next `Update()` frame. This prevents unexpected double-updates.
- State data is stored in contiguous arrays indexed by state ordinal (not pointer chasing). Transition lookup is linear scan bounded by transitions-per-state (typically <10).
- `TContext` constrained by C++20 concept: must be non-const lvalue reference type.

---

## Dependencies

### Required Features
- **inspection-interface** — `IStateMachineInspectable`, `StateInfo`, `TransitionInfo`, `TransitionRecord`
- **state-machine-builder** — produces `StateMachineDefinition` consumed by constructor

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`

### Dependent Features
- **state-machine-component** — wraps `FlatStateMachine` instances
- **transition-logging** — traces via `IStateMachineInspectable`
- **test-utilities** — tests against `FlatStateMachine`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestFlatStateMachine.cpp`)

**Basic lifecycle:**
1. Construct with 3 states (Idle, Walking, Running) — starts in Idle, OnEnter called
2. `Fire("StartWalk")` → transitions to Walking, OnExit(Idle) then OnEnter(Walking) called
3. `Fire("StartRun")` → transitions to Running
4. `Fire("NonexistentTrigger")` → returns false, stays in current state

**Guards:**
5. Two transitions from Idle on same trigger, first guard returns false, second returns true → second wins
6. Both guards return false → no transition, returns false

**Wildcards:**
7. Wildcard transition `kAnyState → Dead` on "OnDie" — fires from Idle, Walking, and Running
8. State-specific transition for "OnDie" from Running → Injured — takes priority over wildcard

**Update:**
9. `Update(0.016f)` calls current state's OnUpdate with correct deltaTime and context

**Safety:**
10. Chain transitions (A fires B fires C...) — stops at max 8, asserts in debug
11. Transition history contains correct sequence after 5 transitions

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | All state/transition/trigger IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | DiaCore containers only |
| PD-007 | Platform | C++20 | Concept constraint on TContext |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-001 | System | Separate flat implementation | This is the flat type — no hierarchy overhead |
| SD-005 | System | Machine owns definition | Constructor takes `Definition&&` — move semantics |
| SD-003 | System | Raw function pointers | ActionFn, GuardFn, UpdateFn are all raw pointers |
| SD-007 | System | First-match guard semantics | Linear scan, first passing guard wins |
| SD-008 | System | Max transitions per frame | Counter resets at Update(), asserts at limit |
| SD-014 | System | Fire() returns bool, no exceptions | Returns false on no match |
| SD-015 | System | kAnyState wildcard | Evaluated after state-specific transitions |
