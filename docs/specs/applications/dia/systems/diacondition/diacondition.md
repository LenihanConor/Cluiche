# System Spec: DiaCondition

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaCondition is the shared boolean expression evaluation layer for the Dia engine. It provides a generic, data-driven way to express and evaluate conditions against any data source — blackboard slots, game state, component values — without coupling the expression evaluator to any specific runtime.

The system has three parts that compose independently:
- **`IConditionContext`** — the evaluation interface: callers implement `GetFloat`/`GetBool` against their data source
- **`ConditionRegistry`** — a concrete `IConditionContext` that holds registered `(slot, field)` accessor callbacks; the bridge from string-keyed JSON conditions to typed C++ reads
- **`ConditionExpr`** — a JSON-loadable boolean expression tree that evaluates against any `IConditionContext`
- **`ConditionGuardAdapter`** — utility that registers a `ConditionExpr` as a named guard in a DiaStateMachine `CallbackRegistry` with zero FSM changes

DiaCondition is deliberately low-level. It does not own blackboard registration, entity lifecycle, or scheduling. DiaRules and DiaUtilityAI consume it as a dependency.

**Research:** @docs/research/ai_gameplay_tools/ideate.md (Candidate 5)

**Dependency chain:**
`DiaCondition → DiaCore (StringCRC, DynamicArrayC, Json)`
`DiaCondition → DiaStateMachine (CallbackRegistry — for ConditionGuardAdapter only)`

## Responsibilities

