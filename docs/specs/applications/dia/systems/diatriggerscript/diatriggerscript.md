# System Spec: DiaTriggerScript

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** gameplay

## Purpose

DiaTriggerScript is a data-driven scripted game events system. It allows level designers to define triggers in JSON — per-map files that say "when condition X is met, execute action Y" — without writing C++ or requiring hardcoded level logic.

The system has three parts:
- **`TriggerDef`** — an immutable data record loaded from JSON: an ID, a trigger condition (one of four trigger types), a list of actions to execute, and optional one-shot vs. repeating semantics
- **`TriggerScriptModule`** — an `IModule` on SimPU that owns a flat list of `TriggerDef`s for the current level, polls them each tick, and executes actions when conditions fire
- **`ITriggerActionHandler`** — the dispatch interface: named action types registered by the caller; DiaTriggerScript dispatches to registered handlers by name

The calling application module owns the `ConditionRegistry`, loads the JSON trigger file, registers action handlers, and decides tick rate. DiaTriggerScript knows nothing about DiaEconomy, DiaObjective internals, or any other domain system — it dispatches to handlers that do.

**Dependency chain:**
`DiaTriggerScript → DiaCondition (ConditionExpr, IConditionContext) → DiaCore`
`DiaTriggerScript → DiaGeometry2D (AABB/Circle — for spatial trigger overlap)`
`DiaTriggerScript → DiaEntitySpatial (entity position queries)`
`DiaTriggerScript → DiaStreams (publishing trigger-fired events cross-PU)`
`DiaTriggerScript → DiaObjective (ChangeObjectiveState action — build-time dependency, decoupled at dispatch)`

## Responsibilities

- Provide `TriggerDef` — immutable data record: ID, trigger type + parameters, action list, one-shot/repeating flag
- Provide `TriggerScriptModule` — `IModule` on SimPU; owns a flat trigger list per level, evaluates each tick
- Support four trigger types: **spatial** (entity enters a region), **temporal** (time elapsed since level start or last fire), **state** (a `ConditionExpr` evaluates true), **count** (N entities of a given type killed/spawned)
- Support four built-in action types: **SpawnEntities**, **GiveResources**, **ChangeObjectiveState**, **FireEvent** (emit a named `TriggerFiredEvent` on DiaStreams sim channel)
- Provide `ITriggerActionHandler` — the open extension point; caller registers named handlers; `FireEvent` is the primary escape hatch for custom reactions
- One-shot semantics: once a trigger fires it is disabled and never re-evaluated
- Repeating semantics: trigger re-arms after firing and can fire again (temporal triggers use an `interval_s` field; others re-arm immediately)
- Per-trigger optional `check_interval_ms` — spatial and count triggers poll at a configurable rate (default 200 ms) to reduce per-frame cost
- Load trigger list from JSON via `TriggerScriptModule::LoadFromJson()`
- Validate all state trigger `ConditionExpr` slot/field pairs against a supplied `ConditionRegistry` at load time
- Publish `TriggerFiredEvent { StringCRC triggerId }` on the sim DiaStreams channel on each fire
- Identify all trigger and event IDs via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.diatriggerscript.architecture.module.md` YAML module documentation
- Provide `DiaTriggerScript.vcxproj` static library registered in `Cluiche.sln`
- Provide test utilities under `DiaTriggerScript/Testing/`: `MockActionHandler`, `TriggerScriptTestHelpers`

## Non-Responsibilities

- Evaluating conditions from DiaBlackboard — caller wires `BlackboardConditionContext` as the `IConditionContext`
- Implementing spatial queries — `TriggerScriptModule` calls `DiaEntitySpatial` query shapes
- Paying out economy resources — a `GiveResources` action dispatches to a registered `ITriggerActionHandler`; DiaEconomy registers the handler
- Objective state mutation — a `ChangeObjectiveState` action dispatches to a registered handler; the game module that owns the `ObjectiveSet` registers the handler
- Scheduling the tick — `TriggerScriptModule::Update(dt)` is called by the application phase; cadence is the caller's decision
- Cross-thread delivery of `TriggerFiredEvent` — DiaStreams handles that; DiaTriggerScript only writes to the sim channel
- Visual debugging of trigger regions — future DiaVisualDebugger extension
- Hot-reload of trigger JSON while the level is running — reload at level restart

## Public Interfaces

### TriggerDef

```cpp
namespace Dia::TriggerScript {
    enum class TriggerType { kSpatial, kTemporal, kState, kCount };

