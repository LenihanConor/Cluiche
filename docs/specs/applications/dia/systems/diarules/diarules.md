# System Spec: DiaRules

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaRules is the forward-chaining rule engine for the Dia engine. It evaluates a set of condition→action rules against a condition context each frame and fires every rule whose guard passes — no priority ordering, no first-match stop. Rules are defined in JSON, actions are bound in code via a registry keyed by `StringCRC`.

The system is deliberately stateless: `RuleSet` is an immutable loaded collection, `RuleActionRegistry` is a plain handler table, and `Evaluate()` is a pure data-in/side-effect-out call. Per-action cooldowns, scoring, and budget-aware scheduling belong to DiaUtilityAI, not here.

**Research:** @docs/research/ai_gameplay_tools/ideate.md (Candidate 5)

**Dependency chain:**
`DiaRules → DiaCondition (ConditionExpr, IConditionContext)`
`DiaRules → DiaCore (StringCRC, DynamicArrayC, Json)`
`DiaRules → DiaEntity (RuleSetComponent via IComponent)`

## Responsibilities

- Provide a `RuleActionRegistry` for binding named action handlers to `StringCRC` keys — explicitly constructed, not a singleton
- Provide a `RuleDef` value type: one `ConditionExpr` guard + one or more `StringCRC` action IDs + an optional `StringCRC` rule ID for debugging
- Provide a `RuleSet` class that loads from JSON and evaluates all matching rules against an `IConditionContext`
- Fire **all** rules whose guard passes in a single `Evaluate()` call (not first-match)
- Return a count of rules fired from `Evaluate()` so callers can detect the no-match case
- Load `RuleSet` definitions from `Json::Value` (caller owns JSON parsing)
- Validate rule definitions on load: non-empty action list, no duplicate rule IDs
- Provide a `RuleSetComponent` (`IComponent`) for attaching a `RuleSet` to a `DiaEntity` entity
- Provide test utilities under `DiaRules/Testing/`: `FireRuleSet` helper, `MockConditionContext`, `AssertRulesFired`
- Identify all rules and actions via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.diarules.architecture.module.md` YAML module documentation
- Provide `DiaRules.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- Per-action cooldowns or rate limiting — DiaUtilityAI
- Score-based action selection or priority ordering — DiaUtilityAI
- Budget-aware time-sliced evaluation — DiaAIBudget + DiaUtilityAI
- Condition/expression evaluation logic — DiaCondition owns `ConditionExpr` and `IConditionContext`
- Blackboard slot registration or accessor wiring — DiaBlackboard + DiaCondition
- State machine integration — `ConditionGuardAdapter` in DiaCondition handles that
- Visual debugging of rule evaluation — future DiaVisualDebugger extension
- Thread-safe evaluation — caller's responsibility; `Evaluate()` is single-threaded

## Public Interfaces

### RuleActionRegistry

```cpp
namespace Dia::Rules {
    using RuleActionFn = void(*)(void* actionContext);

    // Open handler table mapping StringCRC action names to callbacks.
    // NOT a singleton — caller creates, owns, and passes by reference.
    class RuleActionRegistry {
    public:
        void Register(Dia::Core::StringCRC actionId, RuleActionFn handler);

        RuleActionFn Find(Dia::Core::StringCRC actionId) const;
        bool Has(Dia::Core::StringCRC actionId) const;
    };
}
```

### RuleDef and RuleSet

```cpp
namespace Dia::Rules {
    struct RuleDef {
        Dia::Core::StringCRC id;             // optional, kInvalidCRC if unnamed
        Dia::Condition::ConditionExpr guard;
        Dia::Core::DynamicArrayC<Dia::Core::StringCRC> actions;
    };

    class RuleSet {
    public:
        // Load from JSON. Caller owns JSON parsing.
        // JSON format:
        // { "rules": [ { "id": "...", "guard": <ConditionExpr>, "actions": ["ActionA", "ActionB"] } ] }
        static RuleSet LoadFromJson(const Json::Value& root);

        // Validate: non-empty action lists, no duplicate rule IDs.
        // Returns false + populates errors on failure.
        bool Validate(Dia::Core::DynamicArrayC<const char*>& outErrors) const;

        // Evaluate all rules. Fires every action for every rule whose guard passes.
        // actionContext is passed opaquely to each RuleActionFn.
        // Returns count of rules that fired (0 = no match).
        int Evaluate(Dia::Condition::IConditionContext& context,
                     const RuleActionRegistry& registry,
                     void* actionContext) const;

        int GetRuleCount() const;
    };
}
```

