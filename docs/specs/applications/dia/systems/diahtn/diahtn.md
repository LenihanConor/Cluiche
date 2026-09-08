# System Spec: DiaHTN

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaHTN is the Hierarchical Task Network planner for the Dia engine. It decomposes high-level goals into ordered sequences of primitive tasks by recursively expanding compound tasks through a data-driven domain definition. The planner runs against an `IConditionContext` world state snapshot, produces a flat plan (sequence of operator calls), and re-plans when the executing plan diverges from expectations.

HTNs are particularly suited to RTS/strategy AI because they naturally express multi-step intent: "to capture a base — scout it, assemble army, move to staging point, attack, hold." The domain (methods + operators) is defined in JSON; C++ code only needs to register operator callbacks.

The system has four independent parts that compose:
- **`HTNDomain`** — the loaded definition: compound task methods (ordered collections of sub-tasks + preconditions) and primitive task operator bindings
- **`HTNPlanner`** — stateless depth-first forward-chaining planner; given a domain + goal task + `IConditionContext`, produces an `HTNPlan`
- **`HTNPlan`** — the executable result: ordered flat list of `HTNTask` (operator ID + parameter bindings); tracks execution position
- **`OperatorRegistry`** — maps `StringCRC` operator IDs to `OperatorFn` callbacks with `TaskResult` (kRunning / kSucceeded / kFailed) return; owns the multi-tick operator lifecycle
- **`RegisterRuleActionAsOperator()`** — bridge adapter: wraps a `RuleActionFn` as an instant-succeed operator so DiaRules actions can be reused without changes to either system

DiaHTN is deliberately a planning library, not a runtime execution framework. It produces a plan; the caller drives plan execution tick by tick and decides when to re-plan.

**Research:** @docs/research/ai_gameplay_tools/ideate.md (Candidate 4)

**Dependency chain:**
`DiaHTN → DiaCondition (IConditionContext, ConditionExpr)`
`DiaHTN → DiaRules (RuleActionRegistry, RuleActionFn — for bridge adapter only)`
`DiaHTN → DiaCore (StringCRC, DynamicArrayC, Json, DIA_ASSERT)`

## Responsibilities

- Provide an `HTNDomain` class: JSON-loadable collection of compound task method definitions and primitive task operator ID declarations
- Provide an `HTNPlanner` class: stateless depth-first forward-chaining planner with ordered method selection and precondition evaluation via `IConditionContext`
- Provide an `HTNPlan` value type: ordered flat list of `HTNTask` (operator `StringCRC` + `DynamicArrayC<StringCRC>` parameter bindings), execution cursor, and divergence-check snapshot
- Provide an `OperatorRegistry` for binding `StringCRC` operator IDs to `OperatorFn` callbacks — explicitly constructed, not a singleton
- Define `OperatorFn` with `TaskResult` return (`kRunning` / `kSucceeded` / `kFailed`) to support multi-tick operators
- Provide a `RegisterRuleActionAsOperator()` free function: wraps a `RuleActionFn` from DiaRules as an instant-succeed `OperatorFn` in an `OperatorRegistry`
- Provide a `HTNPlannerComponent` (`IComponent`) for attaching a domain + registry + active plan to a `DiaEntity`; caller drives tick
- Support both sync planning (`HTNPlanner::Plan()`) and async planning via `AIBudgetScheduler` submission (`HTNPlanner::PlanAsync()`) — both paths produce the same `HTNPlan`
- Expose a divergence check (`HTNPlan::HasDiverged(IConditionContext&)`) that tests whether the world state has moved outside the snapshot bounds assumed when the plan was built; callers use this to trigger re-planning
- Provide explicit caller-controlled re-plan trigger in the API (no automatic re-planning inside the library)
- Load `HTNDomain` from `Json::Value` (caller owns JSON parsing)
- Validate domain on load: no duplicate task IDs, all method sub-tasks reference known task IDs, no cycles in the compound task graph
- Provide test utilities under `DiaHTN/Testing/`: `MockHTNContext`, `AssertPlanEquals`, `AssertPlanFails`, task/method builder helpers
- Identify all task IDs, operator IDs, and method IDs via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.diahtn.architecture.module.md` YAML module documentation
- Provide `DiaHTN.vcxproj` static library registered in `Cluiche.sln` under `3.0-Gameplay`

## Non-Responsibilities

- Executing operator callbacks each tick — `HTNPlannerComponent` provides a helper, but plan execution is caller-driven
- Automatic re-planning on divergence detection — callers detect divergence and call `Plan()` or `PlanAsync()`; no internal re-plan loop
- Goal priority ordering or multi-goal management — caller selects the active goal and re-invokes the planner
- Blackboard slot registration or world state writing — DiaBlackboard + game code
- State machine integration — if HTN output needs to drive DiaStateMachine transitions, the caller wires that (same as `ConditionGuardAdapter` pattern)
- Behaviour tree nodes or GOAP operators — separate systems
- Visual debugging of plan state — future `DiaHTNVisualDebugger` module (same pattern as `DiaUtilityAIVisualDebugger`)
- Thread-safe planning or execution — caller synchronises; `Plan()` and operator callbacks are single-threaded

## Public Interfaces

### TaskResult

```cpp
namespace Dia::HTN {
    enum class TaskResult {
        kRunning,    // operator is still executing — call again next tick
        kSucceeded,  // operator completed successfully — advance plan cursor
        kFailed      // operator failed — caller should re-plan
    };
}
```

### OperatorRegistry

```cpp
namespace Dia::HTN {
    // void* operatorContext: caller's per-entity context (cast by operator implementation)
    // const DynamicArrayC<StringCRC>& params: parameter bindings from the HTNPlan task entry
    using OperatorFn = TaskResult(*)(void* operatorContext,
                                     const Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& params);