    struct SpatialParams {
        Dia::Geometry2D::AABB region;     // trigger region in world space
        // Optional entity filter — only entities with this StringCRC tag qualify.
        Dia::Core::StringCRC  entityTag;  // empty StringCRC = any entity
    };

    struct TemporalParams {
        float intervalSeconds;            // time between fires (or delay before first fire)
    };

    struct StateParams {
        Dia::Condition::ConditionExpr condition;
    };

    struct CountParams {
        Dia::Core::StringCRC entityTag;   // which entity type to count
        int                  threshold;   // fire when count reaches this value
    };

    struct ActionDef {
        Dia::Core::StringCRC type;        // e.g. StringCRC("SpawnEntities"), StringCRC("FireEvent")
        Json::Value          params;      // type-specific parameters passed verbatim to the handler
    };

    // Immutable data record. Move-only (StateParams contains ConditionExpr).
    struct TriggerDef {
        Dia::Core::StringCRC id;
        TriggerType          type;
        bool                 oneShot;     // default true; false = repeating
        float                checkIntervalMs; // 0 = every tick; >0 = throttled polling

        // Only one of these is valid, chosen by type:
        SpatialParams  spatial;
        TemporalParams temporal;
        StateParams    state;
        CountParams    count;

        Dia::Core::DynamicArrayC<ActionDef, 8> actions;
    };
}
```

### ITriggerActionHandler

```cpp
namespace Dia::TriggerScript {
    // Context passed to an action handler when a trigger fires.
    struct ActionContext {
        Dia::Core::StringCRC triggerId;
        const Json::Value&   params;     // the ActionDef::params from JSON
    };

    class ITriggerActionHandler {
    public:
        virtual ~ITriggerActionHandler() = default;
        virtual void Execute(const ActionContext& ctx) = 0;
    };
}
```

### TriggerActionRegistry

```cpp
namespace Dia::TriggerScript {
    // Explicit registry — caller creates, owns, and passes to TriggerScriptModule.
    // NOT a singleton.
    class TriggerActionRegistry {
    public:
        void Register(Dia::Core::StringCRC actionType, ITriggerActionHandler* handler);
        bool Has(Dia::Core::StringCRC actionType) const;

        // Dispatch: finds handler by type and calls Execute(). DIA_ASSERT if unregistered.
        void Dispatch(Dia::Core::StringCRC actionType, const ActionContext& ctx) const;
    };
}
```

### TriggerScriptModule

```cpp
namespace Dia::TriggerScript {
    // IModule on SimPU. Caller registers with the module system.
    class TriggerScriptModule : public Dia::Application::IModule {
    public:
        // Load trigger list from JSON. Caller owns JSON parsing.
        // Validates all state-trigger ConditionExprs against registry.
        bool LoadFromJson(
            const Json::Value& root,
            const Dia::Condition::ConditionRegistry& validationRegistry,
            Dia::Core::DynamicArrayC<const char*, 32>& outErrors);

        // Set the action registry (must be set before first Update()).
        void SetActionRegistry(TriggerActionRegistry* registry);

        // Set the IConditionContext used for state trigger evaluation.
        void SetConditionContext(Dia::Condition::IConditionContext* ctx);

        // Set the EntitySpatial query interface used for spatial triggers.
        void SetSpatialQuery(Dia::EntitySpatial::IEntitySpatialQuery* query);

        // Called each tick by the application phase. dt = frame delta in seconds.
        void Update(float dt);

        // Inspection
        int  GetTriggerCount() const;
        bool IsFired(Dia::Core::StringCRC triggerId) const;  // one-shot: fired and disabled
        bool IsActive(Dia::Core::StringCRC triggerId) const; // not yet fired / re-armed