### RuleSetComponent

```cpp
namespace Dia::Rules {
    class RuleSetComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"RuleSetComponent"};

        // Caller moves RuleSet into the component after loading.
        void SetRuleSet(RuleSet&& ruleSet);

        // Registry must outlive the component.
        void SetRegistry(const RuleActionRegistry* registry);

        // Evaluate with the given context and action context.
        // No-op if no RuleSet or registry has been set.
        int Evaluate(Dia::Condition::IConditionContext& context,
                     void* actionContext) const;

        const RuleSet* GetRuleSet() const;
        const RuleActionRegistry* GetRegistry() const;
    };
}
```

### JSON Schema

```json
{
  "rules": [
    {
      "id": "AttackWhenHealthy",
      "guard": { "op": "and", "conditions": [
        { "slot": "health", "field": "value", "op": ">=", "value": 50.0 },
        { "slot": "enemy",  "field": "visible", "op": "==", "value": true }
      ]},
      "actions": ["StartAttack", "SetAggressive"]
    },
    {
      "guard": { "slot": "health", "field": "value", "op": "<", "value": 10.0 },
      "actions": ["Flee"]
    }
  ]
}
```

### Test Utilities

```cpp
// DiaRules/Testing/RulesTestHelpers.h
namespace Dia::Rules::Testing {
    // Evaluate a RuleSet against a mock context and collect fired action IDs.
    int FireRuleSet(const RuleSet& ruleSet,
                    Dia::Condition::IConditionContext& context,
                    Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& outFiredActions);

    // Assert that a specific set of actions fired (order-independent).
    void AssertActionsFired(const Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& firedActions,
                            std::initializer_list<Dia::Core::StringCRC> expected);

    // Assert that a specific action did NOT fire.
    void AssertActionNotFired(const Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& firedActions,
                              Dia::Core::StringCRC actionId);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Rule Action Registry | `RuleActionRegistry` — open handler table mapping StringCRC action names to C++ callbacks. Explicitly constructed, not a singleton. | [rule-action-registry.md](rule-action-registry.md) | Draft |
| Rule Set | `RuleSet` — JSON-loadable collection of rules. All-matching `Evaluate()` fires every rule whose guard passes. Returns count of rules fired. | [rule-set.md](rule-set.md) | Draft |
| Rule Set Component | `RuleSetComponent` (`IComponent`) — attaches a `RuleSet` + `RuleActionRegistry` to a `DiaEntity` entity. Evaluation forwarded from the component. | [rule-set-component.md](rule-set-component.md) | Draft |
| Test Utilities | `DiaRules/Testing/` — `FireRuleSet`, `AssertActionsFired`, `AssertActionNotFired`. Ships with library; consumer opt-in via include. | [test-utilities.md](test-utilities.md) | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCondition** — `ConditionExpr` (guard expression type), `IConditionContext` (evaluation context interface)
- **DiaCore** — `StringCRC` (action/rule identity), `DynamicArrayC` (container), `Json` (JSON loading), `IComponent`/`ComponentFactoryRegistry` (entity integration), `DIA_ASSERT`

**Transitive (via DiaCondition):**
- **DiaBlackboard** — concrete `IConditionContext` implementation; DiaRules does not depend on it directly

**Explicitly excluded:**
- **DiaApplicationFlow** — rule sets are evaluated by caller code within phases/modules; no lifecycle dependency
- **DiaAIBudget** — no budget-aware slicing at this layer
- **DiaStateMachine** — no direct dependency; `ConditionGuardAdapter` in DiaCondition bridges the two if needed

**Dependents:**
- Game code (CluicheTest and future games) — attaches `RuleSetComponent` to entities, registers action handlers
- DiaUtilityAI — may build on top of DiaRules for prerequisite checking

## Out of Scope

- Per-rule cooldowns, scoring, or max-concurrent limits — DiaUtilityAI
- Budget-aware time-sliced evaluation — DiaAIBudget + DiaUtilityAI
- Visual debugging or editor tooling — future DiaVisualDebugger extension
- Network synchronization of rule state
- Thread-safe concurrent evaluation — caller synchronizes
- Hot-reload of JSON rule definitions while rules are executing — reload on next construct

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | All-matching evaluation: fire every rule whose guard passes, not first-match | Rule systems are most useful when multiple non-exclusive rules can fire simultaneously (e.g. both "Flee" and "CallForHelp" on low health). First-match would require artificial priority ordering for independent rules. | RuleSet | Accepted | Yes |
| SD-002 | No priority ordering in v1 | Rules fire in definition order. Priority adds spec complexity before a concrete game use-case justifies it. Add as a `priority: int` field in JSON when needed. | RuleSet | Accepted | Yes |
| SD-003 | RuleActionRegistry is explicitly constructed, not a singleton | Avoids hidden global state. Same pattern as `CallbackRegistry` in DiaStateMachine. Caller creates, owns, and passes by reference to `Evaluate()`. Trivially testable with isolated registries. | RuleActionRegistry | Accepted | Yes |
| SD-004 | Action callbacks use type-erased `void*` actionContext | Consistent with DiaStateMachine action callback pattern. Zero allocation overhead. Caller casts to their game context type. | RuleActionRegistry | Accepted | Yes |
| SD-005 | `RuleSet` is immutable after `LoadFromJson()` | Pure data-in/side-effect-out evaluation. No internal mutable state means `Evaluate()` is re-entrant and easy to test. | RuleSet | Accepted | Yes |
| SD-006 | `Evaluate()` takes `IConditionContext&`, not `BlackboardInstance&` directly | Keeps DiaRules decoupled from the concrete blackboard implementation. Any `IConditionContext` implementation works — mock contexts in tests, real blackboards in production. | RuleSet | Accepted | Yes |
| SD-007 | `Evaluate()` returns count of rules fired | Zero means no rule matched, which is often meaningful (e.g. fall through to default behaviour). Cheaper than collecting fired rule IDs in the hot path. | RuleSet | Accepted | Yes |
| SD-008 | Caller owns JSON parsing; `LoadFromJson` takes `Json::Value&` | Consistent with DiaStateMachine. Avoids baking a file-loading path into the library; caller decides how to acquire JSON (file, embedded, network). | RuleSet | Accepted | Yes |
| SD-009 | Test utilities ship inside `DiaRules/Testing/` | Test helpers live in the library, not in GoogleTests. Consumers opt in via `#include <DiaRules/Testing/...>`. Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All rule IDs, action IDs use `StringCRC`. `RuleSetComponent::kUniqueId` is a `StringCRC` constant. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `Evaluate()` is called from within a phase/module by game code. DiaRules has no lifecycle of its own. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal implementation may use STL. |
| PD-005 | Platform | x64 only | `DiaRules.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaRules.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaRules.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic or trace output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diarules.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Rules::` namespace. |

## Open Design Questions

1. **Action context typing** — the `void* actionContext` parameter is flexible but loses type safety at the call site. If every game ends up using the same entity-level context type (e.g. `EntityActionContext` with entity handle + blackboard ref), should that be a first-class concept in DiaRules, or stay generic? Revisit after the first CluicheTest consumer is wired up.

2. **Evaluation frequency** — is per-frame evaluation the right default, or should `RuleSetComponent` support a configurable tick rate (e.g. every N frames, or on-demand trigger)? The research flags AI budget starvation as a known pitfall; once DiaAIBudget is built, `RuleSet::Evaluate` should be pluggable into it. Consider whether the component needs a `SetTickRate(int framesPerEval)` field from the start.

3. **Rule firing observability** — currently `Evaluate()` returns only a count. If a CluicheTest stage needs to visualise which rules fired each frame, a `DynamicArrayC<StringCRC> outFiredRuleIds` out-parameter (optional, nullptr = fast path) would be the minimal addition. Decide when the first debug use-case arrives.

## Status

`Done` — plan: [diarules.plan.md](diarules.plan.md)
