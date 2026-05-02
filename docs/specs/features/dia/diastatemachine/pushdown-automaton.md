# Feature Spec: Pushdown Automaton

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **pushdown-automaton** |

**Status:** `Approved`

---

## Problem Statement

Some state flows need "interrupt and resume" semantics — pause menus overlaying gameplay, dialog interrupting exploration, tutorial prompts overlaying any state. These patterns require remembering what state to return to, and potentially stacking multiple interrupts. A flat FSM or HSM can model this with explicit "return" transitions, but it's awkward and error-prone. A stack naturally expresses the pattern.

---

## Solution Overview

`PushdownAutomaton<TContext>` is a stack-based state machine. `Push(stateId)` suspends the current state (calls OnPause) and enters the new state (calls OnEnter). `Pop()` exits the top state (calls OnExit) and resumes the previous state (calls OnResume). The machine is always in the state at the top of the stack.

Constructed from a `PushdownAutomatonDefinition&&` (move semantics — machine owns its definition) and a `TContext&` reference — consistent with the Flat and HSM pattern (SD-005). Each state in the definition has OnEnter, OnExit, OnUpdate, OnPause (called when pushed over), and OnResume (called when popped back to) actions. The definition is built via a `PushdownAutomatonBuilder` or loaded from JSON via `CallbackRegistry`.

Stack depth is bounded (default 16, SD-010) to prevent push-without-pop bugs.

Implements `IStateMachineInspectable`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Construct from `PushdownAutomatonDefinition` + context — enters initial state, OnEnter called, stack depth = 1 | Unit test |
| AC2 | `Push(stateId)` — OnPause on current, OnEnter on new, stack depth increments | Unit test |
| AC3 | `Pop()` — OnExit on current, OnResume on previous, stack depth decrements | Unit test |
| AC4 | `Pop()` on stack depth 1 — returns false, no state change (can't pop initial) | Unit test |
| AC5 | `Push()` at max stack depth — DIA_ASSERT in debug, returns false in release | Unit test |
| AC6 | `GetCurrentStateId()` returns top-of-stack state | Unit test |
| AC7 | `GetStackDepth()` returns correct value after push/pop sequences | Unit test |
| AC8 | `Update(deltaTime)` calls OnUpdate on top-of-stack state only | Unit test |
| AC9 | Push/Pop sequence: A → push B → push C → pop → B resumes → pop → A resumes | Unit test with callback tracking |
| AC10 | Transition history records push and pop operations | Unit test via `GetTransitionHistory()` |
| AC11 | `GetAllStates()` returns all registered states; only top-of-stack is active | Unit test |
| AC12 | Custom max stack depth (e.g., 4) — overflow at 4, not 16 | Unit test |
| AC13 | No heap allocation during Push/Pop/Update | Code review |
| AC14 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::StateMachine {

    template<typename TContext>
    class PushdownAutomaton : public IStateMachineInspectable {
    public:
        using ActionFn = void(*)(TContext&);
        using UpdateFn = void(*)(TContext&, float deltaTime);

        explicit PushdownAutomaton(Dia::Core::StringCRC machineId,
                                   PushdownAutomatonDefinition&& definition,
                                   TContext& context,
                                   int maxStackDepth = 16);

        // Stack operations
        bool Push(Dia::Core::StringCRC stateId);
        bool Pop();
        Dia::Core::StringCRC GetCurrentStateId() const override;
        int GetStackDepth() const;

        void Update(float deltaTime);

        // IStateMachineInspectable
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

- Stack stored as a fixed-size array of state ordinals (not pointers). `maxStackDepth` determines array size at construction.
- Constructed from `PushdownAutomatonDefinition&&` — consistent with Flat/HSM pattern. Machine owns its definition after move. Definition contains all state entries with OnEnter/OnExit/OnUpdate/OnPause/OnResume callbacks. Built via `PushdownAutomatonBuilder` or loaded from JSON. Use `Clone()` for explicit sharing.
- `OnPause` and `OnResume` are unique to PushdownAutomaton — flat FSM and HSM don't have these concepts. JSON schema adds `"onPause"` and `"onResume"` callback name fields.
- `GetAllTransitions()` for inspection returns an empty set — pushdown automata don't have predefined transitions; push/pop are explicit operations.
- Transition history records pushes as transitions (source = previous top, target = pushed state) and pops as transitions (source = popped state, target = resumed state).
- `Pop()` on depth 1 is not an error — it's a common "try to go back" pattern. Returns `false` silently.
- `Push()` of the same state that's already on top is allowed (stack-based recursion).
- **OnUpdate is called on top-of-stack state only.** Paused states are truly paused — no processing. If background updates are needed, use a separate machine.

---

## Dependencies

### Required Features
- **inspection-interface** — `IStateMachineInspectable`
- **state-machine-builder** — `PushdownAutomatonDefinition` (built via `PushdownAutomatonBuilder`)

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`

### Dependent Features
- **state-machine-component** — wraps PushdownAutomaton instances
- **transition-logging** — traces via `IStateMachineInspectable`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestPushdownAutomaton.cpp`)

1. Construct + Start — OnEnter called, depth = 1, current = initial state
2. Push state B — OnPause(A), OnEnter(B), depth = 2
3. Pop — OnExit(B), OnResume(A), depth = 1
4. Pop at depth 1 — returns false, no callbacks fired
5. Push to max depth — DIA_ASSERT on next push
6. Custom max depth 4 — overflow at 4
7. Full push/pop cycle: A → B → C → pop → pop → A with all callbacks in correct order
8. Update calls OnUpdate on top state only (B paused, A at bottom — neither gets Update)
9. Push same state twice — stack has [A, B, B], depth = 3
10. Transition history after push/pop sequence — correct records

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | All state IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | DiaCore containers |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-003 | System | Raw function pointers | All callbacks are raw pointers |
| SD-005 | System | Machine owns definition | Constructor takes `Definition&&` — move semantics |
| SD-010 | System | Bounded stack depth | Fixed max, DIA_ASSERT on overflow |
| SD-014 | System | Returns bool, no exceptions | Pop returns false at depth 1 |