    // Open handler table mapping StringCRC operator names to OperatorFn callbacks.
    // NOT a singleton — caller creates, owns, and passes by reference.
    class OperatorRegistry {
    public:
        void Register(Dia::Core::StringCRC operatorId, OperatorFn fn);
        OperatorFn Find(Dia::Core::StringCRC operatorId) const;
        bool Has(Dia::Core::StringCRC operatorId) const;
    };
}
```

### Bridge Adapter

```cpp
namespace Dia::HTN {
    // Wrap a DiaRules RuleActionFn as an instant-succeed OperatorFn.
    // The RuleActionFn receives the operatorContext pointer as its void* data.
    // Registered actions always return kSucceeded — use for fire-and-forget operators.
    void RegisterRuleActionAsOperator(Dia::Core::StringCRC operatorId,
                                      Dia::Rules::RuleActionFn action,
                                      OperatorRegistry& registry);
}
```

### HTNDomain

```cpp
namespace Dia::HTN {
    // Immutable after LoadFromJson. Shared across entities running the same domain.
    class HTNDomain {
    public:
        // Load from JSON. Returns an invalid domain and populates errors on failure.
        static HTNDomain LoadFromJson(
            const Json::Value& root,
            Dia::Core::DynamicArrayC<const char*>& outErrors);

        // Validate: no duplicate IDs, all sub-tasks resolvable, no cycles.
        bool Validate(Dia::Core::DynamicArrayC<const char*>& outErrors) const;

        bool IsValid() const;

        // Task type queries (used by HTNPlanner internally).
        bool IsCompound(Dia::Core::StringCRC taskId) const;
        bool IsPrimitive(Dia::Core::StringCRC taskId) const;
    };
}
```

### HTNPlan

```cpp
namespace Dia::HTN {
    struct HTNTask {
        Dia::Core::StringCRC operatorId;
        Dia::Core::DynamicArrayC<Dia::Core::StringCRC> params;
    };

    class HTNPlan {
    public:
        bool IsEmpty() const;
        bool IsComplete() const;   // cursor past last task

        // Current task under execution. Undefined if IsComplete().
        const HTNTask& CurrentTask() const;

        // Advance cursor after kSucceeded. No-op if complete.
        void Advance();

        // Test whether world state has diverged from the snapshot taken at plan-build time.
        // Callers check this each tick and re-plan on true.
        bool HasDiverged(Dia::Condition::IConditionContext& ctx) const;

        int GetTaskCount() const;
    };
}
```

### HTNPlanner

```cpp
namespace Dia::HTN {
    using PlanResultCallback = void(*)(HTNPlan plan, void* userData);

    class HTNPlanner {
    public:
        // Sync planning. Returns a valid plan or an empty plan on failure.
        // Takes a snapshot of ctx at plan time for later HasDiverged() checks.
        HTNPlan Plan(Dia::Core::StringCRC rootTask,
                     const HTNDomain& domain,
                     Dia::Condition::IConditionContext& ctx) const;