        // IModule
        static constexpr Dia::Core::StringCRC kUniqueId{ "TriggerScriptModule" };
        Dia::Core::StringCRC GetUniqueId() const override { return kUniqueId; }
    };
}
```

### TriggerFiredEvent

```cpp
namespace Dia::TriggerScript {
    // Published on the sim DiaStreams channel when any trigger fires.
    struct TriggerFiredEvent {
        Dia::Core::StringCRC triggerId;
        TriggerType          type;
    };
}
```

### JSON Schema

```json
{
    "triggers": [
        {
            "id": "player-enters-castle",
            "type": "spatial",
            "one_shot": true,
            "check_interval_ms": 200,
            "region": { "min": [100, 50], "max": [200, 150] },
            "entity_tag": "player",
            "actions": [
                { "type": "FireEvent",           "params": { "event": "castle-entered" } },
                { "type": "ChangeObjectiveState", "params": { "id": "reach-castle", "state": "complete" } }
            ]
        },
        {
            "id": "wave-two-timer",
            "type": "temporal",
            "one_shot": false,
            "interval_s": 30.0,
            "actions": [
                { "type": "SpawnEntities", "params": { "blueprint": "goblin-wave", "count": 5, "radius": 20 } }
            ]
        },
        {
            "id": "low-health-warning",
            "type": "state",
            "one_shot": false,
            "check_interval_ms": 0,
            "condition": { "op": "<", "slot": "player", "field": "health", "value": 20.0 },
            "actions": [
                { "type": "FireEvent", "params": { "event": "low-health-warning" } }
            ]
        },
        {
            "id": "ten-kills-bonus",
            "type": "count",
            "one_shot": true,
            "check_interval_ms": 100,
            "entity_tag": "enemy",
            "threshold": 10,
            "actions": [
                { "type": "GiveResources", "params": { "type": "gold", "amount": 200 } }
            ]
        }
    ]
}
```

`"one_shot"` defaults to `true`. `"check_interval_ms"` defaults to `0` (every tick). `"entity_tag"` on spatial triggers is optional (omit = any entity). `"region"` is `[minX, minY], [maxX, maxY]` in world units.

### Test Utilities

```cpp
// DiaTriggerScript/Testing/TriggerScriptTestHelpers.h
namespace Dia::TriggerScript::Testing {
    // Records all dispatched action calls for assertion in tests.
    class MockActionHandler : public ITriggerActionHandler {
    public:
        struct Call {
            Dia::Core::StringCRC triggerId;
            Json::Value          params;
        };
        const Dia::Core::DynamicArrayC<Call, 32>& GetCalls() const;
        void Clear();

        void Execute(const ActionContext& ctx) override;
    };

