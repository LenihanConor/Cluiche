# System Spec: DiaAttribute

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** stats, ai

## Purpose

DiaAttribute is a generic, data-driven gameplay attribute/stat framework for the Dia engine. It gives any entity a schema-typed set of named numeric attributes (Health, Strength, FireResistance, Pace, ...) resolved from a base value plus a stack of modifiers (buffs, equipment, status effects) — for games with many similarly-shaped stat-bearing objects (RPG characters, football-card players, creature battlers). It knows nothing about UI, AI decision logic, or what a modifier's gameplay source *means* — it stores values, resolves them through a fixed evaluation pipeline, and fires discrete change events.

**Research:** docs/research/gameplay_attribut_stat_system/summary.md

Everything expressible as data is. Attribute names, ranges, and defaults live in JSON schema assets loaded once and shared across instances. Modifiers are added/removed at runtime through a symmetric handle-based API — no C++ recompile per new attribute or per new buff.

DiaAttribute is *not* a generalization of DiaEconomy, despite sharing the same idiom (StringCRC keys, JSON schema loading, DiaCondition-gated conditionals, Observer events, DiaCore containers). DiaEconomy models a flow/transaction system — pools with income rules, earn/spend/transfer, rate/cap modifiers. DiaAttribute models a composition/derivation system — a value resolved from `Base + modifier stack`, with modifiers targeting the value itself, not a rate. The two systems share no code.

**Dependency chain:**
`DiaAttribute → DiaCore (containers, StringCRC, Observer, DIA_ASSERT, DIA_LOG_*)`
`DiaAttribute → diaentitytemplate (AttributeSetComponent entity attachment)`
`DiaAttribute + DiaCondition → conditional modifiers (optional, adaptor, Feature 2) and AI accessor bridge (optional, Feature 4)`
`DiaAttribute + DiaSaveGame → ISaveable integration (optional, Feature 6) — NOT DiaSerializer's MetadataValue/MetadataArray, which caps at 8 entries and is unsuitable for bulk attribute/modifier data; see save-serialization.md`

## Responsibilities

