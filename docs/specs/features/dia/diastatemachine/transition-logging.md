# Feature Spec: Transition Logging & Tracing

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **transition-logging** |

**Status:** `Approved`

---

## Problem Statement

State machines are notoriously hard to debug — the current state and recent transition path are invisible without instrumentation. When a machine enters an unexpected state, developers need to know: what state it was in, what trigger fired, which guard passed, and how long it spent in each state. Without structured logging, debugging relies on breakpoints and printf — which don't scale to dozens of machines running simultaneously.

---

## Solution Overview

A dedicated `StateMachine` DiaLogger channel with a `StateMachineTracer` wrapper. The tracer intercepts transitions on any `IStateMachineInspectable` and emits structured log entries. Verbosity is configurable per tracer instance (SD-012):

- **`kTransitionsOnly`** (default) — source state, target state, trigger ID, machine ID, timestamp
- **`kTransitionsAndGuards`** — adds guard names and pass/fail results for each evaluated guard
- **`kFull`** — adds transition duration, state residence time (how long the machine was in the previous state)

Rate limiting (max entries per second per tracer) prevents log flooding from high-frequency machines (e.g., hundreds of AI agents). Output is NDJSON for post-hoc analysis with standard tools.

Tracing is opt-in — unwrapped machines have zero logging overhead (SD-014 Q9).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `kLogChannel` is `StringCRC{"StateMachine"}` | Compile check |
| AC2 | `StateMachineTracer` wraps any `IStateMachineInspectable` | Unit test: wrap flat, HSM, pushdown |
| AC3 | Default verbosity is `kTransitionsOnly` | Unit test |
| AC4 | `SetVerbosity(kTransitionsOnly)` logs: source, target, trigger, machineId, timestamp | Unit test with mock sink |
| AC5 | `SetVerbosity(kTransitionsAndGuards)` additionally logs guard names and results | Unit test with mock sink |
| AC6 | `SetVerbosity(kFull)` additionally logs transition duration and state residence time | Unit test with mock sink |
| AC7 | `SetRateLimit(N)` — at most N entries per second emitted; excess silently dropped | Unit test |
| AC8 | `Enable()` / `Disable()` — disabled tracer emits nothing | Unit test |
| AC9 | `IsEnabled()` returns correct state | Unit test |
| AC10 | Unwrapped machine (no tracer) has zero logging overhead — no log calls | Code review |
| AC11 | Log output is valid NDJSON (one JSON object per line) | Unit test: parse output |
| AC12 | Logs emitted to `StateMachine` channel via DiaLogger | Unit test with channel filter |
| AC13 | Failed `Fire()` logged at `kDebug` level when tracer attached | Unit test |
| AC14 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::StateMachine {

    static constexpr Dia::Core::StringCRC kLogChannel{"StateMachine"};

    enum class TraceVerbosity {
        kTransitionsOnly,
        kTransitionsAndGuards,
        kFull
    };

    class StateMachineTracer {
    public:
        explicit StateMachineTracer(IStateMachineInspectable& machine);

        void SetVerbosity(TraceVerbosity verbosity);
        TraceVerbosity GetVerbosity() const;
        void SetRateLimit(int maxEntriesPerSecond);
        void Enable();
        void Disable();
        bool IsEnabled() const;
    };

} // namespace Dia::StateMachine
```

### NDJSON Output Examples

**kTransitionsOnly:**
```json
{"machineId":"EnemyAI","source":"Idle","target":"Chase","trigger":"OnSeePlayer","ts":12.345}
```

**kTransitionsAndGuards:**
```json
{"machineId":"EnemyAI","source":"Idle","target":"Chase","trigger":"OnSeePlayer","ts":12.345,"guards":[{"name":"CanSeePlayer","result":true},{"name":"HasAmmo","result":true}]}
```

**kFull:**
```json
{"machineId":"EnemyAI","source":"Idle","target":"Chase","trigger":"OnSeePlayer","ts":12.345,"guards":[{"name":"CanSeePlayer","result":true}],"transitionDuration":0.00002,"stateResidenceTime":3.456}
```

---

## Implementation Notes

- **Hook mechanism:** `StateMachineTracer` implements `ITransitionListener` and attaches itself via `machine.SetTransitionListener(this)`. Machines call `OnTransition(TransitionEvent)` after each successful transition and `OnTransitionFailed()` after each failed `Fire()`. When the listener pointer is null, machines skip all listener calls — zero overhead for untraced machines.
- Rate limiter: simple token bucket — one token per `1/maxEntriesPerSecond`. Refill checked on each `OnTransition` call. No thread safety needed (single-threaded machines).
- State residence time: tracer records timestamp on each `OnTransition`. Residence time = current `TransitionEvent.timestamp` - previous transition timestamp. Available in `TransitionEvent.stateResidenceTime`.
- NDJSON format uses DiaCore Json for serialization. StringCRC values serialized as their string representation (not CRC hash value) for human readability.
- Guard logging (`kTransitionsAndGuards`): `TransitionEvent` carries a `GuardEvaluation` array with guard names and pass/fail results. The machine populates this array on the stack during transition dispatch — valid only for the duration of the `OnTransition` callback.
- The tracer does NOT modify the machine's behavior — it's a passive observer implementing `ITransitionListener`. A traced machine behaves identically to an untraced machine.

---

## Dependencies

### Required Features
- **inspection-interface** — `IStateMachineInspectable`

### Required Modules
- **DiaCore** — `StringCRC`, `Json`
- **DiaLogger** — `Logger`, `ISink`, `LogLevel`, `LogEntry`

### Dependent Features
- None (leaf feature — consumed by editor tooling in future DiaStateMachineEditor)

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestTransitionLogging.cpp`)

Uses a mock `ISink` to capture log output.

1. Attach tracer to flat FSM — fire transition — sink receives NDJSON entry
2. Verify `kTransitionsOnly` output has: machineId, source, target, trigger, ts
3. Set `kTransitionsAndGuards` — verify guards array present
4. Set `kFull` — verify transitionDuration and stateResidenceTime present
5. `Disable()` — fire transition — sink receives nothing
6. `Enable()` after disable — resumes logging
7. Rate limit to 2/sec — fire 10 transitions rapidly — sink receives at most 2
8. Failed `Fire()` — sink receives kDebug entry with "no matching transition"
9. Wrap HSM — same logging behavior
10. Wrap PushdownAutomaton — push/pop logged as transitions
11. Parse all output as JSON — valid NDJSON

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | Channel, machine ID, state IDs all StringCRC |
| PD-004 | Platform | No STL in public APIs | No STL exposed |
| PD-009 | Platform | Output under Cluiche/out/ | NDJSON output files go under Cluiche/out/<AppName>/ |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-012 | System | Configurable verbosity | Three levels per instance |
| SD-014 | System | Fire() returns bool | Failed Fire() logged at kDebug, not warning |
