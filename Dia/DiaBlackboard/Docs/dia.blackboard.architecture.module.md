---
schema: dia.module.v1
module_id: dia.blackboard
name: DiaBlackboard
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaBlackboard
language: cpp
parent_module_id: dia.root

summary: >
  Named typed-blob store — per-entity and global blackboards with StringCRC keys,
  typed Register/Get/TryGet slot access, and Observer lifecycle notifications.

intent: >
  Provides a shared memory store for gameplay and AI systems. Each slot is a typed
  struct aggregate owned by the blackboard. Systems register their struct on init
  and unregister on shutdown, giving clean subsystem lifecycle management and
  zero field-collision risk between systems.

responsibilities:
  - Named typed slot store (Register/Get/TryGet/Has/Unregister)
  - IBlackboardObserver lifecycle notifications (register/unregister)
  - BlackboardComponent plain wrapper for entity attachment
  - GlobalBlackboard singleton for world-state
  - DIA_LOG_INFO on slot register/unregister
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Per-frame mutation notifications (poll-only)
  - Serialisation of slot contents
  - Thread safety
  - Visual debugger widget

dependent_modules: []

public_api:
  headers:
    - Dia/DiaBlackboard/Blackboard.h
    - Dia/DiaBlackboard/IBlackboardObserver.h
    - Dia/DiaBlackboard/BlackboardComponent.h
    - Dia/DiaBlackboard/GlobalBlackboard.h
  namespaces:
    - Dia::Blackboard
  entry_points:
    - Blackboard
    - IBlackboardObserver
    - BlackboardComponent
    - GlobalBlackboard

dependencies:
  required:
    - dia.core
    - dia.observation
  forbidden:
    - dia.statemachine
    - dia.streams
    - dia.graphics
    - dia.application
    - dia.maths
---