    // Build a minimal TriggerDef with a state condition for unit tests.
    TriggerDef MakeStateTrigger(
        Dia::Core::StringCRC id,
        Dia::Condition::ConditionExpr condition,
        Dia::Core::StringCRC actionType,
        bool oneShot = true);
}
```

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| TriggerDef | Immutable data record: ID, trigger type + params, action list, one-shot/repeating flag | Approved |
| Spatial trigger | Entity-enters-region evaluation via DiaEntitySpatial query; optional entity tag filter; configurable check interval | Approved |
| Temporal trigger | Time-elapsed evaluation; interval_s controls cadence; repeating by default | Approved |
| State trigger | ConditionExpr evaluation via IConditionContext each tick (or throttled); same expression format as DiaRules/DiaObjective | Approved |
| Count trigger | Entity tag count threshold; polling interval; one-shot or repeating | Approved |
| TriggerActionRegistry | Named handler registration; open extension point; `FireEvent` is the primary escape hatch | Approved |
| TriggerScriptModule | IModule on SimPU; loads JSON, polls triggers, dispatches actions, publishes TriggerFiredEvent | Approved |
| Test Utilities | `DiaTriggerScript/Testing/` — `MockActionHandler`, `MakeStateTrigger`; ships with library | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaCondition** — `ConditionExpr` (state triggers), `IConditionContext` (evaluation), `ConditionRegistry` (validation at load)
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `Json`, `DIA_ASSERT`
- **DiaGeometry2D** — `AABB` (spatial trigger region type)
- **DiaEntitySpatial** — `IEntitySpatialQuery` (entity-in-region queries for spatial triggers)
- **DiaStreams** — sim channel write for `TriggerFiredEvent`

**Deferred (build-time dependency, decoupled at dispatch):**
- **DiaObjective** — `ChangeObjectiveState` action dispatches to a registered `ITriggerActionHandler`; the game module that owns the `ObjectiveSet` registers the handler; DiaTriggerScript holds no `ObjectiveSet*`

**Explicitly excluded:**
- **DiaBlackboard** — game code wires a `BlackboardConditionContext` as the `IConditionContext`; DiaTriggerScript never reads DiaBlackboard directly
- **DiaEconomy** — registers an `ITriggerActionHandler` for `GiveResources`; DiaTriggerScript dispatches blindly by name
- **DiaEntitySpawner** — registers an `ITriggerActionHandler` for `SpawnEntities`; DiaTriggerScript dispatches blindly by name

**Dependents:**
- Game modules — `LoadFromJson`, register action handlers, call `Update()` each tick
- CluicheTest — `TriggerScriptTestStage` validates trigger evaluation behaviour

## Out of Scope

- Hot-reload of trigger JSON while level is running — reload at level restart
- Scripted sequences (ordered, multi-step event chains) — compose via `FireEvent` + downstream state machines
- Visual debugging of trigger regions in-editor — future DiaVisualDebugger extension
- Trigger priority or ordering guarantees — all active triggers are evaluated in definition order
- Nested or conditional trigger activation (trigger B enables trigger A) — use prerequisite objectives or state conditions for this
- Thread-safe concurrent `Update()` calls — single SimPU caller assumed

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | `TriggerScriptModule` owns a flat list of triggers; no entity anchor | Triggers are level-wide, not entity-owned. Anchoring to entities requires dummy entities for temporal/state/count triggers with no natural owner. Flat list with a module owner matches DiaObjective and DiaRules patterns. | All features | Accepted | Yes |
| SD-002 | Tick-polled evaluation with per-trigger `check_interval_ms` | Spatial and count triggers can't be purely push-driven without DiaEntitySpatial emitting overlap events it doesn't currently publish. Pull with throttling gives cost control. `check_interval_ms = 0` is every tick for latency-sensitive triggers. | All trigger types | Accepted | Yes |
| SD-003 | Closed built-in action types; `FireEvent` is the extension point | Four action types cover the concrete use-cases. Custom reactions subscribe to `TriggerFiredEvent` on DiaStreams or register an `ITriggerActionHandler` — no plugin factory or open-ended registry required for the common cases. | Action dispatch | Accepted | Yes |
| SD-004 | `TriggerActionRegistry` explicitly constructed, not a singleton | Consistent with `ConditionRegistry`, `RuleActionRegistry`. Caller creates, owns, and passes to the module. Trivially testable with isolated registries. | TriggerActionRegistry | Accepted | Yes |
| SD-005 | `ChangeObjectiveState` and `GiveResources` dispatch to registered handlers; no direct `ObjectiveSet*` or `EconomyInstance*` in DiaTriggerScript | Eliminates circular or upward dependencies. DiaTriggerScript remains buildable without DiaObjective or DiaEconomy. Game code wires the handler at startup; DiaTriggerScript dispatches blindly. | Action dispatch | Accepted | Yes |
| SD-006 | `TriggerFiredEvent` always published on fire, regardless of action type | Decouples observers from action implementation. Any system (audio, UI, analytics) can react to any trigger firing without registering an action handler. Consistent with DiaStreams event-driven design. | TriggerFiredEvent | Accepted | Yes |
| SD-007 | One-shot is the default; repeating is opt-in | The majority of level events are one-shot (cutscene trigger, door unlock). Repeating is the exception. Defaulting to one-shot prevents accidental infinite action loops. | TriggerDef | Accepted | Yes |
| SD-008 | Test utilities ship inside `DiaTriggerScript/Testing/` | Platform-wide pattern (established by DiaCondition SD-011, DiaObjective SD-008). Consumers opt in via `#include <DiaTriggerScript/Testing/...>`. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All trigger IDs, action type names, entity tags, and event names use `StringCRC`. No raw string comparison. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `TriggerScriptModule::Update()` is called from within a SimPU phase. Module registered with the standard IModule system. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal implementation may use STL. |
| PD-005 | Platform | x64 only | `DiaTriggerScript.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaTriggerScript.vcxproj` and `.vcxproj.filters` created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaTriggerScript.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diatriggerscript.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::TriggerScript::` namespace. |

## Open Design Questions

1. **Count trigger — how is the count maintained?** The spec assumes the caller increments a count that DiaTriggerScript reads via `IConditionContext` (e.g. a slot/field pair like `("kills", "enemy_count")`). An alternative is a `TriggerScriptModule::IncrementCount(StringCRC tag)` push API, which avoids needing a blackboard slot just for a kill counter. Decide when the first CluicheTest consumer is wired up.

2. **Spatial trigger re-arm on exit** — currently a repeating spatial trigger re-fires every `check_interval_ms` while the entity remains inside the region. That may fire many times. An alternative is edge-trigger semantics: fire on enter, suppress until exit, fire again on next enter. Decide based on first concrete use-case.

3. **Action parameter type safety** — `ActionDef::params` is a raw `Json::Value` passed verbatim to the handler. This is flexible but unvalidated at load time. Consider adding an optional `ValidateParams(Json::Value&, outErrors)` method to `ITriggerActionHandler` so the module can validate action params at `LoadFromJson()` time rather than at fire time. Revisit after the first handler implementations land.

**Status:** `Done`

**Plan:** @docs/specs/applications/dia/systems/diatriggerscript/diatriggerscript.plan.md
