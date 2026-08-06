# System Spec: DiaEconomy

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** economy, simulation, ai

## Purpose

DiaEconomy is a generic, fully data-driven resource economy system for the Dia engine. It manages named resource pools across independent economy instances — each instance (player, AI opponent, neutral faction) carries its own runtime state derived from a shared schema definition. It knows nothing about factions, UI, or rendering — it tracks resource values, enforces rules, fires discrete events, and provides an API for transferring resources between instances.

Everything that can be expressed as data is. Resource names, bounds, income rules, cost tables, and modifiers all live in JSON. The only C++ registration hook is for derived/computed values (e.g. "net income = harvest rate − upkeep") where an expression language would be excessive.

**Dependency chain:**
`DiaEconomy → DiaCore (containers, StringCRC, Observer, DIA_ASSERT, DIA_LOG_*)`
`DiaEconomy + DiaCondition → conditional modifiers (optional, adaptor)`

## Responsibilities

- Define `EconomySchema` — JSON asset (`economy_schema` type) declaring resource definitions, income rules, cost tables, and modifiers; loaded once; shared across instances
- Define `EconomyInstance` — runtime state (current pool values + active modifier state) for one participant; created from a schema reference with an optional override block
- Provide `EconomySystem` — stateless service class; caller (game/test module) owns the tick loop and calls `Tick(instance, delta_seconds)` per instance per frame
- Provide `ResourcePool` — named float value with configurable `minimum_value`, `maximum_value`, and `starting_value`; clamped after every mutation
- Provide transaction API — `Earn(instance, resource_name, amount)`, `Spend(instance, resource_name, amount)` → `TransactionResult`; `Spend` clamps to zero on insufficient funds and returns `TransactionResult::Clamped`; never silent
- Provide transfer API — `Transfer(from_instance, to_instance, resource_name, amount)` → `TransactionResult`; atomic (deduct then deposit); game code decides when to call it, DiaEconomy provides the primitive
- Provide income rule evaluation — periodic `income_rules` in schema fire `Earn` automatically each tick; rules specify `resource_name`, `amount_per_second`, and optional `when` condition
- Provide cost tables — named JSON objects mapping resource costs for game actions; pure data lookup, no runtime logic; `GetCost(table_name, resource_name)` query
- Provide modifier stack — per-pool multipliers applied after base income/spend; each modifier has `resource_name`, `operation` (multiply_income, multiply_cap, flat_income), `value`, `description`, and optional `when` (DiaCondition expression); simple modifiers (no `when`) always apply; conditional modifiers require DiaCondition
- Provide Observer events (SimPU, same-thread) — `OnPoolChanged(instance, resource_name, new_value, delta)`, `OnTransactionClamped(instance, resource_name, requested, actual)`, `OnTransferCompleted(from, to, resource_name, amount)`, `OnPoolReachedMaximum(instance, resource_name)`, `OnPoolReachedMinimum(instance, resource_name)`; subscribers receive discrete events; cross-PU relay is the caller's concern
- Provide schema validation — `EconomySchemaValidator` checks all required fields, value range coherence (min ≤ starting ≤ max), and unknown field warnings on load
- Emit `DIA_LOG_INFO` on instance creation, schema load, and clamped transactions; `DIA_LOG_WARN` on schema validation warnings
- Provide test utilities under `DiaEconomy/Testing/` — `MockEconomyInstance`, `AssertPoolValue`, `AssertEventFired`; shipped with library, consumer opt-in via include
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.economy.architecture.module.md` YAML module documentation
- Provide `DiaEconomy.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- IModule / PU wiring — DiaEconomy does not provide an IModule; game and test applications wire `EconomySystem::Tick()` into their own SimPU module
- Faction semantics — "faction" is not a concept in DiaEconomy; game code creates instances with meaningful names
- UI and rendering — economy events are fired as Observer notifications; cross-PU relay and display belong to a separate system
- AI decision-making — DiaEconomy publishes resource state; whether an AI spends resources is DiaRules / DiaUtilityAI's concern
- Save/game serialisation — persisting instance state across sessions is DiaSaveGame's concern
- Expression language for derived values — complex derived resources (net income = A − B) use a C++ registration hook, not data-only expressions
- Tech tree or research gating — DiaTechTree (future) builds on top of cost tables; DiaEconomy has no knowledge of research state

## Public Interfaces

### Schema JSON Format

