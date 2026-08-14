# System Spec: DiaBehaviourTree

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaBehaviourTree is the data-driven behaviour tree evaluator for the Dia engine. It executes a tree of composable nodes — control-flow nodes (Sequence, Selector, Parallel), decorator wrappers, and leaf nodes (action, condition) — against a per-entity `Blackboard` and dispatches actions via `ActionFn` callbacks. Trees are defined in JSON, loaded at runtime as a shared `BehaviourTreeAsset`, and ticked per-entity by `BehaviourTreeSystem` within the `AIBudgetScheduler` time slice.

The key design property is **tree sharing**: many entities can run the same `BehaviourTreeAsset` with independent execution state in their `BehaviourTreeComponent`. Memory cost is proportional to entity count (execution cursor only), not tree complexity × entity count.

Evaluation is **time-sliced**: a `BehaviourTreeComponent` that returns `kRunning` from a node pauses at that node; the next `Update()` call resumes from where it left off. `AIBudgetScheduler` controls how many entities are ticked per frame.

**Research:** @docs/research/ai_gameplay_tools/ideate.md (Candidate 3)

**Dependency chain:**
`DiaBehaviourTree → DiaBlackboard (condition leaf reads bool slots by key)`
`DiaBehaviourTree → DiaAIBudget (BehaviourTreeSystem as IAIBudgetedSystem)`
`DiaBehaviourTree → DiaCore (StringCRC, DynamicArrayC, Json, Timer, IComponent, DIA_ASSERT)`

## Responsibilities

- Provide a `BehaviourTreeAsset` class: JSON-loadable, immutable after load, shared across entities
- Define node types: Sequence (all children succeed), Selector (first child succeeds), Parallel (configurable policy), Decorator (Inverter, Repeater, Cooldown, Guard), action leaf, condition leaf
- Provide an `ActionRegistry` for binding `StringCRC` action IDs to `ActionFn` callbacks — explicitly constructed, not a singleton
- Define `ActionFn` with `NodeResult` return (`kRunning` / `kSuccess` / `kFailure`) to support multi-tick actions
- Provide a `BehaviourTreeComponent` (`IComponent`) that binds a `BehaviourTreeAsset` + `Blackboard` + `ActionRegistry` to an entity and owns the execution cursor (per-entity running-node stack + decorator state)
- Provide a `BehaviourTreeSystem` implementing `IAIBudgetedSystem`: registered with `AIBudgetScheduler`, ticks registered components within the allocated budget per frame
- Support caller-driven ticking via `BehaviourTreeComponent::Tick()` without requiring `BehaviourTreeSystem`
- Provide `IDecoratorNode` + `DecoratorRegistry` as an extension interface for user-defined decorator types
- Provide `IBehaviourTreeEventListener` with `OnNodeEntered`, `OnNodeCompleted`, and `OnTreeCompleted` callbacks
- Load `BehaviourTreeAsset` from `Json::Value` (caller owns JSON parsing)
- Validate asset on load: no duplicate node IDs, root node exists, no cycles in tree
- Identify all node IDs and action/condition IDs via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide test utilities under `DiaBehaviourTree/Testing/`: `SpyAction`, `AssertNodeVisited`, `AssertLastResult`
- Provide `dia.diabehaviourtree.architecture.module.md` YAML module documentation
- Provide `DiaBehaviourTree.vcxproj` static library registered in `Cluiche.sln` under `3.0-Gameplay`

## Non-Responsibilities

- Executing all entities every frame — `BehaviourTreeSystem` time-slices across entities; some may defer to the next budget window
- Automatic tree switching or goal selection — caller changes the asset pointer on `BehaviourTreeComponent` and calls `Reset()`
- Blackboard slot registration — DiaBlackboard owns slot creation; BT reads existing slots
- Dispatching orders to `OrderQueue` directly — action `ActionFn` callbacks receive the caller's `actionContext` (void*) and push orders themselves; BT has no dependency on DiaOrder
- Visual debugging of tree state — future `DiaBehaviourTreeVisualDebugger`
- Behaviour authoring UI — future `DiaBehaviourTreeEditor` (separate spec)
- Thread-safe ticking — caller synchronises; `Tick()` and all callbacks run single-threaded on SimPU
- GOAP-style world-state simulation or HTN-style planning — BT evaluates a fixed tree structure against live blackboard state
- Hot-reload while entities are mid-execution — reload requires `Reset()` on all bound components