- Provide an `IConditionContext` interface with `GetFloat(slot, field)` and `GetBool(slot, field)` by `StringCRC` pair
- Provide a `ConditionRegistry` — a concrete `IConditionContext` that stores `(slot, field)` accessor callbacks registered by the caller
- Provide a `ConditionExpr` class: JSON-loadable boolean expression tree with AND / OR / NOT composition and leaf comparison operators (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- Support float and bool value types in leaf comparisons (v1 scope; no int, string, or vector types)
- Evaluate `ConditionExpr` against any `IConditionContext` via `Evaluate(IConditionContext&)`
- Load `ConditionExpr` from `Json::Value` (caller owns JSON parsing)
- Validate expression trees on load: all leaf slot/field pairs resolvable in a supplied registry, no malformed ops
- Provide a `ConditionGuardAdapter` free-function set that registers a `ConditionExpr` as a named guard in a DiaStateMachine `CallbackRegistry`
- Provide test utilities under `DiaCondition/Testing/`: `MockConditionContext`, `AssertExprResult`, slot-value builder helpers
- Identify all slot/field keys via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.diacondition.architecture.module.md` YAML module documentation
- Provide `DiaCondition.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- Blackboard slot registration or typed slot ownership — DiaBlackboard
- Providing a `BlackboardConditionContext` implementation — DiaBlackboard provides its own `IConditionContext` adapter over its slots
- Rule set evaluation or action dispatch — DiaRules
- Score-based utility evaluation — DiaUtilityAI
- Visual debugging of expression evaluation — future DiaVisualDebugger extension
- Thread-safe evaluation — caller's responsibility; `Evaluate()` is single-threaded

## Public Interfaces

### IConditionContext

```cpp
namespace Dia::Condition {
    // Pure evaluation interface. Implemented by callers:
    // - ConditionRegistry (accessor callbacks)
    // - DiaBlackboard's BlackboardConditionContext
    // - MockConditionContext in tests
    class IConditionContext {
    public:
        virtual ~IConditionContext() = default;

        virtual float GetFloat(Dia::Core::StringCRC slot,
                               Dia::Core::StringCRC field) const = 0;
        virtual bool  GetBool (Dia::Core::StringCRC slot,
                               Dia::Core::StringCRC field) const = 0;
    };
}
```

### ConditionRegistry

```cpp
namespace Dia::Condition {
    using FloatAccessorFn = float(*)(void* data);
    using BoolAccessorFn  = bool (*)(void* data);

    // Concrete IConditionContext. Caller registers (slot, field) accessors
    // and provides a void* data pointer at evaluation time.
    // NOT a singleton — caller creates, owns, and passes by reference.
    class ConditionRegistry : public IConditionContext {
    public:
        // data pointer is stored; must remain valid for the lifetime of
        // calls to Evaluate on any ConditionExpr using this registry.
        explicit ConditionRegistry(void* data);

        void RegisterFloat(Dia::Core::StringCRC slot,
                           Dia::Core::StringCRC field,
                           FloatAccessorFn accessor);
        void RegisterBool (Dia::Core::StringCRC slot,
                           Dia::Core::StringCRC field,
                           BoolAccessorFn  accessor);

        bool HasFloat(Dia::Core::StringCRC slot,
                      Dia::Core::StringCRC field) const;
        bool HasBool (Dia::Core::StringCRC slot,
                      Dia::Core::StringCRC field) const;

        // IConditionContext
        float GetFloat(Dia::Core::StringCRC slot,
                       Dia::Core::StringCRC field) const override;
        bool  GetBool (Dia::Core::StringCRC slot,
                       Dia::Core::StringCRC field) const override;
    };
}
```

### ConditionExpr

```cpp
namespace Dia::Condition {
    enum class ConditionOp {
        kAnd, kOr, kNot,                          // interior nodes
        kEq, kNeq, kLt, kLte, kGt, kGte          // leaf comparison ops
    };

    // Immutable expression tree. Move into owning types after LoadFromJson.
    class ConditionExpr {
    public:
        // Evaluate against any IConditionContext.
        bool Evaluate(IConditionContext& ctx) const;

        // Load from JSON. Caller owns JSON parsing.
        // Returns a default-false expr and populates errors on failure.
        static ConditionExpr LoadFromJson(
            const Json::Value& node,
            Dia::Core::DynamicArrayC<const char*>& outErrors);

        // Validate all leaf slot/field pairs are resolvable in the given registry.
        bool Validate(const ConditionRegistry& registry,
                      Dia::Core::DynamicArrayC<const char*>& outErrors) const;

        bool IsValid() const;
    };
}
```

### ConditionGuardAdapter

```cpp
namespace Dia::Condition {
    // Register a ConditionExpr as a named guard in a DiaStateMachine CallbackRegistry.
    // expr and ctx must outlive all guard evaluations.
    // guardName is the StringCRC the FSM definition references by name.
    void RegisterAsGuard(Dia::Core::StringCRC guardName,
                         const ConditionExpr& expr,
                         IConditionContext& ctx,
                         Dia::StateMachine::CallbackRegistry& registry);
}
```

### JSON Schema

```json
// Leaf — float comparison
{ "slot": "health", "field": "value", "op": ">=", "value": 50.0 }

// Leaf — bool comparison
{ "slot": "enemy", "field": "visible", "op": "==", "value": true }

// AND interior node
{ "op": "and", "conditions": [
    { "slot": "health", "field": "value", "op": ">=", "value": 50.0 },
    { "slot": "enemy",  "field": "visible", "op": "==", "value": true }
]}

// OR interior node
{ "op": "or", "conditions": [
    { "slot": "health", "field": "value", "op": "<", "value": 20.0 },
    { "slot": "ammo",   "field": "count",   "op": "==", "value": 0.0 }
]}

// NOT interior node
{ "op": "not", "condition":
    { "slot": "shield", "field": "active", "op": "==", "value": true }
}
```

### Test Utilities

```cpp
// DiaCondition/Testing/ConditionTestHelpers.h
namespace Dia::Condition::Testing {
    // Simple mock context for tests — caller sets slot/field values directly.
    class MockConditionContext : public IConditionContext {
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

    // Assert a ConditionExpr evaluates to an expected result.
    void AssertExprResult(const ConditionExpr& expr,
                          IConditionContext& ctx,
                          bool expected);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Condition Context Interface | `IConditionContext` — pure virtual interface for float/bool slot+field access. Foundation for all evaluation. | [condition-context.md](condition-context.md) | Draft |
| Condition Registry | `ConditionRegistry` — concrete `IConditionContext` with registered accessor callbacks. Explicitly constructed, not a singleton. | [condition-registry.md](condition-registry.md) | Draft |
| Condition Expression | `ConditionExpr` — JSON-loadable AND/OR/NOT boolean expression tree evaluated via `IConditionContext`. | [condition-expr.md](condition-expr.md) | Draft |
| Condition Guard Adapter | `RegisterAsGuard()` — wires a `ConditionExpr` + `IConditionContext` into a DiaStateMachine `CallbackRegistry` guard slot. | [condition-guard-adapter.md](condition-guard-adapter.md) | Draft |
| Test Utilities | `DiaCondition/Testing/` — `MockConditionContext`, `AssertExprResult`. Ships with library; consumer opt-in via include. | [test-utilities.md](test-utilities.md) | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `StringCRC` (slot/field identity), `DynamicArrayC` (containers), `Json` (JSON loading), `DIA_ASSERT`
- **DiaStateMachine** — `CallbackRegistry` (for `ConditionGuardAdapter` only)

**Explicitly excluded:**
- **DiaBlackboard** — DiaCondition is lower-level; DiaBlackboard provides its own `BlackboardConditionContext : IConditionContext` adapter above this layer
- **DiaApplicationFlow** — no lifecycle dependency; expressions are evaluated by caller code

**Dependents:**
- **DiaRules** — uses `ConditionExpr` as rule guards, `IConditionContext` as evaluation target
- **DiaUtilityAI** — uses `ConditionExpr` for action prerequisites
- **DiaBlackboard** — provides `BlackboardConditionContext` implementing `IConditionContext`
- Game code — registers accessor callbacks in `ConditionRegistry`, wires guards via `ConditionGuardAdapter`

## Out of Scope

- Blackboard slot registration or typed slot ownership — DiaBlackboard
- `BlackboardConditionContext` implementation — DiaBlackboard owns that bridge
- Integer, string, or vector value types in leaf comparisons — deferred; add when a concrete use-case arrives
- Arithmetic expressions (e.g. `health + shield >= 50`) — out of scope for v1
- Visual debugging of expression evaluation — future DiaVisualDebugger extension
- Thread-safe evaluation — caller synchronizes
- Hot-reload of expression trees while evaluating — reload on next construct

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | `IConditionContext` is a pure interface; `ConditionRegistry` is one concrete implementation | Keeps the expression evaluator decoupled from DiaBlackboard. Mock contexts work in tests. Any data source (blackboard, game state struct, component) can implement the interface without taking a DiaBlackboard dependency. | All features | Accepted | Yes |
| SD-002 | Float and bool value types only in v1 | Covers the overwhelmingly common case (health comparisons, flag checks). Int is representable as float. String and vector comparisons add complexity before a concrete use-case exists. Add types incrementally as JSON schema fields. | Condition Expression | Accepted | Yes |
| SD-003 | `ConditionRegistry` explicitly constructed, not a singleton | Avoids hidden global state. Consistent with `RuleActionRegistry` (DiaRules) and `CallbackRegistry` (DiaStateMachine). Caller creates, owns, and passes by reference. Trivially testable with isolated registries. | Condition Registry | Accepted | Yes |
| SD-004 | `ConditionExpr` is immutable after `LoadFromJson()` | Pure data-in/result-out evaluation. No internal mutable state means `Evaluate()` is re-entrant and easy to test. Consistent with `RuleSet` (DiaRules SD-005). | Condition Expression | Accepted | Yes |
| SD-005 | AND / OR / NOT composition only; no XOR, NAND, etc. | Sufficient to express all common game conditions. Additional operators add JSON surface area for marginal benefit. | Condition Expression | Accepted | Yes |
| SD-006 | Leaf comparison operators: `==`, `!=`, `<`, `<=`, `>`, `>=` | Complete ordered comparison set. Sufficient for all float and bool comparisons. Bool comparisons typically use `==` / `!=` against `true`/`false`. | Condition Expression | Accepted | Yes |
| SD-007 | `ConditionGuardAdapter` is a free function, not a class | Guards are stateless once registered — they capture `const ConditionExpr*` and `IConditionContext*`. A class would add no value. Mirrors the `CallbackRegistry::RegisterGuard()` pattern in DiaStateMachine. | Condition Guard Adapter | Accepted | Yes |
| SD-008 | `ConditionGuardAdapter` depends on DiaStateMachine (not the reverse) | The adapter is a convenience shim in the higher-level system (DiaCondition), not a modification to the lower-level system (DiaStateMachine). DiaStateMachine has no knowledge of DiaCondition. | Condition Guard Adapter | Accepted | Yes |
| SD-009 | `GetFloat`/`GetBool` return value (not bool+out-param) | Keeps expression evaluation code clean — no error-checking noise in the evaluation loop. Missing slot/field returns a defined default (0.0f / false) and fires DIA_ASSERT in debug. | IConditionContext | Accepted | Yes |
| SD-010 | Caller owns JSON parsing; `LoadFromJson` takes `Json::Value&` | Consistent with DiaStateMachine and DiaRules. Avoids baking a file-loading path into the library. | Condition Expression | Accepted | Yes |
| SD-011 | Test utilities ship inside `DiaCondition/Testing/` | Test helpers live in the library, not in GoogleTests. Consumers opt in via `#include <DiaCondition/Testing/...>`. Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All slot and field keys use `StringCRC`. No raw string comparison at evaluation time. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `Evaluate()` is called from within a phase/module by game code. DiaCondition has no lifecycle of its own. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal implementation may use STL. |
| PD-005 | Platform | x64 only | `DiaCondition.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaCondition.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaCondition.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diacondition.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Condition::` namespace. |

## Open Design Questions

1. **`ConditionRegistry` data pointer lifetime** — the registry stores a `void*` data pointer at construction and all accessor callbacks receive it. If the underlying data object is replaced mid-frame (e.g. entity recycled), callers must reconstruct the registry. Is that acceptable, or should the registry take a `void*(*getData)()` factory instead? Revisit after the first CluicheTest consumer is wired up.

2. **Missing slot/field behaviour** — currently `GetFloat`/`GetBool` return 0.0f/false with `DIA_ASSERT` in debug when a slot/field pair is unregistered. An alternative is returning an `Optional<float>` so the expression can distinguish "missing" from "zero." Decide when a concrete use-case (e.g. optional sensor data) arises.

3. **Validation at load vs. evaluation time** — `ConditionExpr::Validate()` checks all leaf pairs against a `ConditionRegistry`, but the registry used for validation and the one used for evaluation may differ (e.g. validate against a full schema registry, evaluate against a per-entity live registry). Confirm this two-registry pattern is acceptable before implementation, or require they be the same instance.

## Status

`Done`

**Plan:** @docs/specs/applications/dia/systems/diacondition/diacondition.plan.md