```json
{
  "schema_name": "standard_rts",
  "description": "A standard RTS resource set with gold, wood, and food",
  "resources": [
    {
      "resource_name": "gold",
      "description": "Primary currency for unit training and buildings",
      "minimum_value": 0.0,
      "maximum_value": 9999.0,
      "starting_value": 200.0
    },
    {
      "resource_name": "wood",
      "description": "Construction material, gathered from forests",
      "minimum_value": 0.0,
      "maximum_value": 9999.0,
      "starting_value": 100.0
    },
    {
      "resource_name": "food",
      "description": "Population capacity — consumed by units, produced by farms",
      "minimum_value": 0.0,
      "maximum_value": 200.0,
      "starting_value": 10.0
    }
  ],
  "income_rules": [
    {
      "rule_name": "base_gold_income",
      "description": "Passive gold trickle at game start",
      "resource_name": "gold",
      "amount_per_second": 5.0
    }
  ],
  "cost_tables": {
    "unit_costs": {
      "description": "Resource costs to train each unit type",
      "footsoldier": { "gold": 50.0, "food": 1.0 },
      "archer":      { "gold": 75.0, "food": 1.0, "wood": 25.0 }
    }
  },
  "modifiers": [
    {
      "modifier_name": "market_gold_bonus",
      "description": "Doubles gold income when the player owns a market building",
      "resource_name": "gold",
      "operation": "multiply_income",
      "value": 2.0,
      "when": "building.market == true"
    }
  ]
}
```

### Instance Override JSON Format

```json
{
  "schema_ref": "standard_rts",
  "instance_name": "player_one",
  "description": "Human player — starts with more gold on easy difficulty",
  "overrides": [
    {
      "resource_name": "gold",
      "starting_value": 400.0
    }
  ]
}
```

### EconomySchema / EconomyInstance

```cpp
namespace Dia::Economy {

    struct ResourceDefinition {
        StringCRC   resource_name;
        float       minimum_value;
        float       maximum_value;
        float       starting_value;
    };

    class EconomySchema {
    public:
        static EconomySchema   LoadFromJson(const char* json_path);
        const ResourceDefinition* FindResource(StringCRC resource_name) const;
        // cost table query — returns 0.0f if table or resource not found
        float                  GetCost(StringCRC table_name, StringCRC resource_name) const;
    };

    class EconomyInstance {
    public:
        static EconomyInstance CreateFromSchema(const EconomySchema& schema);
        static EconomyInstance CreateFromJson(const char* override_json_path, const EconomySchema& schema);

        float   GetValue(StringCRC resource_name) const;
        float   GetMaximum(StringCRC resource_name) const;
        float   GetMinimum(StringCRC resource_name) const;
        StringCRC GetInstanceName() const;
    };
}
```

### Transaction API

```cpp
namespace Dia::Economy {

    enum class TransactionResult {
        Success,      // full amount applied
        Clamped,      // amount was reduced due to min/max limit; event fired
        UnknownResource  // resource_name not in schema; DIA_ASSERT in debug
    };

    class EconomySystem {
    public:
        // Called by game/test code each sim tick — applies income rules and modifiers for one instance
        void              Tick    (EconomyInstance& instance, float delta_seconds);

        TransactionResult  Earn    (EconomyInstance& instance, StringCRC resource_name, float amount);
        TransactionResult  Spend   (EconomyInstance& instance, StringCRC resource_name, float amount);
        TransactionResult  Transfer(EconomyInstance& from, EconomyInstance& to, StringCRC resource_name, float amount);
        TransactionResult  SetValue(EconomyInstance& instance, StringCRC resource_name, float value); // direct set, clamped
    };
}
```

### Observer Events

```cpp
namespace Dia::Economy {

    struct PoolChangedEvent {
        const EconomyInstance* instance;
        StringCRC              resource_name;
        float                  new_value;
        float                  delta;
    };

    struct TransactionClampedEvent {
        const EconomyInstance* instance;
        StringCRC              resource_name;
        float                  requested_amount;
        float                  actual_amount;
    };

    struct TransferCompletedEvent {
        const EconomyInstance* from_instance;
        const EconomyInstance* to_instance;
        StringCRC              resource_name;
        float                  amount;
    };

    class EconomyObserverSubject : public Dia::Core::ObserverSubject {
    public:
        void Subscribe  (IEconomyObserver* observer);
        void Unsubscribe(IEconomyObserver* observer);
    };

    class IEconomyObserver {
    public:
        virtual void OnPoolChanged         (const PoolChangedEvent&) {}
        virtual void OnTransactionClamped  (const TransactionClampedEvent&) {}
        virtual void OnTransferCompleted   (const TransferCompletedEvent&) {}
        virtual void OnPoolReachedMaximum  (const EconomyInstance&, StringCRC resource_name) {}
        virtual void OnPoolReachedMinimum  (const EconomyInstance&, StringCRC resource_name) {}
    };
}
```

