# System Spec: DiaObjective

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** gameplay

## Purpose

DiaObjective is the data-driven gameplay goal tracking system for the Dia engine. It evaluates objective completion conditions against any `IConditionContext` (the same interface used by DiaRules and DiaUtilityAI), latches when conditions first become true, and notifies registered observers — without coupling to any specific data source such as DiaEconomy or DiaBlackboard.

The system has three parts:
- **`ObjectiveDef`** — an immutable data record loaded from JSON: an ID, classification (primary/secondary/optional), a `ConditionExpr` completion condition, an optional failure condition, an optional reward payload (generic key-value bag), and optional prerequisite objective IDs
- **`ObjectiveSet`** — the evaluated runtime collection: holds a list of `ObjectiveDef`s, tracks per-objective state (Inactive → Active → Complete / Failed), exposes `Evaluate(IConditionContext&)` which is called by the application module each tick
- **`IObjectiveObserver`** — the notification interface: `OnObjectiveActivated`, `OnObjectiveCompleted`, `OnObjectiveFailed`; any system (economy, UI, audio) implements this and registers on the `ObjectiveSet`

The calling application module owns the `ConditionRegistry`, decides when to call `Evaluate()`, and wires any observers. DiaObjective knows nothing about DiaEconomy, DiaBlackboard, or any other domain system.

**Dependency chain:**
`DiaObjective → DiaCondition (ConditionExpr, IConditionContext) → DiaCore (StringCRC, DynamicArrayC, Json)`

## Responsibilities

- Provide `ObjectiveDef` — immutable data record: ID, classification, completion `ConditionExpr`, optional failure `ConditionExpr`, reward payload (key-value bag), optional prerequisite IDs
- Provide `ObjectiveSet` — runtime collection that tracks per-objective state and evaluates conditions each tick
- Track three objective states: `Inactive` (prerequisites not met), `Active` (evaluating), `Complete` (completion condition latched), `Failed` (failure condition latched)
- Activate objectives automatically when all prerequisite objectives are `Complete`
- Evaluate only `Active` objectives each tick — skip `Inactive`, `Complete`, and `Failed`
- Latch on first-true: once an objective transitions to `Complete` or `Failed`, do not re-evaluate it
- Notify `IObjectiveObserver` subscribers on `Activated`, `Completed`, and `Failed` transitions
- Provide a generic reward payload (`DynamicArrayC` of `RewardEntry { StringCRC type; float amount }`) — DiaObjective fires it; observers interpret it
- Load `ObjectiveDef` list from JSON via `ObjectiveSet::LoadFromJson()`
- Validate all completion/failure condition slot/field pairs against a supplied `ConditionRegistry` at load time
- Provide `ObjectiveSetComponent` — `DIA_COMPONENT` wrapper for entity integration
- Provide test utilities under `DiaObjective/Testing/`: `ObjectiveTestHelpers`, pre-built `MockConditionContext` usage examples
- Identify all objective IDs via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.diaobjective.architecture.module.md` YAML module documentation
- Provide `DiaObjective.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- Blackboard slot registration or typed slot ownership — DiaBlackboard
- Economy resource payouts — DiaEconomy (subscribes as an `IObjectiveObserver`)
- UI display of objective state — application or editor code
- Scheduling when `Evaluate()` is called — the application module decides this
- Cross-thread delivery of observer notifications — DiaObjective notifies on the calling thread; DiaStreams is for cross-PU delivery
- Quest trees, branching narrative graphs — objective chaining via prerequisite IDs covers linear chains; DAGs are out of scope for v1
- Persistent save/load of objective state — caller serialises `ObjectiveSet` state; DiaObjective provides no serialiser

## Public Interfaces

### IObjectiveObserver

```cpp
namespace Dia::Objective {
    class IObjectiveObserver {
    public:
        virtual ~IObjectiveObserver() = default;

        virtual void OnObjectiveActivated(Dia::Core::StringCRC objectiveId) = 0;
        virtual void OnObjectiveCompleted(Dia::Core::StringCRC objectiveId,
                                          const RewardPayload& reward) = 0;
        virtual void OnObjectiveFailed   (Dia::Core::StringCRC objectiveId) = 0;
    };
}
```

### RewardPayload

```cpp
namespace Dia::Objective {
    struct RewardEntry {
        Dia::Core::StringCRC type;   // e.g. StringCRC("gold"), StringCRC("xp")
        float                amount;
    };

    // Fixed-capacity reward list (capacity 8 covers typical objective rewards).
    using RewardPayload = Dia::Core::DynamicArrayC<RewardEntry, 8>;
}
```

### ObjectiveDef

```cpp
namespace Dia::Objective {
    enum class ObjectiveClassification { kPrimary, kSecondary, kOptional };
    enum class ObjectiveState          { kInactive, kActive, kComplete, kFailed };

    // Immutable data record. Move-only (ConditionExpr is move-only).
    struct ObjectiveDef {
        Dia::Core::StringCRC      id;
        ObjectiveClassification   classification;
        Dia::Condition::ConditionExpr completion;       // required
        Dia::Condition::ConditionExpr failure;          // optional — default-invalid = no failure condition
        RewardPayload             reward;
        // Prerequisite objective IDs — all must be Complete before this activates.
        Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 4> prerequisites;
    };
}
```

