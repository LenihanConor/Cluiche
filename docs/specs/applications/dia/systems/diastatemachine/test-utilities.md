# Feature Spec: Test Utilities

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diastatemachine/diastatemachine.md | **test-utilities** |

**Status:** `Done`

---

## Problem Statement

State machines are highly testable — deterministic input/output, clear state queries, well-defined transition semantics. But writing test assertions against raw FSM API calls is verbose and repetitive: check current state, fire trigger, check new state, verify callback order. Without dedicated test helpers, every test suite that uses DiaStateMachine will reinvent the same assertion patterns.

---

## Solution Overview

A set of test helper utilities shipped inside the DiaStateMachine library under `DiaStateMachine/Testing/`. Consumers opt in via `#include <DiaStateMachine/Testing/...>`. This follows the platform-wide pattern (SD-017) of shipping test utilities with the library they test.

Provides:
- **`AssertInState(machine, stateId)`** — assert current state matches, with clear failure message
- **`FireAndExpect(machine, triggerId, expectedStateId)`** — fire + assert result in one call
- **`AssertTransitionSequence(machine, triggers[], expectedStates[])`** — fire a sequence and verify each resulting state
- **`TransitionRecorder`** — attaches to a machine and records all OnEnter/OnExit/OnUpdate callback invocations with ordering, for verifying callback sequences

These work with all machine types via `IStateMachineInspectable`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `AssertInState(machine, stateId)` passes when current state matches | Unit test |
| AC2 | `AssertInState(machine, wrongId)` fails with message containing expected and actual state IDs | Unit test |
| AC3 | `FireAndExpect(machine, trigger, expectedState)` fires and asserts result | Unit test |
| AC4 | `FireAndExpect` with wrong expected state — fails with descriptive message | Unit test |
| AC5 | `FireAndExpect` when trigger doesn't match — fails, reports "no transition" | Unit test |
| AC6 | `AssertTransitionSequence` with N triggers and N expected states — all pass | Unit test |
| AC7 | `AssertTransitionSequence` fails at first mismatch — reports which step failed | Unit test |
| AC8 | `TransitionRecorder` records OnEnter calls in order | Unit test |
| AC9 | `TransitionRecorder` records OnExit calls in order | Unit test |
| AC10 | `TransitionRecorder` records OnUpdate calls | Unit test |
| AC11 | `TransitionRecorder.GetSequence()` returns ordered list of (event, stateId) pairs | Unit test |
| AC12 | `TransitionRecorder.Clear()` resets recorded history | Unit test |
| AC13 | All utilities work with FlatStateMachine, HierarchicalStateMachine, and PushdownAutomaton | Unit test: one test per type |
| AC14 | Headers under `DiaStateMachine/Testing/` — not included by default library headers | Compile check |
| AC15 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
// DiaStateMachine/Testing/StateMachineTestHelpers.h
namespace Dia::StateMachine::Testing {

    // Assert current state matches expected
    void AssertInState(const IStateMachineInspectable& machine,
                       Dia::Core::StringCRC expectedStateId);

    // Fire trigger and assert resulting state
    template<typename TStateMachine>
    void FireAndExpect(TStateMachine& machine,
                       Dia::Core::StringCRC triggerId,
                       Dia::Core::StringCRC expectedStateId);

    // Fire a sequence of triggers and assert each resulting state
    template<typename TStateMachine>
    void AssertTransitionSequence(
        TStateMachine& machine,
        const Dia::Core::StringCRC* triggers,
        const Dia::Core::StringCRC* expectedStates,
        int count);

} // namespace Dia::StateMachine::Testing
```

```cpp
// DiaStateMachine/Testing/TransitionRecorder.h
namespace Dia::StateMachine::Testing {

    enum class RecordedEventType {
        kOnEnter,
        kOnExit,
        kOnUpdate,
        kOnPause,   // PushdownAutomaton only
        kOnResume   // PushdownAutomaton only
    };

    struct RecordedEvent {
        RecordedEventType type;
        Dia::Core::StringCRC stateId;
        int sequenceNumber;
    };

    class TransitionRecorder {
    public:
        void Record(RecordedEventType type, Dia::Core::StringCRC stateId);
        void GetSequence(
            Dia::Core::DynamicArrayC<RecordedEvent>& outEvents) const;
        int GetEventCount() const;
        void Clear();
    };

} // namespace Dia::StateMachine::Testing
```

---

## Implementation Notes

- `AssertInState` uses Google Test's `EXPECT_EQ` (or `ASSERT_EQ`) internally, with a custom message formatter that prints StringCRC names, not raw CRC values.
- `FireAndExpect` is a template to accept any machine type with a `Fire()` method. Falls back to `IStateMachineInspectable` for state query.
- `TransitionRecorder` is designed to be wired into state machine callbacks — game test code passes `recorder.Record(kOnEnter, stateId)` as the OnEnter action. A helper macro or builder extension could simplify this wiring.
- `AssertTransitionSequence` stops at the first failure and reports the step index, trigger that was fired, expected state, and actual state.
- All helpers use `Dia::Core::StringCRC` for identification — no raw strings in assertions.
- Headers are in `DiaStateMachine/Testing/` subdirectory — they are NOT included by `DiaStateMachine`'s main headers. Consumer must explicitly include them.

---

## Dependencies

### Required Features
- **inspection-interface** — `IStateMachineInspectable` for state queries

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`
- **GoogleTest** (external) — `EXPECT_EQ`, `ASSERT_EQ`, `FAIL` macros

### Dependent Features
- None (leaf feature — consumed by test suites)

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestTestUtilities.cpp`)

Meta-tests: tests that verify the test utilities themselves work correctly.

1. `AssertInState` on correct state — passes (no assertion failure)
2. `AssertInState` on wrong state — fails with descriptive message (test expects failure)
3. `FireAndExpect` success — fires and lands in expected state
4. `FireAndExpect` wrong state — fails with descriptive message
5. `FireAndExpect` trigger doesn't match — fails with "no transition"
6. `AssertTransitionSequence` — 4-step sequence, all correct — passes
7. `AssertTransitionSequence` — failure at step 2 — reports step 2
8. `TransitionRecorder` — record 3 events — `GetSequence()` returns them in order
9. `TransitionRecorder.Clear()` — sequence is empty after clear
10. All helpers work with flat FSM
11. All helpers work with HSM
12. All helpers work with PushdownAutomaton

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | All assertions use StringCRC |
| PD-004 | Platform | No STL in public APIs | DiaCore containers |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::Testing::` |
| SD-017 | System | Test utilities in library | Under `DiaStateMachine/Testing/`, opt-in include |
