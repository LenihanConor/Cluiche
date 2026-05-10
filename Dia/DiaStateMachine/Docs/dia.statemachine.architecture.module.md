---
schema: dia.module.v1
module_id: dia.statemachine
name: DiaStateMachine
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaStateMachine
language: cpp
parent_module_id: dia.root

summary: >
  Generic state machine library — flat FSM, hierarchical state machine, and pushdown automaton
  with shared inspection interface, data-driven definitions, and transition tracing.

intent: >
  Provides reusable state machine infrastructure for gameplay, AI, animation, UI flows,
  and any system needing explicit stateful behavior. Two core implementations (flat and
  hierarchical) share a common IStateMachineInspectable interface for universal tooling.
  Definitions are constructed via fluent builders or loaded from JSON, moved into machines
  at construction (machine owns definition). Type-erased void* callbacks enable definition
  sharing across context types.

responsibilities:
  - FlatStateMachine with states, transitions, guards, wildcard (kAnyState)
  - HierarchicalStateMachine with nested states, inherited transitions, shallow history
  - PushdownAutomaton with push/pop stack semantics
  - StateMachineBuilder fluent APIs for all three types
  - StateMachineDefinition immutable topology objects
  - IStateMachineInspectable shared inspection interface
  - CallbackRegistry for data-driven JSON definitions
  - StateMachineTracer for transition logging via DiaLogger
  - StateMachineComponent IComponent wrapper
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Editor visualization (future DiaStateMachineEditor)
  - Application phase management (DiaApplicationFlow)
  - Animation blending or clip playback
  - Thread safety within callbacks
  - Network synchronization

dependent_modules: []

public_api:
  headers:
    - Dia/DiaStateMachine/IStateMachineInspectable.h
    - Dia/DiaStateMachine/FlatStateMachine.h
    - Dia/DiaStateMachine/StateMachineDefinition.h
    - Dia/DiaStateMachine/StateMachineBuilder.h
  namespaces:
    - Dia::StateMachine
  entry_points:
    - FlatStateMachine
    - StateMachineBuilder
    - StateMachineDefinition
    - IStateMachineInspectable

dependencies:
  required:
    - dia.core
    - dia.logger
  forbidden:
    - dia.graphics
    - dia.application
    - dia.maths
---