### ObjectiveSet

```cpp
namespace Dia::Objective {
    class ObjectiveSet {
    public:
        // Load from JSON. Caller owns JSON parsing.
        static ObjectiveSet LoadFromJson(
            const Json::Value& root,
            Dia::Core::DynamicArrayC<const char*, 32>& outErrors);

        // Validate all condition slot/field pairs against the supplied registry.
        bool Validate(const Dia::Condition::ConditionRegistry& registry,
                      Dia::Core::DynamicArrayC<const char*, 32>& outErrors) const;

        // Evaluate all Active objectives. Activates Inactive objectives whose
        // prerequisites are now Complete. Fires observer notifications on transition.
        // Returns the number of objectives that transitioned this call.
        int Evaluate(Dia::Condition::IConditionContext& ctx);

        void AddObserver   (IObjectiveObserver* observer);
        void RemoveObserver(IObjectiveObserver* observer);

        // Inspection
        ObjectiveState       GetState  (Dia::Core::StringCRC objectiveId) const;
        int                  GetCount  () const;
        const ObjectiveDef*  GetAt     (int index) const;

        // Returns true if all Primary objectives are Complete.
        bool AllPrimaryComplete() const;

        // Returns true if any Primary objective has Failed.
        bool AnyPrimaryFailed() const;
    };
}
```

### ObjectiveSetComponent

```cpp
namespace Dia::Objective {
    // DIA_COMPONENT("objective-set-component", version=1)  DIA_READONLY
    class ObjectiveSetComponent : public Dia::Entity::IComponent {
    public:
        void SetObjectiveSet(ObjectiveSet&& set);

        // Delegates to the internal ObjectiveSet.
        int            Evaluate  (Dia::Condition::IConditionContext& ctx);
        ObjectiveState GetState  (Dia::Core::StringCRC objectiveId) const;
        bool           AllPrimaryComplete() const;
        bool           AnyPrimaryFailed  () const;

        void AddObserver   (IObjectiveObserver* observer);
        void RemoveObserver(IObjectiveObserver* observer);
    };
}
```

### JSON Schema

```json
{
    "objectives": [
        {
            "id": "destroy-enemy-base",
            "classification": "primary",
            "completion": { "op": "==", "slot": "enemy", "field": "base_alive", "value": false },
            "failure":    { "op": "==", "slot": "player", "field": "base_alive", "value": false },
            "reward": [
                { "type": "gold", "amount": 200 },
                { "type": "xp",   "amount": 500 }
            ]
        },
        {
            "id": "collect-relics",
            "classification": "secondary",
            "prerequisites": ["destroy-enemy-base"],
            "completion": { "op": ">=", "slot": "player", "field": "relics_collected", "value": 5 },
            "reward": [{ "type": "gold", "amount": 100 }]
        },
        {
            "id": "no-casualties",
            "classification": "optional",
            "completion": { "op": "and", "conditions": [
                { "op": "==", "slot": "player", "field": "units_lost", "value": 0 },
                { "op": "==", "slot": "enemy",  "field": "base_alive", "value": false }
            ]}
        }
    ]
}
```

Classification values: `"primary"`, `"secondary"`, `"optional"`.
`"prerequisites"` and `"failure"` are optional fields.
`"reward"` is optional; omit for objectives with no payout.

### Test Utilities

```cpp
// DiaObjective/Testing/ObjectiveTestHelpers.h
namespace Dia::Objective::Testing {
    // Thin observer that records all transition events for assertion in tests.
    class CapturingObserver : public IObjectiveObserver {
    public:
        struct Event { enum class Type { Activated, Completed, Failed }; Type type; Dia::Core::StringCRC id; };
        const Dia::Core::DynamicArrayC<Event, 32>& GetEvents() const;
        void Clear();

        void OnObjectiveActivated(Dia::Core::StringCRC id) override;
        void OnObjectiveCompleted(Dia::Core::StringCRC id, const RewardPayload&) override;
        void OnObjectiveFailed   (Dia::Core::StringCRC id) override;
    };
}
```

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| ObjectiveDef | Immutable data record: ID, classification, completion/failure ConditionExpr, reward payload, prerequisites | Approved |
| ObjectiveSet | Runtime collection: per-objective state machine, prerequisite activation, all-Active evaluation, latch semantics | Approved |
| Observer notifications | `IObjectiveObserver` — Activated/Completed/Failed callbacks; same-thread only | Approved |
| Prerequisite chaining | Objectives activate only when all prerequisite IDs are Complete; linear DAG via DynamicArrayC | Approved |
| Reward payload | Generic `RewardEntry { type, amount }` bag in `RewardPayload`; fired on Complete; observers interpret | Approved |
| ObjectiveSetComponent | `DIA_COMPONENT` entity wrapper delegating to `ObjectiveSet` | Approved |
| Test Utilities | `DiaObjective/Testing/` — `CapturingObserver`; ships with library | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaCondition** — `ConditionExpr` (completion and failure conditions), `IConditionContext` (evaluation interface), `ConditionRegistry` (validation)
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `Json`, `DIA_ASSERT`
- **diaentitytemplate** — `IComponent`, `DIA_COMPONENT` macro (for `ObjectiveSetComponent`)