        // Async planning — submits work to AIBudgetScheduler.
        // Callback fires when the scheduler drains the work item.
        // Returns false if submission fails (scheduler at capacity).
        bool PlanAsync(Dia::Core::StringCRC rootTask,
                       const HTNDomain& domain,
                       Dia::Condition::IConditionContext& ctx,
                       Dia::AIBudget::AIBudgetScheduler& scheduler,
                       PlanResultCallback callback,
                       void* callbackUserData) const;
    };
}
```

### HTNPlannerComponent

```cpp
namespace Dia::HTN {
    class HTNPlannerComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"HTNPlannerComponent"};

        void SetDomain(const HTNDomain* domain);       // shared; must outlive component
        void SetRegistry(const OperatorRegistry* registry);
        void SetRootTask(Dia::Core::StringCRC taskId);

        // Re-plan synchronously; replaces active plan.
        void Replan(Dia::Condition::IConditionContext& ctx);

        // Re-plan asynchronously; active plan continues until callback fires.
        void ReplanAsync(Dia::Condition::IConditionContext& ctx,
                         Dia::AIBudget::AIBudgetScheduler& scheduler);

        // Tick the active plan: calls current operator, advances on kSucceeded,
        // returns kFailed if operator fails (caller decides whether to re-plan).
        // No-op and returns kSucceeded if plan is complete or not set.
        TaskResult Tick(void* operatorContext);

        // Check divergence and optionally trigger re-plan.
        bool HasDiverged(Dia::Condition::IConditionContext& ctx) const;

        const HTNPlan* GetActivePlan() const;
        bool HasActivePlan() const;
    };
}
```

### JSON Schema

```json
{
  "tasks": {
    "CaptureBase": {
      "type": "compound",
      "methods": [
        {
          "id": "CaptureBase_ArmyReady",
          "precondition": {
            "op": "and", "conditions": [
              { "slot": "army", "field": "size", "op": ">=", "value": 5.0 },
              { "slot": "base", "field": "scouted", "op": "==", "value": true }
            ]
          },
          "subtasks": ["MoveToStaging", "Attack", "Hold"]
        },
        {
          "id": "CaptureBase_Scout",
          "precondition": {
            "slot": "base", "field": "scouted", "op": "==", "value": false
          },
          "subtasks": ["SendScout", "CaptureBase"]
        }
      ]
    },
    "MoveToStaging": {
      "type": "primitive",
      "operator": "MoveToPosition",
      "params": ["staging_point"]
    },
    "Attack": {
      "type": "primitive",
      "operator": "AttackTarget",
      "params": []
    },
    "Hold": {
      "type": "primitive",
      "operator": "HoldPosition",
      "params": []
    },
    "SendScout": {
      "type": "primitive",
      "operator": "SpawnScout",
      "params": []
    }
  }
}
```

### Test Utilities

```cpp
// DiaHTN/Testing/HTNTestHelpers.h
namespace Dia::HTN::Testing {
    // Simple mock context for tests.
    // Same interface as Dia::Condition::Testing::MockConditionContext.
    class MockHTNContext : public Dia::Condition::IConditionContext {
    public:
        void SetFloat(Dia::Core::StringCRC slot,
                      Dia::Core::StringCRC field, float value);
        void SetBool (Dia::Core::StringCRC slot,
                      Dia::Core::StringCRC field, bool  value);
        float GetFloat(Dia::Core::StringCRC slot,
                       Dia::Core::StringCRC field) const override;
        bool  GetBool (Dia::Core::StringCRC slot,
                       Dia::Core::StringCRC field) const override;
    };

    // Assert a plan produces a specific ordered sequence of operator IDs.
    void AssertPlanEquals(const HTNPlan& plan,
                          std::initializer_list<Dia::Core::StringCRC> expectedOperators);