## Public Interfaces

### NodeResult

```cpp
namespace Dia::BehaviourTree {
    enum class NodeResult {
        kRunning,   // node is still executing — resume next tick
        kSuccess,   // node completed successfully
        kFailure    // node failed — parent handles propagation
    };
}
```

### ActionRegistry

```cpp
namespace Dia::BehaviourTree {
    // void* actionContext: caller's per-entity context (cast by action implementation)
    // const DynamicArrayC<StringCRC>& params: parameter bindings from JSON node definition
    using ActionFn = NodeResult(*)(void* actionContext,
                                   const Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& params);

    // Open handler table mapping StringCRC action names to ActionFn callbacks.
    // NOT a singleton — caller creates, owns, and passes by reference.
    class ActionRegistry {
    public:
        void Register(Dia::Core::StringCRC actionId, ActionFn fn);
        ActionFn Find(Dia::Core::StringCRC actionId) const;
        bool Has(Dia::Core::StringCRC actionId) const;
    };
}
```

### BehaviourTreeAsset

```cpp
namespace Dia::BehaviourTree {
    // Immutable after LoadFromJson. Shared across entities running the same tree.
    class BehaviourTreeAsset {
    public:
        static BehaviourTreeAsset LoadFromJson(
            const Json::Value& root,
            Dia::Core::DynamicArrayC<const char*>& outErrors);

        bool Validate(Dia::Core::DynamicArrayC<const char*>& outErrors) const;
        bool IsValid() const;

        Dia::Core::StringCRC GetRootNodeId() const;
        int GetNodeCount() const;
    };
}
```

### IBehaviourTreeEventListener

```cpp
namespace Dia::BehaviourTree {
    class IBehaviourTreeEventListener {
    public:
        virtual ~IBehaviourTreeEventListener() = default;
        virtual void OnNodeEntered(Dia::Core::StringCRC nodeId) = 0;
        virtual void OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result) = 0;
        virtual void OnTreeCompleted(NodeResult result) = 0;
    };
}
```

### IDecoratorNode

```cpp
namespace Dia::BehaviourTree {
    // Per-entity-per-node mutable state passed to decorator callbacks.
    // counter and accumulator are owned by BehaviourTreeComponent's execution cursor.
    struct DecoratorContext {
        float  deltaTime;    // seconds since last tick
        int&   counter;      // per-entity-per-node integer (repeat count, etc.)
        float& accumulator;  // per-entity-per-node float (cooldown timer, etc.)
    };

    // Extension interface for user-defined decorators.
    // Stateless implementation — per-entity mutable state accessed via DecoratorContext.
    class IDecoratorNode {
    public:
        virtual ~IDecoratorNode() = default;
        // Return false to skip the child and return kFailure immediately.
        virtual bool ShouldTickChild(const DecoratorContext& ctx) const = 0;
        // Transform the child's result. Called after child ticks.
        virtual NodeResult Evaluate(NodeResult childResult, DecoratorContext& ctx) const = 0;
        // Called when the owning component is Reset().
        virtual void OnReset(DecoratorContext& ctx) const = 0;
        virtual Dia::Core::StringCRC GetTypeId() const = 0;
    };

    // Registry for user-defined decorator types. Explicitly constructed, not a singleton.
    class DecoratorRegistry {
    public:
        void Register(Dia::Core::StringCRC typeId, const IDecoratorNode* decorator);
        const IDecoratorNode* Find(Dia::Core::StringCRC typeId) const;
    };
}
```

### BehaviourTreeComponent

