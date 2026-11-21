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
  - Named typed slot store (Register/Get/TryGet/Has/Unregister), local-only reads
  - Board name (GetName) and constructor slot cap (maxSlots, default 32; exceeding asserts)
  - Scopes - optional parent chain via SetParent/GetParent (refuses self, cycles and chains over 8 boards)
  - Chain reads - Lookup<T>, HasInChain, FindOwner (local first, then up the chain, type-checked)
  - Set<T> writes the local slot only and fires IBlackboardObserver::OnSlotChanged(key) on this board's observers
  - IBlackboardObserver lifecycle notifications (register/unregister) and defaulted OnSlotChanged
  - BlackboardComponent plain wrapper for entity attachment
  - GlobalBlackboard singleton for world-state (legacy, unused by new code)
  - DIA_LOG_INFO on slot register/unregister
  - BlackboardInspectorSource reporting name, parent and inherited slots with shadowed flags
  - Test utilities under Testing/ subdirectory

non_responsibilities:
  - Notifications for writes made through Get/TryGet references (only Set notifies)
  - Propagating parent slot changes to child board observers
  - Parent lifetime management (parent must outlive its children)
  - Serialisation of slot contents
  - Thread safety (SimPU use only)
  - Removing GlobalBlackboard (kept as debt)
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
