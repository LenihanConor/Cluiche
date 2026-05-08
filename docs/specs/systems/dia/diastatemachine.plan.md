# Plan: DiaStateMachine

**Spec:** @docs/specs/systems/dia/diastatemachine.md
**Status:** In Progress (core complete, JSON loader deferred)
**Started:** 2026-05-01
**Last Updated:** 2026-05-01

## Implementation Patterns

### Project Structure
```
Dia/DiaStateMachine/
  DiaStateMachine.vcxproj
  DiaStateMachine.vcxproj.filters
  Docs/dia.statemachine.architecture.module.md
  IStateMachineInspectable.h       # Interface + data structs
  ITransitionListener.h            # Listener interface + TransitionEvent
  StateMachineDefinition.h/.cpp    # Flat definition + StateDef/TransitionDef
  StateMachineBuilder.h/.cpp       # Fluent builder for flat definitions
  HierarchicalStateMachineDefinition.h/.cpp
  HierarchicalStateMachineBuilder.h/.cpp
  PushdownAutomatonDefinition.h/.cpp
  PushdownAutomatonBuilder.h/.cpp
  FlatStateMachine.h               # Template — header-only
  HierarchicalStateMachine.h       # Template — header-only
  PushdownAutomaton.h              # Template — header-only
  CallbackRegistry.h/.cpp          # Name→pointer resolution for JSON loading
  DataDrivenLoader.h/.cpp          # LoadFromJson/LoadFromFile implementations
  StateMachineTracer.h/.cpp        # ITransitionListener impl with DiaLogger
  StateMachineComponent.h/.cpp     # IComponent wrapper
  Testing/
    StateMachineTestHelpers.h      # AssertInState, FireAndExpect, AssertTransitionSequence
    TransitionRecorder.h/.cpp      # Records callback invocations
```

### Key Conventions
- **Namespace:** `Dia::StateMachine::` (AD-003)
- **IDs:** All `Dia::Core::StringCRC` (PD-001)
- **Containers:** `Dia::Core::DynamicArrayC`, `Dia::Core::HashTable` — no STL in public APIs (PD-004)
- **Callbacks:** Type-erased `void(*)(void*)`, `bool(*)(const void*)`, `void(*)(void*, float)` (SD-019)
- **Ownership:** Machine takes `Definition&&` — move semantics. `Clone()` for explicit sharing (SD-005)
- **Template machines:** Header-only (FlatStateMachine, HSM, PushdownAutomaton are templates on TContext)
- **Non-template types:** .h/.cpp split (definitions, builders, registry, tracer, component)
- **Include pattern:** `#include "DiaCore/CRC/StringCRC.h"` (relative to Dia/ parent)
- **Ring buffer:** Fixed-size array with head/tail for transition history (SD-006)
- **Error model:** `Fire()` returns bool, DIA_ASSERT for programmer errors, no exceptions (SD-014)

### Dependency Order
```
inspection-interface ─┐
                      ├─ flat-state-machine
state-machine-builder ┤
                      ├─ hierarchical-state-machine
                      ├─ pushdown-automaton
                      ├─ data-driven-definitions
                      │
inspection-interface ──── transition-logging
                      │
all machine types ─────── state-machine-component
inspection-interface ──── test-utilities
```

## Tasks

| # | Task | Spec | Status | Notes |
|---|------|------|--------|-------|
| 1 | Project scaffolding: vcxproj, filters, sln entry, module doc, GoogleTests integration | - | Done | GUID: {E3F4A5B6-C7D8-9012-EFAB-334455667788} |
| 2 | Inspection Interface: IStateMachineInspectable, StateInfo, TransitionInfo, TransitionRecord, ITransitionListener, TransitionEvent, GuardEvaluation | inspection-interface | Done | Pure headers, no .cpp needed |
| 3 | State Machine Builder: StateMachineDefinition, StateMachineBuilder, HierarchicalStateMachineDefinition, HierarchicalStateMachineBuilder, PushdownAutomatonDefinition, PushdownAutomatonBuilder, validation | state-machine-builder | Done | All 3 builder/definition pairs implemented |
| 4 | Flat State Machine: FlatStateMachine<TContext> template | flat-state-machine | Done | 12 tests passing |
| 5 | Hierarchical State Machine: HierarchicalStateMachine<TContext> template with LCA, history | hierarchical-state-machine | Done | 9 tests passing |
| 6 | Pushdown Automaton: PushdownAutomaton<TContext> template | pushdown-automaton | Done | 8 tests passing |
| 7 | Data-Driven Definitions: CallbackRegistry | data-driven-definitions | Done | CallbackRegistry done; JSON loading removed — consumers (AI, animation, phases) own their own JSON parsing and use builders |
| 8 | Transition Logging: StateMachineTracer | transition-logging | Done | NDJSON output, 3 verbosity levels, rate limiting |
| 9 | State Machine Component: StateMachineComponent IComponent wrapper | state-machine-component | Done | Type-erased pointer with MachineType tag |
| 10 | Test Utilities: AssertInState, FireAndExpect, TransitionRecorder | test-utilities | Done | Ships in DiaStateMachine/Testing/ |
| 11 | Unit tests for all features | all | Done | 29 tests passing (12 flat + 9 HSM + 8 PDA) |
| 12 | Build verification: full solution Debug + Release x64 | all | Done | Both configs pass, 29/29 tests |

## Session Notes

### 2026-05-01
- Plan created. Starting with project scaffolding (task 1), then core types (tasks 2-3), then machines (tasks 4-6).
- All core implementation complete. 29 tests passing across all 3 machine types.
- CallbackRegistry implemented. DataDrivenLoader (JSON) removed — JSON parsing will be done at the consumer level (AI, animation, phases) using the builders.
- StateMachineTracer logs NDJSON to "StateMachine" DiaLogger channel.
- StateMachineComponent wraps any machine type via type-erased pointer + MachineType enum.
- Test utilities (AssertInState, FireAndExpect, AssertTransitionSequence, TransitionRecorder) in Testing/ dir.