```cpp
namespace Dia::BehaviourTree {
    class BehaviourTreeComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"BehaviourTreeComponent"};

        void SetAsset(const BehaviourTreeAsset* asset);           // shared; must outlive component
        void SetBlackboard(Dia::Blackboard::Blackboard* blackboard);
        void SetActionRegistry(const ActionRegistry* registry);
        void SetActionContext(void* context);                      // passed verbatim to ActionFn
        void SetDecoratorRegistry(const DecoratorRegistry* registry); // optional; for custom decorators

        void AddEventListener(IBehaviourTreeEventListener* listener);
        void RemoveEventListener(IBehaviourTreeEventListener* listener);

        // Advance evaluation by one node. Returns kRunning while mid-execution.
        // Caller-driven — BehaviourTreeSystem calls this within the budget window.
        NodeResult Tick(float deltaTime);

        // Restart evaluation from the root. Clears cursor and all decorator state.
        void Reset();

        bool IsComplete() const;          // root returned kSuccess or kFailure last tick
        NodeResult LastResult() const;
        bool HasAsset() const;
    };
}
```

### BehaviourTreeSystem

```cpp
namespace Dia::BehaviourTree {
    // Registered with AIBudgetScheduler. Ticks BehaviourTreeComponents within budget.
    class BehaviourTreeSystem : public Dia::AIBudget::IAIBudgetedSystem {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"BehaviourTreeSystem"};

        void Register(BehaviourTreeComponent* component);
        void Unregister(BehaviourTreeComponent* component);

        // IAIBudgetedSystem — called by AIBudgetScheduler each frame
        void Update(float budgetMs, float deltaTime) override;

        int GetRegisteredCount() const;
    };
}
```

### JSON Schema

```json
{
  "root": "Patrol",
  "nodes": {
    "Patrol": {
      "type": "selector",
      "children": ["AttackIfEnemy", "PatrolRoute"]
    },
    "AttackIfEnemy": {
      "type": "sequence",
      "children": ["EnemyVisible", "AttackTarget"]
    },
    "EnemyVisible": {
      "type": "condition",
      "blackboard_key": "enemy_visible"
    },
    "AttackTarget": {
      "type": "action",
      "action_id": "AttackOrder",
      "params": []
    },
    "PatrolRoute": {
      "type": "decorator",
      "decorator": "repeater",
      "repeat_count": 0,
      "break_on_failure": true,
      "child": "MoveToWaypoint"
    },
    "CooldownAbility": {
      "type": "decorator",
      "decorator": "cooldown",
      "cooldown_seconds": 2.0,
      "child": "FireAbility"
    },
    "InvertCheck": {
      "type": "decorator",
      "decorator": "inverter",
      "child": "IsIdle"
    },
    "FlankGuard": {
      "type": "decorator",
      "decorator": "guard",
      "blackboard_key": "flank_available",
      "child": "FlankManoeuvre"
    },
    "MultiTask": {
      "type": "parallel",
      "policy": "require_all",
      "children": ["MoveToTarget", "FaceTarget"]
    },
    "MoveToWaypoint": {
      "type": "action",
      "action_id": "MoveToOrder",
      "params": ["next_waypoint"]
    }
  }
}
```

Parallel `policy` values: `require_all` (succeed when all succeed; fail on first failure), `require_one` (succeed on first success; fail when all fail), `require_none` (always succeed; run all children to completion).

### Test Utilities