    // Assert planning fails (returns empty plan) for a given root task.
    void AssertPlanFails(const HTNPlanner& planner,
                         Dia::Core::StringCRC rootTask,
                         const HTNDomain& domain,
                         Dia::Condition::IConditionContext& ctx);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Operator Registry | `OperatorRegistry` — maps StringCRC operator names to multi-tick `OperatorBinding` callables. Explicitly constructed, not a singleton. | inline | Done |
| Rule Action Bridge | `RegisterRuleActionAsOperator()` — wraps a `RuleActionFn` as an instant-succeed `OperatorBinding` via a capture-bearing fat struct. Zero globals, zero statics. | inline | Done |
| HTN Domain | `HTNDomain` — JSON-loadable compound + primitive task definition. Validates no cycles, no dangling sub-task references. Immutable after load. | inline | Done |
| HTN Plan | `HTNPlan` — ordered flat operator sequence with execution cursor and world-state divergence check. | inline | Done |
| Sync Planner | `HTNPlanner::Plan()` — stateless depth-first forward-chaining planner. Takes world-state snapshot for divergence detection. Returns empty plan on failure. | inline | Done |
| Async Planning | `HTNPlanner::PlanAsync()` — submits heap-allocated self-deleting work item to `AIBudgetScheduler`; result delivered via callback. No static storage. | inline | Done |
| HTN Planner Component | `HTNPlannerComponent` (`IComponent`) — attaches domain + registry + active plan to a `DiaEntity`. Caller drives `Tick()` each frame. | inline | Done |
| Test Utilities | `DiaHTN/Testing/` — `MockHTNContext`, `AssertPlanOperators`, `AssertPlanFails`. Ships with library; consumer opt-in via include. | inline | Done |

## Dependencies on Other Systems

**Required:**
- **DiaCondition** — `IConditionContext` (world state evaluation for method preconditions and divergence checks), `ConditionExpr` (precondition expressions in JSON methods)
- **DiaCore** — `StringCRC` (task/operator/method IDs), `DynamicArrayC` (plan task list, error output), `Json` (domain loading), `IComponent` / `ComponentFactoryRegistry` (entity integration), `DIA_ASSERT`
- **DiaAIBudget** — `AIBudgetScheduler` (async planning path)

**Optional (bridge adapter only):**
- **DiaRules** — `RuleActionFn` type (for `RegisterRuleActionAsOperator()` only); DiaHTN compiles without this if the bridge header is not included

**Explicitly excluded:**
- **DiaBlackboard** — HTN reads world state via `IConditionContext`; `BlackboardConditionContext` is one concrete implementation but not a dependency
- **DiaApplicationFlow** — no lifecycle dependency; `HTNPlannerComponent::Tick()` is called by caller code within a phase/module
- **DiaStateMachine** — no direct dependency; if HTN output needs to drive FSM transitions, caller wires that bridge

**Dependents:**
- Game code (CluicheTest and future games) — registers operators, attaches `HTNPlannerComponent`, drives `Tick()` and divergence checks
- Future `DiaHTNVisualDebugger` — reads active plan + last evaluation from component for debug overlay

## Out of Scope

- Automatic re-planning — divergence detection is exposed; re-planning is always caller-triggered
- Goal priority ordering or multi-goal management — caller selects the active root task
- Partial-order planning or GOAP-style graph search — HTN is strictly decomposition-based
- World state effects (STRIPS-style `add`/`delete` lists) — HTN plans against a live `IConditionContext`, not a simulated state delta; expected effects are captured as the divergence snapshot
- Behaviour trees — separate future system
- Visual debugging — future `DiaHTNVisualDebugger` (same pattern as `DiaUtilityAIVisualDebugger`)
- Thread-safe planning or execution — caller synchronises; `Plan()` and all callbacks are single-threaded
- Hot-reload of domain JSON while planning is active — reload on next construct

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | `HTNPlanner` is stateless; plan state lives in `HTNPlan` | Consistent with `RuleSet` (DiaRules SD-005) and `ConditionExpr` (DiaCondition SD-004). One `HTNPlanner` instance can plan for any number of entities without shared mutable state. | HTN Planner | Accepted | Yes |
| SD-002 | `OperatorFn` returns `TaskResult` (kRunning/kSucceeded/kFailed), not `void` | Operators can be multi-tick (move-to-position takes N frames). `RuleActionFn` is void because rules are fire-and-forget. HTN operators need lifecycle. | Operator Registry | Accepted | Yes |
| SD-003 | Bridge adapter wraps `RuleActionFn` as instant-succeed `OperatorFn`, not the reverse | DiaRules is lower-level (no knowledge of TaskResult or HTNPlan). The adapter lives in the higher-level system (DiaHTN). Same pattern as `ConditionGuardAdapter` in DiaCondition (SD-008). | Rule Action Bridge | Accepted | Yes |
| SD-004 | Re-planning is always caller-triggered; no automatic re-plan loop inside DiaHTN | Avoids surprise mid-frame re-plans. Callers call `HasDiverged()` at their chosen frequency and re-plan on their schedule. Consistent with DiaUtilityAI's async path — the caller controls when work is submitted. | HTN Plan | Accepted | Yes |
| SD-005 | Divergence detection uses a world-state snapshot taken at plan time | Snapshot captures the float/bool values that method preconditions relied on. `HasDiverged()` re-evaluates those values and returns true if any have crossed their precondition threshold. Callers can ignore divergence if they prefer simpler "plan until complete" semantics. | HTN Plan | Accepted | Yes |
| SD-006 | `HTNDomain` is immutable after `LoadFromJson()` | Shared safely across entities running the same domain. Consistent with `RuleSet` immutability. Domain is data — it should not change at runtime. | HTN Domain | Accepted | Yes |
| SD-007 | Depth-first forward-chaining with ordered method selection | Ordered methods give deterministic, tunable behaviour. Authors put the "best" method first; the planner falls through to the next if the precondition fails. Simpler to reason about than best-first or heuristic search for game AI domains. | HTN Planner | Accepted | Yes |
| SD-008 | Async path submits to `AIBudgetScheduler` — no internal threading | Consistent with DiaUtilityAI (SD-008) and the platform threading model (PD-002). DiaHTN does not spin its own threads. | Async Planning | Accepted | Yes |
| SD-009 | `OperatorRegistry` is explicitly constructed, not a singleton | Avoids hidden global state. Same pattern as `RuleActionRegistry` (DiaRules SD-003) and `ConditionRegistry` (DiaCondition SD-003). Caller creates, owns, passes by reference. | Operator Registry | Accepted | Yes |
| SD-010 | Operator parameters are `DynamicArrayC<StringCRC>` — symbolic, not typed values | Parameters name target entities, waypoints, or config keys resolved by the operator callback. Keeps the plan data-driven without encoding game-specific types in DiaHTN. | HTN Plan, Operator Registry | Accepted | Yes |
| SD-011 | Caller owns JSON parsing; `LoadFromJson` takes `Json::Value&` | Consistent across DiaStateMachine, DiaRules, DiaCondition, DiaUtilityAI. | HTN Domain | Accepted | Yes |
| SD-012 | Test utilities ship inside `DiaHTN/Testing/` | Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All task IDs, operator IDs, and method IDs use `StringCRC`. `HTNPlannerComponent::kUniqueId` is a `StringCRC` constant. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `HTNPlannerComponent::Tick()` is called from within a phase/module by game code. DiaHTN has no lifecycle of its own. Async planning work runs on SimPU via `AIBudgetScheduler`. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal planner implementation may use STL for the search stack. |
| PD-005 | Platform | x64 only | `DiaHTN.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaHTN.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaHTN.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diahtn.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal search stack may use STL. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All types in `Dia::HTN::` namespace. |
| AD-004 | Dia App | ProcessingUnit/Phase/Module for application structure | `HTNPlannerComponent` is a proper `IComponent` — not a singleton or service. |

## Open Design Questions

All three design questions resolved 2026-07-29:

1. **World state effects (STRIPS-style)** — **Resolved: live state only.** Planner reasons against `IConditionContext` snapshot with no simulated state deltas. Revisit after the first CluicheTest consumer exposes a concrete planning-chain gap.

2. **Divergence snapshot granularity** — **Resolved: minimal snapshot.** `HTNPlan::HasDiverged()` snapshots only the slot/field/value pairs tested by the active plan's method preconditions (consistent with SD-005). Cheap and precise; no full context clone.

3. **Failed plan behaviour** — **Resolved: caller-driven only.** `HTNPlannerComponent` reports `kFailed` and stops. Caller decides whether to re-plan, wait, or switch goal. Consistent with DiaRules and DiaUtilityAI. Add a `FailurePolicy` enum when a real game use-case demands it.

## Status

**Status:** `Done`

**Plan:** @docs/specs/applications/dia/systems/diahtn/diahtn.plan.md
