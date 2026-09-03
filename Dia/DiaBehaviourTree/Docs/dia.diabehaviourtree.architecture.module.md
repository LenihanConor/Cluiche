---
schema: dia.module.v1
module_id: dia.behaviourtree
name: DiaBehaviourTree
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaBehaviourTree
language: cpp
parent_module_id: dia.root

summary: >
  Data-driven behaviour tree evaluator. BehaviourTreeAsset (JSON-loadable, immutable, shared across entities),
  control-flow nodes (Sequence, Selector, Parallel with configurable policy), decorator nodes (Inverter,
  Repeater, Cooldown, Guard) + IDecoratorNode extension interface, action leaf (ActionFn via ActionRegistry,
  multi-tick kRunning), condition leaf (Blackboard bool slot by StringCRC key),
  BehaviourTreeComponent (IComponent, owns execution cursor and decorator state, caller-driven Tick()),
  BehaviourTreeSystem (ISimTimeBudgetedSystem, time-slices across entities within budget).

intent: >
  Provides a composable, time-sliced behaviour tree execution layer. Many entities share a single
  BehaviourTreeAsset; per-entity execution state lives entirely in BehaviourTreeComponent. Trees are
  defined in JSON, loaded at runtime, and ticked by BehaviourTreeSystem within the SimTimeBudget
  time slice. Action leaves dispatch via ActionRegistry callbacks; condition leaves read Blackboard
  bool slots directly.

responsibilities:
  - BehaviourTreeAsset — JSON-loadable immutable tree definition; validates on load (no duplicate IDs, root exists, no cycles)
  - Control-flow nodes — Sequence (fail-fast, all children succeed), Selector (first success wins), Parallel (require_all / require_one / require_none per node)
  - Decorator nodes — Inverter, Repeater, Cooldown (DiaCore/Timer gate), Guard (blackboard bool key gate); IDecoratorNode + DecoratorRegistry for custom types
  - Leaf nodes — condition leaf reads Blackboard bool slot by StringCRC key; action leaf invokes ActionFn via ActionRegistry with actionContext and params
  - BehaviourTreeComponent — IComponent binding asset + blackboard + action context + optional decorator registry; owns execution cursor and per-node decorator state; Tick(deltaTime) is caller-driven; Reset() restarts from root
  - BehaviourTreeSystem — ISimTimeBudgetedSystem registered with DiaSimTime (SimTimeBudget); ticks BehaviourTreeComponents within budget window
  - IBehaviourTreeEventListener — OnNodeEntered, OnNodeCompleted, OnTreeCompleted per-component callbacks
  - BehaviourTreeBusAdapter — forwards event-listener callbacks onto DiaMessageBus::Bus, entity-addressed via Dia::Entity::MakeEntityAddress (never Broadcast)
  - ActionRegistry — explicit handler table (StringCRC → ActionFn); not a singleton
  - DecoratorRegistry — explicit registry for custom IDecoratorNode types; not a singleton
  - Test utilities under DiaBehaviourTree/Testing/ — SpyAction, AssertNodeVisited, AssertLastResult

non_responsibilities:
  - Automatic tree switching or goal arbitration — caller owns tree selection and calls Reset()
  - GOAP-style world-state simulation or HTN-style planning — separate peer systems
  - Visual debugging — future DiaBehaviourTreeVisualDebugger
  - Behaviour authoring UI — future DiaBehaviourTreeEditor
  - Thread-safe ticking — caller synchronises; all callbacks run single-threaded on SimPU
  - Hot-reload while entities are mid-execution — Reset() required on reload
  - Blackboard slot registration — DiaBlackboard owns slot creation

dependent_modules:
  - dia.blackboard
  - dia.simtime
  - dia.core

public_api:
  headers:
    - Dia/DiaBehaviourTree/NodeResult.h
    - Dia/DiaBehaviourTree/ActionRegistry.h
    - Dia/DiaBehaviourTree/IDecoratorNode.h
    - Dia/DiaBehaviourTree/DecoratorRegistry.h
    - Dia/DiaBehaviourTree/BehaviourTreeAsset.h
    - Dia/DiaBehaviourTree/IBehaviourTreeEventListener.h
    - Dia/DiaBehaviourTree/BehaviourTreeComponent.h
    - Dia/DiaBehaviourTree/BehaviourTreeSystem.h
    - Dia/DiaBehaviourTree/Testing/BTTestHelpers.h
    - Dia/DiaBehaviourTree/BehaviourTreeBusAdapter.h
  namespaces:
    - Dia::BehaviourTree
    - Dia::BehaviourTree::Testing

dependencies:
  required:
    - dia.core
    - dia.blackboard
    - dia.simtime
  optional:
    - dia.messagebus
    - dia.entity
  forbidden:
    - dia.order
    - dia.rules
    - dia.condition
    - dia.applicationflow
    - dia.statemachine
    - dia.visualdebugger
---