```cpp
// DiaBehaviourTree/Testing/BTTestHelpers.h
namespace Dia::BehaviourTree::Testing {
    // Spy action — records calls and returns a configurable NodeResult.
    class SpyAction {
    public:
        void SetResult(NodeResult result);
        int GetCallCount() const;
        const Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& LastParams() const;
        // Register this spy as an action in an ActionRegistry.
        void RegisterIn(ActionRegistry& registry, Dia::Core::StringCRC actionId);
    };

    // Assert that a specific node was visited (entered) during the most recent Tick() chain.
    void AssertNodeVisited(const BehaviourTreeComponent& component,
                           Dia::Core::StringCRC nodeId);

    // Assert the component's last completed result matches the expected value.
    void AssertLastResult(const BehaviourTreeComponent& component, NodeResult expected);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| BehaviourTreeAsset | JSON-loadable immutable tree definition. Shared across entities. Validates on load: no duplicate IDs, root exists, no cycles. | inline | Draft |
| Control-flow Nodes | Built-in Sequence (fail-fast, all children must succeed), Selector (first success wins), Parallel (configurable policy per-node: require_all / require_one / require_none). | inline | Draft |
| Decorator Nodes | Built-in Inverter (flip Success↔Failure), Repeater (N times or until failure), Cooldown (DiaCore/Timer gate), Guard (blackboard bool key gate). `IDecoratorNode` + `DecoratorRegistry` for custom types. Per-entity decorator state owned by `BehaviourTreeComponent` cursor. | inline | Draft |
| Leaf Nodes | Condition leaf (reads a blackboard bool slot by `StringCRC` key). Action leaf (invokes `ActionFn` via `ActionRegistry`; multi-tick via `kRunning`). | inline | Draft |
| BehaviourTreeComponent | Entity integration — binds asset + blackboard + action context + optional decorator registry; owns execution cursor and per-node decorator state. `Tick(deltaTime)` is caller-driven. `Reset()` restarts from root. | inline | Draft |
| BehaviourTreeSystem | `IAIBudgetedSystem` registered with `AIBudgetScheduler`. `Update(budgetMs, deltaTime)` ticks registered components until budget is exhausted. | inline | Draft |
| Event Listener | `IBehaviourTreeEventListener` — `OnNodeEntered`, `OnNodeCompleted`, `OnTreeCompleted`. Attached per-component; suitable for animation triggers and debug logging. | inline | Draft |
| Test Utilities | `DiaBehaviourTree/Testing/` — `SpyAction`, `AssertNodeVisited`, `AssertLastResult`. Ships with library; consumer opt-in via include. | inline | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaBlackboard** — `Blackboard` (condition leaf reads `bool` slots by `StringCRC` key)
- **DiaAIBudget** — `AIBudgetScheduler`, `IAIBudgetedSystem` (for `BehaviourTreeSystem` time-slicing)
- **DiaCore** — `StringCRC` (node/action/type IDs), `DynamicArrayC` (node children, error output), `Json` (asset loading), `IComponent` / `ComponentFactoryRegistry` (entity integration), `Timer` (Cooldown decorator), `DIA_ASSERT`

**Explicitly excluded:**
- **DiaOrder** — action `ActionFn` callbacks receive the caller's `actionContext` (void*) and push to `OrderQueue` themselves; BT has no direct dependency on DiaOrder
- **DiaCondition** — condition leaves read blackboard keys directly; shared condition expressions deferred to v2 if reuse across BT + DiaRules becomes painful
- **DiaRules** — peer system at the same decision layer; no bridge adapter in v1
- **DiaApplicationFlow** — no lifecycle dependency; `BehaviourTreeComponent::Tick()` is called from within a phase/module

**Dependents:**
- Game code (CluicheTest and future games) — registers actions, attaches `BehaviourTreeComponent`, optionally registers `BehaviourTreeSystem` with `AIBudgetScheduler`
- Future `DiaBehaviourTreeVisualDebugger` — reads execution cursor + last node results for debug overlay

## Out of Scope

- Automatic tree switching or goal arbitration — caller owns tree selection and calls `Reset()` on switch
- GOAP-style world-state simulation — BT evaluates a fixed structure against live blackboard state
- HTN-style planning — separate peer system; one entity can have both a `BehaviourTreeComponent` and an `HTNPlannerComponent`
- Visual debugging — future `DiaBehaviourTreeVisualDebugger`
- Authoring UI / node editor — future `DiaBehaviourTreeEditor`
- Thread-safe ticking — caller synchronises; all callbacks run single-threaded on SimPU
- Hot-reload while entities are mid-execution — `Reset()` required on reload
- Shared condition expressions with DiaRules via DiaCondition — deferred to v2 if needed

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Tri-state `NodeResult`: kRunning / kSuccess / kFailure | `kRunning` enables multi-tick action leaves and time-sliced evaluation without the caller tracking partial state externally. Consistent with `HTNPlanner::TaskResult` (DiaHTN). | All nodes | Accepted | Yes |
| SD-002 | Time-slicing via `BehaviourTreeSystem` registered with `AIBudgetScheduler` | Keeps all AI budget accounting in one place. `BehaviourTreeComponent::Tick()` is also available for caller-driven use. Consistent with DiaHTN async path (SD-008). | BehaviourTreeSystem | Accepted | Yes |
| SD-003 | Parallel policy is configurable per-node (require_all / require_one / require_none) | Hardcoding either semantic excludes valid use cases. Policy is a JSON field, not a subtype — keeps the node hierarchy flat. | Parallel node | Accepted | Yes |
| SD-004 | Four built-in decorators + `IDecoratorNode` extension interface; decorator state lives in `BehaviourTreeComponent` | Inverter/Repeater/Cooldown/Guard cover the common cases. Extension interface allows custom decorators without forking the module. State in the cursor (not the asset) enables tree sharing. | Decorator nodes | Accepted | Yes |
| SD-005 | `BehaviourTreeAsset` is immutable after `LoadFromJson()` | Shared safely across entities. All mutable per-entity state lives in `BehaviourTreeComponent`. Consistent with `HTNDomain` (DiaHTN SD-006) and `RuleSet` (DiaRules). | BehaviourTreeAsset | Accepted | Yes |
| SD-006 | `ActionRegistry` is explicitly constructed, not a singleton | Consistent with `OperatorRegistry` (DiaHTN SD-009), `RuleActionRegistry` (DiaRules SD-003), `ConditionRegistry` (DiaCondition SD-003). Zero hidden global state. | ActionRegistry | Accepted | Yes |
| SD-007 | Action leaves call `ActionFn` via `ActionRegistry`; BT passes `actionContext` (void*) through unchanged | Same pattern as HTN `OperatorFn` + `operatorContext`. Game code owns the context type; BT has no dependency on DiaOrder or any game type. | Action leaf | Accepted | Yes |
| SD-008 | Condition leaves read `Blackboard` bool slots by `StringCRC` key directly | Avoids DiaCondition dependency in v1. Covers the common case. Complex predicates can be pre-computed by DiaSensor/game code and written as blackboard keys. | Condition leaf | Accepted | Yes |
| SD-009 | Caller owns JSON parsing; `LoadFromJson` takes `Json::Value&` | Consistent across DiaStateMachine, DiaRules, DiaCondition, DiaUtilityAI, DiaHTN. | BehaviourTreeAsset | Accepted | Yes |
| SD-010 | Test utilities ship inside `DiaBehaviourTree/Testing/` | Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All node IDs, action IDs, and type IDs use `StringCRC`. `BehaviourTreeComponent::kUniqueId` and `BehaviourTreeSystem::kUniqueId` are `StringCRC` constants. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `BehaviourTreeSystem::Update()` runs on SimPU via `AIBudgetScheduler`. `BehaviourTreeComponent::Tick()` is called from within a phase/module by game code or `BehaviourTreeSystem`. DiaBehaviourTree has no lifecycle of its own. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal node child lists and evaluation stack may use STL. |
| PD-005 | Platform | x64 only | `DiaBehaviourTree.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaBehaviourTree.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaBehaviourTree.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diabehaviourtree.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal evaluation stack may use STL. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All types in `Dia::BehaviourTree::` namespace. |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for application structure | `BehaviourTreeComponent` is a proper `IComponent` — not a singleton or service. `BehaviourTreeSystem` is an explicit `IAIBudgetedSystem`, not a global. |

## Open Design Questions

1. **Condition leaf custom callbacks** — condition leaves currently read a single `bool` blackboard key. Complex predicates (e.g., "distance < 10") need either a custom `ConditionFn` callback (parallel to `ActionFn`) or to be pre-computed by DiaSensor/game code and written as blackboard keys. The first `BehaviourTreeTestStage` consumer will clarify which is needed.

2. **Parallel partial completion and cancellation** — when `require_one` fires on the first child success, still-running children are left in `kRunning` state until they complete naturally. If explicit cancellation is needed (e.g., cancel a move order when the attack succeeds first), a `CancelFn` callback alongside `ActionFn` would be the extension point. Defer until a concrete use case demands it.

3. **Multiple trees per entity** — `BehaviourTreeComponent` binds one tree per entity. Concurrent orthogonal trees (movement + attack) could be modelled as multiple components or as a Parallel root with subtree-reference nodes. Defer until `BehaviourTreeTestStage` reveals a concrete need.

## Status

**Status:** `Approved`
**Plan:** @docs/specs/applications/dia/systems/diabehaviourtree/diabehaviourtree.plan.md