- Define `AttributeDefinition` — StringCRC name, `minimum_value`, `maximum_value`, `default_value`
- Define `AttributeSchema` — JSON asset (`attribute_schema` type) declaring the attributes a class of object has (e.g. a Dragon or Footballer schema); loaded once, shared across instances
- Define `AttributeSet` — per-entity runtime state: one base value per attribute plus its active modifier stack; resolved through a **fixed evaluation pipeline** (`Base → Add → Multiply → Override → Clamp to [minimum_value, maximum_value]`)
- Provide `AddModifier(AttributeModifier) → ModifierHandle` / `RemoveModifier(ModifierHandle)` — symmetric lifecycle API so a modifier's source (equip a ring, apply a status effect) can add and later remove exactly one modifier without leaking or double-removing
- Provide `AttributeSetComponent` — a single `diaentitytemplate` component per entity wrapping one `AttributeSet` (not one component per attribute — respects `kMaxComponentTypesPerDomain = 64` and `kMaxEntitiesPerDomain = 1024`)
- Provide conditional modifiers — an `AttributeModifier` may carry an optional `when_condition` (DiaCondition expression); simple modifiers (no condition) always apply once added; conditional modifiers are skipped during pipeline resolution when their condition evaluates false, without being removed
- Provide Observer events — `OnAttributeChanged(entity, attribute_name, old_value, new_value)`, `OnAttributeReachedMaximum(entity, attribute_name)`, `OnAttributeReachedMinimum(entity, attribute_name)`
- Provide `AttributeAccessorBridge` — registers each entity's resolved attribute values as float accessors in `DiaCondition::ConditionRegistry` (slot+field StringCRC pairs), so `DiaRules` / `DiaUtilityAI` JSON conditions and scorers can read attributes directly
- Provide schema validation — required-field checks and range coherence (`minimum_value ≤ default_value ≤ maximum_value`) on load
- Emit `DIA_LOG_INFO` on schema load and modifier add/remove; `DIA_LOG_WARN` on schema validation warnings
- Provide test utilities under `DiaAttribute/Testing/` — `MockAttributeSet`, `AssertAttributeValue`, `AssertModifierApplied`; shipped with library, consumer opt-in via include
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.attribute.architecture.module.md` YAML module documentation
- Provide `DiaAttribute.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- **Dependency-graph derived attributes** — parked per research (`docs/research/gameplay_attribut_stat_system/`); resolution is a fixed pipeline only, no dependency tracking, no derived-attribute recalculation graph
- **Archetype/Instance layering** (shared base values across many instances, e.g. a `RedDragonArchetype`) — deferred; every `AttributeSet` owns its own full base-value storage; revisit if a large shared-population use case appears (see Open Design Questions)
- **Non-numeric typed properties** (Enum, Tag, Vector, Curve) — parked; overlaps `DiaBlackboard`'s existing typed-slot storage; no attribute type beyond float is in scope
- **Sharing code with DiaEconomy** — explicitly ruled out; different semantic model (flow/transaction vs. composition/derivation), same idiom only
- **IModule / PU wiring** — DiaAttribute provides no `IModule`; if a consumer needs time-limited modifiers (a buff that expires after N seconds), the *caller* owns the timer and calls `RemoveModifier` — DiaAttribute does not tick or expire modifiers itself
- **Gameplay behavior/rules** ("if Health < 10, flee") — belongs to `DiaRules` / `DiaUtilityAI` / `DiaBehaviourTree`; DiaAttribute only stores and resolves data
- **UI and rendering** of attribute values — belongs to game UI code or `DiaAttributeVisualDebugger`, not Core
- **Faction/ownership semantics** — not a concept here
- **Save/game serialisation of attribute state across sessions** — provided by a separate feature (`DiaAttribute` + `DiaSaveGame`'s `ISaveable` integration), not assumed by Core

## Public Interfaces

### Schema JSON Format

```json
{
  "schema_name": "dragon",
  "description": "Base attribute set shared by all dragon entities",
  "attributes": [
    { "attribute_name": "health",          "minimum_value": 0.0, "maximum_value": 2000.0, "default_value": 1000.0 },
    { "attribute_name": "strength",         "minimum_value": 0.0, "maximum_value": 200.0,  "default_value": 50.0 },
    { "attribute_name": "fire_resistance",  "minimum_value": 0.0, "maximum_value": 1.0,    "default_value": 0.5 },
    { "attribute_name": "move_speed",       "minimum_value": 0.0, "maximum_value": 20.0,   "default_value": 6.0 }
  ]
}
```

### AttributeSchema / AttributeSet

```cpp
namespace Dia::Attribute {

    struct AttributeDefinition {
        StringCRC   attribute_name;
        float       minimum_value;
        float       maximum_value;
        float       default_value;
    };

    class AttributeSchema {
    public:
        static AttributeSchema     LoadFromJson(const char* json_path);
        const AttributeDefinition* FindAttribute(StringCRC attribute_name) const;
        unsigned int                GetAttributeCount() const;
        const AttributeDefinition& GetAttributeByIndex(unsigned int index) const;
        StringCRC                  GetSchemaName() const;
        bool                       IsValid() const;
    };

    enum class ModifierOperation {
        Add,        // Base + value, evaluated in the Add stage
        Multiply,   // (Base + all Add modifiers) * value, evaluated in the Multiply stage
        Override,   // replaces the resolved value outright, evaluated last before Clamp
    };

    struct AttributeModifier {
        StringCRC          modifier_name;
        StringCRC          attribute_name;
        ModifierOperation  operation;
        float              value;
        char                when_condition[128]; // empty = always-on; non-empty = DiaCondition expression
    };

    class ModifierTag {};
    using ModifierHandle = Dia::Core::Handle<ModifierTag>;

    class AttributeSet {
    public:
        static AttributeSet CreateFromSchema(const AttributeSchema& schema);

        // Resolved value: Base -> sum(Add) -> * product(Multiply) -> Override (if any) -> Clamp
        float       GetValue(StringCRC attribute_name) const;
        float       GetBaseValue(StringCRC attribute_name) const;
        void        SetBaseValue(StringCRC attribute_name, float value);

        ModifierHandle AddModifier(const AttributeModifier& modifier);
        void           RemoveModifier(ModifierHandle handle);

        StringCRC   GetSchemaName() const;
    };
}
```

### Entity Integration

```cpp
namespace Dia::Attribute {

    // Single diaentitytemplate component per entity — wraps one AttributeSet.
    // Respects kMaxComponentTypesPerDomain = 64: never one component per attribute.
    struct AttributeSetComponent {
        AttributeSet attribute_set;
    };
}
```

### Observer Events

```cpp
namespace Dia::Attribute {

    struct AttributeChangedEvent {
        Entity      entity;
        StringCRC   attribute_name;
        float       old_value;
        float       new_value;
    };

    class IAttributeObserver {
    public:
        virtual void OnAttributeChanged        (const AttributeChangedEvent&) {}
        virtual void OnAttributeReachedMaximum (Entity entity, StringCRC attribute_name) {}
        virtual void OnAttributeReachedMinimum (Entity entity, StringCRC attribute_name) {}
    };

    class AttributeObserverSubject : public Dia::Core::ObserverSubject {
    public:
        void Subscribe  (IAttributeObserver* observer);
        void Unsubscribe(IAttributeObserver* observer);
    };
}
```

### AI / Condition Accessor Bridge

```cpp
namespace Dia::Attribute {

    class AttributeAccessorBridge {
    public:
        // Registers every attribute in `set` as a float accessor (slot_name.attribute_name)
        // in the given DiaCondition registry, so JSON conditions and DiaUtilityAI scorers
        // can read resolved attribute values directly.
        static void RegisterAccessors(Dia::Condition::ConditionRegistry& registry,
                                       const AttributeSet& set,
                                       StringCRC slot_name);
    };
}
```

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | DiaAttribute Core | `AttributeDefinition`/`AttributeSchema`/`AttributeSet`, fixed modifier pipeline, `AttributeSetComponent` entity integration | [core.md](core.md) | Approved |
| 2 | Conditional Modifiers | `when_condition` gating on `AttributeModifier` via DiaCondition; symmetric add/remove lifecycle | [conditional-modifiers.md](conditional-modifiers.md) | Approved |
| 3 | Attribute Change Notifications | `IAttributeObserver`/`AttributeObserverSubject`, `OnAttributeChanged`/`OnAttributeReachedMaximum`/`OnAttributeReachedMinimum` | [change-notifications.md](change-notifications.md) | Approved |
| 4 | AI/Blackboard Float Accessor Bridge | `AttributeAccessorBridge` registering attribute values into `DiaCondition::ConditionRegistry` via a compile-time accessor trampoline table | [accessor-bridge.md](accessor-bridge.md) | Approved |
| 5 | DiaAttributeVisualDebugger | Separate module; `IDebugDomain` overlay for live per-entity attribute + modifier-stack inspection | [visual-debugger.md](visual-debugger.md) | Approved |
| 6 | Save/Serialization Support | `Dia::SaveGame::ISaveable` integration persisting base values + active modifier state | [save-serialization.md](save-serialization.md) | Approved |

> **Note:** Features 2, 3, and 4 are independent increments on top of Feature 1 (Core) — order between them is flexible. Features 5 and 6 build on top of 2/3 (the debugger wants change events and the modifier stack visible; serialization wants a stable modifier-state shape) and should ship after the Core cluster lands.

## Inherited Binding Decisions

| ID | Decision | How it applies to DiaAttribute |
|----|----------|------------------------------|
| PD-001 | Use StringCRC for all entity/component IDs | All attribute names, modifier names, schema names, and accessor keys use StringCRC — no raw `const char*` or `std::string` in public APIs |
| PD-002 | ProcessingUnit/Phase/Module architecture | DiaAttribute provides no `IModule`; any time-limited modifier expiry is the caller's responsibility, wired into their own SimPU module |
| PD-004 | No STL containers in public APIs | `DynamicArrayC` for modifier lists and attribute definition arrays; `HashTable` for schema attribute lookup; no `std::vector`/`std::unordered_map` in public headers |
| PD-007 | C++20 required language standard | `[[nodiscard]]` on handle-returning methods; `constexpr` for StringCRC constants |
| AD-001 | Module system with YAML frontmatter | `dia.attribute.architecture.module.md` required alongside implementation |
| AD-002 | No STL containers in public APIs | Redundant with PD-004; both apply |
| AD-003 | Namespace convention `Dia::<Module>::` | All public types live in `Dia::Attribute::` |
| AD-004 | ProcessingUnit/Phase/Module for application structure | Redundant with PD-002; both apply |
| AD-005 | Component-based entities — superseded by diaentitytemplate | `AttributeSetComponent` is a `diaentitytemplate` component (`DIA_COMPONENT`/`FIELD` macros), not the old `IComponent`/`IComponentFactory` path |

## Open Design Questions

1. **Same-category modifier ordering** — the fixed pipeline (`Base → Add → Multiply → Override → Clamp`) resolves cross-category order, but if two `Add` modifiers apply to the same attribute, do they simply sum (order-independent), or does insertion/priority order matter within a category? Needs a decision before Feature 1 implementation.
2. **Time-limited modifier ownership** — Core deliberately provides no tick/expiry (Non-Responsibilities), so should `AttributeModifier` carry an optional `duration` field purely as caller-readable metadata (so every status-effect system doesn't invent its own), or should duration be entirely outside DiaAttribute's data model?
3. **Archetype/Instance Layering revisit trigger** — deferred per the research choice, not ruled out. What concrete signal (a specific game mode kickoff, a measured memory cost from many `AttributeSet` instances) should trigger reopening this design, so the deferral doesn't just get forgotten once Features 2-6 have shipped against an instance-only API?

## Status

**Status:** `Done`
**Plan:** [diaattribute.plan.md](diaattribute.plan.md)