### Derived Resource Registration Hook

```cpp
namespace Dia::Economy {

    // Register a C++ callback to compute a derived float readable as a resource value.
    // Used for values like "net_income = gold_income - gold_upkeep" that cannot be
    // expressed purely in data.
    using DerivedResourceFn = std::function<float(const EconomyInstance&)>;

    class EconomySystem {
    public:
        void RegisterDerivedResource(StringCRC resource_name, DerivedResourceFn fn);
        float QueryDerived(const EconomyInstance& instance, StringCRC resource_name) const;
    };
}
```

## Features

| # | Feature | Description | Status |
|---|---------|-------------|--------|
| 1 | EconomySchema + ResourceDefinition | JSON asset loading, field validation, cost table queries | Draft |
| 2 | EconomyInstance | Runtime pool state, CreateFromSchema, CreateFromJson override | Draft |
| 3 | Transaction API | Earn / Spend / Transfer / SetValue / Tick with TransactionResult clamping | Draft |
| 4 | Observer events | OnPoolChanged, OnTransactionClamped, OnTransferCompleted, OnPoolReached* | Draft |
| 5 | Modifier stack | Per-pool multipliers, simple always-on + conditional via DiaCondition `when` field | Draft |
| 6 | Derived resource hook | C++ registration for computed values not expressible in data | Draft |
| 7 | Test Utilities | MockEconomyInstance, AssertPoolValue, AssertEventFired | Draft |

> **Note:** `EconomySystem` is a stateless service — it owns no tick loop and holds no instance registry. Game and test code (e.g. a SimPU module in CluicheTest or CoW) calls `Tick(instance, delta_seconds)` per instance per frame. PU wiring is the application's concern, not the engine's.

## Inherited Binding Decisions

| ID | Decision | How it applies to DiaEconomy |
|----|----------|------------------------------|
| PD-001 | Use StringCRC for all entity/component IDs | All resource names, table names, instance names, and event keys use StringCRC — no raw `const char*` or `std::string` in public APIs |
| PD-002 | ProcessingUnit/Phase/Module architecture | DiaEconomy does not provide an IModule — PU wiring is the game/test application's responsibility. `EconomySystem::Tick()` is the integration point; applications call it from their own module phase. |
| PD-004 | No STL containers in public APIs | `DynamicArrayC` for instance lists and event queues; `HashTable` for schema resource lookup; no `std::vector` or `std::unordered_map` in public headers |
| PD-007 | C++20 required language standard | Concepts used for any internal template constraints; `constexpr` for StringCRC constants; `[[nodiscard]]` on TransactionResult returns |
| AD-001 | Module system with YAML frontmatter | `dia.economy.architecture.module.md` required alongside implementation |
| AD-002 | No STL containers in public APIs | Redundant with PD-004; both apply |
| AD-003 | Namespace convention `Dia::<Module>::` | All public types live in `Dia::Economy::` |
| AD-004 | ProcessingUnit/Phase/Module for application structure | Redundant with PD-002; both apply |

## Open Design Questions

1. **Conditional modifier evaluation cost** — `when` conditions are evaluated each tick per modifier per instance. If instance count or modifier count grows large (e.g. 64 AI opponents × 20 modifiers), this becomes non-trivial. Should modifiers be dirty-flagged (only re-evaluate when a relevant blackboard key changes) rather than evaluated unconditionally each tick?

2. **Income rule granularity** — income rules use `amount_per_second` and are applied per sim tick via `delta_seconds`. If the sim tick rate is variable (e.g. slow-motion mode), does fractional accumulation need to be carried across ticks, or is per-tick truncation acceptable?

3. **DiaEconomyInspector** — a follow-on CluicheEditor panel showing per-instance pool values, income rates, and event log should be added to the backlog as a separate spec after this system ships.

## Status

`Approved`
