# Research: Explore — Gameplay Attribute/Stat System

**Session date:** 2026-08-21
**Folder:** docs/research/gameplay_attribut_stat_system/

## Problem Space Overview

A gameplay Attribute/Stat system provides typed, named, numeric (and occasionally non-numeric) properties on game objects — Health, Strength, FireResistance, Pace, Shooting — plus the machinery to modify them (buffs, equipment, status effects, progression) and derive new values from others (AttackPower = Strength × WeaponPower). It matters wherever a game has many instances of similarly-shaped objects with data that changes at runtime through layered effects: RPGs, sports management sims (FUT-style card games), creature collectors/battlers, strategy games with unit stat blocks.

The core tension is between **flexibility** (arbitrary attributes per object type, discoverable at runtime, data-driven) and **performance/compactness** (hundreds or thousands of instances, each potentially with dozens of attributes, ticked every frame or on every mutation). A naive "bag of floats keyed by string" is flexible but slow and unstructured; a naive "struct with named fields" is fast but inflexible and requires a recompile per new attribute. Mature implementations (Unreal's GameplayAbilitySystem AttributeSet, EVE Online's Dogma, most CRPGs) converge on a three-layer model: **Definition** (what an attribute is — type, range, defaults) → **Archetype/Schema** (what attributes a class of object has, with base values) → **Instance** (one object's live values + active modifiers).

This is explicitly a **gameplay-data** system, not an instrumentation/telemetry system (that's a separate, already-considered concern — see `Metrics/Stats` in prior conversation, distinct from this). The defining architectural principle carried into this research: **attributes describe data, they do not contain behavior** — "if Health < 10, flee" belongs in AI/gameplay logic, not the attribute system.

## Existing Approaches

- **Unreal GAS (GameplayAbilitySystem) AttributeSet** — C++ structs of `FGameplayAttributeData` (base + current value), modified via `GameplayEffect` modifier specs (Add/Multiply/Override/Divide) with execution-time ordering, `PreAttributeChange`/`PostGameplayEffectExecute` hooks for derived/clamped values, attached to an `AbilitySystemComponent` per actor.
- **EVE Online Dogma** — fully data-driven attribute graph; every ship/module attribute is a node, modifiers form a dependency graph, recalculation is lazy and dependency-tracked rather than full recompute per tick.
- **Diablo/PoE-style flat stat sheets** — simpler: base + additive layer + multiplicative layer, computed top-to-bottom in a fixed order, no dependency graph (accepts recompute-everything-on-change since stat counts are small per entity).
- **Sports sim card games (FUT/Football Manager)** — three-tier authoring: Player definition (name/position/base attributes) → Card/Version (attribute deltas per rarity/edition) → Owned instance (temporary state — chemistry, morale, contracts). Attribute *definitions* are shared across millions of card instances; only deltas and instance state are per-object.
- **ECS-style dense attribute storage** — attributes stored as parallel arrays/SoA per archetype for cache-friendly bulk iteration (relevant at "hundreds of dragons" scale); trades per-object flexibility for throughput.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Storage model | Struct-of-fields (compile-time) vs. generic typed slot map (runtime) vs. SoA per-archetype | Struct-of-fields is fastest/least flexible; slot map is most flexible/slowest; SoA is a middle ground but needs archetype grouping |
| Modifier evaluation | Eager (recompute on every Set) vs. lazy/dirty-flagged (recompute on Get if invalidated) vs. dependency graph (topological recompute) | Eager is simplest, wastes cycles with many modifiers; dependency graph is EVE-style, most correct, most complex |
| Definition scope | Per-attribute (flat registry) vs. per-schema (Dragon schema, Footballer schema) | Schema-scoped avoids cross-domain name collisions and lets tooling enumerate "what attributes does a Dragon have" |
| Value types | Float-only vs. Float/Int/Bool/Enum/Vector/Curve/Tag | Float-only is simplest and covers the modifier math; non-numeric types need separate storage/no modifier stack |
| Modifier ordering | Fixed pipeline (Base→Additive→Multiplicative→Override→Clamp) vs. explicit priority per modifier | Fixed pipeline is predictable and matches PoE/Diablo; explicit priority is more powerful but harder to reason about and debug |
| Instance vs. archetype split | Every instance owns full attribute data vs. archetype holds base values, instance holds only deltas/modifiers | Critical for "hundreds of dragons" — archetype-shared base avoids duplicating static schema data thousands of times |
| Change notification | Poll-only vs. Observer/event callback (HealthChanged) | Existing DiaBlackboard precedent is poll-only; DiaEconomy precedent has Observer events — this system likely wants events (UI health bars, AI triggers) |
| Conditional modifiers | None vs. DiaCondition-gated ("Poison -25% while Poisoned tag active") | DiaEconomy already integrates DiaCondition for conditional modifiers — directly reusable pattern |
| Authoring | C++-only definitions vs. JSON schema-driven definitions/archetypes | DiaEconomy precedent is fully JSON schema-driven with only derived values needing a C++ hook — likely the right fit here too |

## Known Tradeoffs

- Dependency-graph derived attributes (AttackPower recalculates only when Strength changes) are more correct and efficient at scale but add real complexity: cycle detection, invalidation propagation, ordering guarantees. A fixed-pipeline recompute-on-read is much simpler and may be fast enough unless attribute counts get large (100+ per instance).
- Generic slot-map storage (StringCRC-keyed, like Blackboard) is flexible and matches existing engine idiom, but loses compile-time type safety and cache locality compared to a generated struct-of-fields per schema.
- JSON-schema-driven definitions (DiaEconomy's approach) keep the system generic and data-only, but push all validation to load-time and require careful schema versioning as games add attributes over their lifecycle.
- Supporting non-float types (Enum, Vector, Curve, Tag) alongside floats broadens applicability (status flags, direction vectors, damage-falloff curves) but the modifier/derivation machinery only meaningfully applies to numeric types — mixed-type systems tend to bifurcate into "numeric attributes with modifiers" and "typed properties without modifiers," which is more surface area to design and document.
- Archetype/instance layering (schema → archetype → instance) minimizes memory for large populations but adds a lookup indirection on every read unless instances cache resolved values.

## Known Pitfalls (C++ / game engine context)

- Modifier order-of-operations bugs: `(Base + Flat) × Multiplier` vs `Base × Multiplier + Flat` give different results — must be specified and tested explicitly, not left to insertion order.
- Recursive/cyclic derived attributes (A depends on B depends on A) will infinite-loop or silently give stale values without cycle detection.
- Floating-point modifier stacking (many small percentage modifiers) accumulates rounding error — needs a defined evaluation order and possibly fixed-point or double intermediate math for consistency.
- Dangling modifier lifetime: a modifier tied to a status effect/equipment must be removed exactly once when the source expires — a common source of "stat stuck at wrong value" bugs if removal isn't symmetric with application.
- StringCRC collisions across schemas (Dragon's `Speed` vs Footballer's `Pace` hashing differently is fine, but two authors independently naming an attribute `Speed` under different schemas can collide if the registry isn't schema-scoped) — needs a scoping strategy (schema-qualified CRC or per-schema registry).
- Thread-safety: DiaEconomy explicitly has no cross-thread guarantees stated; if Attribute reads happen from Render/UI while Sim mutates them, this needs an explicit answer (matches [[feedback_simpu_game_logic]] — game logic on SimPU, so likely all mutation is sim-thread and reads elsewhere need a defined sync point, similar to how frame data is snapshotted for Render).

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaEconomy** | Closest existing analogue in *idiom only* — StringCRC-keyed schema definitions, JSON schema loading, DiaCondition-gated modifier stack, Observer change events, DiaCore containers. Semantically a different system: Economy is a **flow/transaction model** (pools with min/max bounds, `IncomeRule` accrual per second, `Earn`/`Spend`/`Transfer`, cost tables, modifiers target the *rate/cap* of a rule) vs. Attribute's **composition/derivation model** (a value resolved from `Base + modifier stack`, read-driven not tick-accrued, modifiers target the *value itself*, dependency-aware derived attributes). **Decision: no shared code/base class with Economy — build Attribute as its own module following the same idiom** (StringCRC schema, JSON-driven definitions, DiaCondition-gated modifiers, Observer events), not a generalization of Economy. Revisit extraction only if a third system needs the identical shape. |
| **DiaBlackboard** | Typed named-slot store precedent (StringCRC keys, Register/Get/TryGet, per-entity + global). Poll-only, no modifiers, no schema/archetype concept — a simpler sibling pattern, not a fit for the modifier/derivation requirements here but relevant for "how does Dia already do typed keyed storage." |
| **DiaCondition** | Existing conditional-evaluation module already wired into DiaEconomy for `when_condition` modifier gating — directly reusable for "Poison -25% while tag active" style conditional modifiers. |
| **DiaEntity** | Entities are `Dia::Core::Handle<EntityTag>`, capped at `kMaxEntitiesPerDomain = 1024` and `kMaxComponentTypesPerDomain = 64` per domain. For "hundreds of dragons," an AttributeSet should be **one component per entity** (not one component per attribute) — the component-type cap makes per-attribute components a non-starter at scale. |
| **DiaCore (StringCRC, DynamicArray, HashTable)** | PD-001-compliant identifier type and containers already used identically by DiaEconomy's schema — direct precedent to follow for definitions/schemas storage. |
| **DiaSerializer / DiaSaveGame** | Attribute instance state (current values, active modifiers) will need save/load and possibly network replication eventually — DiaEconomy explicitly lists save/game serialization as a non-responsibility, so this is an open gap for either system. |
| **Component system (IComponent/IComponentObject)** | PD-003 (superseded by diaentitytemplate per platform spec) — the AttributeSet would attach to an entity as a component; needs to follow whatever the current entity/component attachment pattern is (DiaEntity + component template), not the superseded IComponent path. |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Attribute names, schema names, and modifier operation names must be StringCRC, not raw strings — matches DiaEconomy precedent exactly. |
| PD-002 ProcessingUnit/Phase/Module | Any per-frame modifier evaluation (e.g., time-based effect expiry) runs as a Module tick on the appropriate PU — per [[feedback_simpu_game_logic]], gameplay attribute mutation belongs on SimPU. |
| PD-003 Component-based entities (superseded) | AttributeSet attaches as a component via the current entity/component template mechanism, not the superseded IComponent/IComponentFactory path — needs confirming against DiaEntity's current attachment pattern before spec. |
| PD-004 No STL in public APIs | Definitions/schemas/instances must use DiaCore containers (DynamicArray, HashTable) — direct precedent in `EconomySchema.h`. |
| PD-007 C++20 | No blocker; could use concepts for compile-time attribute-type constraints if desired, but not required. |

## Open Questions for Ideation

- Is a **dependency-graph derived-attribute system** (EVE Dogma-style) worth the complexity now, or should the first version ship with a **fixed-pipeline recompute-on-read** model (simpler, matches PoE/Diablo, extensible later)?
- How much of the **archetype/instance split** (schema → archetype → instance) is needed for CluicheTest's current scale vs. deferred until a real "hundreds of dragons" or FUT-style game exists? Building it too early risks over-engineering against an application that doesn't exist yet.
- Should non-numeric attribute types (Enum, Tag, Curve) be in scope for a first version, or should the first version be **numeric-only** (float/int, with modifiers) and typed/tag properties left to DiaBlackboard or a future extension?
- What's the right home module name — `DiaAttribute` (matches "attributes describe data" framing from the source conversation) vs. something that signals its relationship to DiaEconomy?