**Explicitly excluded:**
- **DiaEconomy** — subscribes as an `IObjectiveObserver`; DiaObjective has no knowledge of it
- **DiaBlackboard** — game code wires blackboard slots into the `ConditionRegistry`; DiaObjective never reads DiaBlackboard directly
- **DiaStreams** — cross-PU delivery is the caller's responsibility; DiaObjective notifies on the calling thread only

**Dependents:**
- Game modules — call `Evaluate()` each tick; implement `IObjectiveObserver` for reward/UI/audio responses
- DiaEconomy — implements `IObjectiveObserver` to pay out resources on completion
- CluicheTest — `ObjectiveTestStage` validates system behaviour

## Out of Scope

- Branching quest trees or DAG objective graphs — prerequisites cover linear chains; DAG deferred
- Partial-progress objectives (e.g. "5/10 relics" as a float 0–1) — deferred; add a `progress` field to `ObjectiveDef` when a concrete use-case arrives
- Per-faction independent objective instances — v1 is single-context; multi-faction support deferred
- Time-limited objectives (fail after N seconds) — add a `time_limit_seconds` field to `ObjectiveDef` when needed
- Persistent save/load — caller serialises state; no serialiser in this system
- Visual debugging overlay — future DiaVisualDebugger extension

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Completion condition uses `ConditionExpr` / `IConditionContext` — no direct coupling to any data source | Keeps DiaObjective decoupled from DiaEconomy, DiaBlackboard, etc. The same registry wired for DiaRules feeds objective evaluation. | All features | Accepted | Yes |
| SD-002 | Latch semantics: once Complete or Failed, never re-evaluate | Objectives are one-shot events, not recurring rules. Re-evaluation would require resetting state — a more complex API with no clear use-case in v1. | ObjectiveSet | Accepted | Yes |
| SD-003 | Same-thread observer notifications; no cross-thread delivery | DiaStreams is for cross-PU traffic. Objective evaluation runs on SimPU; observers registered from SimPU code respond on SimPU. Avoids any threading concern inside DiaObjective. | IObjectiveObserver | Accepted | Yes |
| SD-004 | Reward payload is a generic key-value bag; DiaObjective does not interpret it | Any system (economy, UI, audio) can respond to any reward type without DiaObjective knowing about it. Adding a new reward type requires no change to DiaObjective. | RewardPayload | Accepted | Yes |
| SD-005 | `ObjectiveSet` explicitly constructed, not a singleton | Consistent with `ConditionRegistry`, `RuleSet`, `RuleActionRegistry`. Caller creates, owns, and passes by reference. Trivially testable with isolated sets. | ObjectiveSet | Accepted | Yes |
| SD-006 | `ObjectiveSet` is move-only after `LoadFromJson()` | `ConditionExpr` is move-only; `ObjectiveSet` inherits that constraint. Prevents accidental copies of the expression tree. | ObjectiveSet | Accepted | Yes |
| SD-007 | Prerequisites are objective IDs resolved at activation time, not pointers | Avoids dependency on insertion order. IDs are `StringCRC`; resolution scans the set's own defs. Simpler than a pointer graph for v1 linear chains. | Prerequisite chaining | Accepted | Yes |
| SD-008 | Test utilities ship inside `DiaObjective/Testing/` | Platform-wide pattern (PD-011 implied by DiaCondition SD-011). Consumers opt in via `#include <DiaObjective/Testing/...>`. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All objective IDs, reward type IDs, and slot/field keys use `StringCRC`. No raw string comparison. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `Evaluate()` is called from within a SimPU phase/module by game code. DiaObjective has no lifecycle of its own. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal implementation may use STL. |
| PD-005 | Platform | x64 only | `DiaObjective.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaObjective.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaObjective.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diaobjective.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Objective::` namespace. |

## Open Design Questions

1. **Partial-progress objectives** — the v1 model is binary (incomplete → complete). Some objectives benefit from a progress signal (e.g. "collect 5/10 relics" shown in UI). Consider adding an optional `progress_field` to `ObjectiveDef` pointing at a float slot/field in the `IConditionContext` — callers read it directly for display. Decision: revisit after the first CluicheTest consumer is wired up.

2. **Failure condition vs. always-on evaluation** — failure conditions are evaluated every tick alongside completion conditions. For objectives with no failure condition this is harmless, but if many objectives have complex failure trees it could be expensive. An alternative is a separate `EvaluateFailures()` call so callers can throttle failure checks. Decide based on profiling once a real objective set exists.

3. **Observer lifetime** — `AddObserver` stores a raw pointer; the caller is responsible for calling `RemoveObserver` before the observer is destroyed. This is the same contract as `IEconomyObserver`. If a game module forgets to unregister, the set will call into freed memory. Consider adding a `DIA_ASSERT(observer != nullptr)` guard, or a debug-mode registered-observer lifetime check. Revisit if this causes issues in practice.

## Status

**Status:** `Done`
