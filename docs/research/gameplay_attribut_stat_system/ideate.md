# Research: Ideate — Gameplay Attribute/Stat System

**Input:** docs/research/gameplay_attribut_stat_system/explore.md

## Candidates

### Candidate 1: DiaAttribute Core (MVP)
**Home module/system:** New module — `DiaAttribute`
**Size:** M
**Description:** The foundational module: `AttributeDefinition` (StringCRC name, type, default, min/max), `AttributeSchema` (JSON-loaded, like `EconomySchema::LoadFromJson`) defining what attributes a class of object has (e.g. `DragonAttributeSchema`), and `AttributeSet` — one component per entity holding resolved values for its schema's attributes. Modifier evaluation is a **fixed pipeline** (Base → Flat additive → Multiplicative → Override → Clamp to Min/Max), float-only, no dependency graph. Follows DiaEconomy's idiom (StringCRC keys, DynamicArray/HashTable storage, JSON authoring) without sharing code.
**Primary value:** Unblocks every other candidate — gives any game object a data-driven, schema-typed set of numeric stats with buffs/equipment/status-effect modifiers, without touching C++ struct definitions per game.

### Candidate 2: Conditional Modifiers via DiaCondition
**Home module/system:** `DiaAttribute` (feature on top of Candidate 1)
**Size:** S
**Description:** Wire modifier application through DiaCondition, mirroring DiaEconomy's `when_condition` gating on `ModifierDef` — e.g. "Poison -25% MoveSpeed while `Poisoned` tag is active." Includes symmetric add/remove lifecycle for modifiers tied to a source (equip ring → +15 Strength; unequip → modifier removed exactly once).
**Primary value:** Lets status effects, equipment, and terrain/buffs drive stat changes declaratively instead of hand-written C++ per effect, and closes the "dangling modifier" bug class identified in explore.md.

### Candidate 3: Dependency-Graph Derived Attributes
**Home module/system:** `DiaAttribute` (advanced feature on top of Candidate 1)
**Size:** L
**Description:** EVE Dogma-style derived attributes — `AttackPower = Strength × WeaponPower` — with a dependency graph, dirty-flag invalidation propagation (changing Strength invalidates AttackPower without recomputing everything), and cycle detection at schema-load time. Significantly more complex than DiaEconomy's single derived-value C++ hook.
**Primary value:** Correctness and performance at scale for games with deep stat interdependencies (RPG builds, FUT chemistry/rating formulas) — avoids full-recompute-per-frame across hundreds of instances with many derived stats.

### Candidate 4: Archetype/Instance Layering
**Home module/system:** `DiaAttribute` (storage model on top of Candidate 1)
**Size:** M
**Description:** Three-tier authoring: `AttributeSchema` (what attributes exist) → `AttributeArchetype` (e.g. `RedDragonArchetype` — shared base values for a class of object) → `AttributeInstance` (per-object deltas and active modifiers only). Matches the FUT Player/Card/Owned-instance pattern and the Dragon Species/Archetype/Instance example from the source conversation.
**Primary value:** Makes "hundreds of dragons" or thousands of FUT cards memory-cheap — base schema/archetype data is shared, only per-instance deltas and modifier state are duplicated.

### Candidate 5: Attribute Change Notifications
**Home module/system:** `DiaAttribute` (feature on top of Candidate 1)
**Size:** S
**Description:** Observer-based change events (`OnAttributeChanged(entity, attribute, oldValue, newValue)`), directly mirroring DiaEconomy's `IEconomyObserver`/`OnPoolChanged`. Lets UI (health bars), AI (react to stat drop), and the future visual debugger subscribe without polling.
**Primary value:** Removes the need for every consumer to poll every frame; matches an already-proven Dia pattern (Economy, and contrasts with Blackboard's poll-only model, which explore.md flagged as a worse fit here).

### Candidate 6: Non-Numeric Typed Properties
**Home module/system:** `DiaAttribute` (storage extension)
**Size:** M
**Description:** Extend `AttributeDefinition`/`AttributeSet` to store non-numeric typed values — Enum (`Species = Fire`), Tag, Vector, Curve (e.g. damage-falloff-by-distance) — alongside numeric attributes. These are deliberately **excluded from the modifier/derivation pipeline** (per explore.md's data-not-behavior principle) — they're typed properties, not buffable stats.
**Primary value:** Covers cases like species/element/category tagging and curve-driven data (falloff, growth curves) without forcing everything into DiaBlackboard or raw JSON blobs, while keeping the modifier math numeric-only and simple.

### Candidate 7: DiaAttributeVisualDebugger
**Home module/system:** New module — `DiaAttributeVisualDebugger`
**Size:** S
**Description:** In-game overlay (`IDebugDomain`, following the same migration pattern already used for `DiaEntityVisualDebugger`, `DiaBlackboardVisualDebugger`, etc.) showing a selected entity's live attribute values, active modifier stack, and derived-attribute dependency chain.
**Primary value:** Makes stat bugs (wrong modifier order, dangling modifier, derived-value staleness) visible and debuggable in-game instead of requiring log spelunking — directly serves the "Observation Opportunity" instrumentation habit already established in this codebase.

### Candidate 8: Save/Serialization Support
**Home module/system:** `DiaAttribute`, integrating with `DiaSerializer`/`DiaSaveGame`
**Size:** S
**Description:** Persist `AttributeInstance` current values and active modifier state (source, operation, value, remaining duration if time-limited) through the existing serialization pipeline. Explicitly closes a gap DiaEconomy also left as a non-responsibility.
**Primary value:** Without this, buffs/equipment-driven stats reset on save/load — a correctness requirement for any real game built on this system, not just CluicheTest.

### Candidate 9: AttributeSet–DiaEntity Component Integration
**Home module/system:** `DiaAttribute`, wiring into `DiaEntity`
**Size:** S
**Description:** Formal integration of `AttributeSet` as a single component per entity via the current entity/component-template attachment mechanism (post-PD-003, since the old IComponent/IComponentFactory path is superseded). Respects `kMaxComponentTypesPerDomain = 64` by keeping the whole schema's attributes in one component rather than one component per attribute.
**Primary value:** Makes Attribute a first-class part of the entity system rather than a bolt-on, and avoids the component-type-cap trap explore.md flagged for "hundreds of dragons" at scale.

### Candidate 10: AI/Blackboard Float Accessor Bridge
**Home module/system:** `DiaAttribute`, bridging to `DiaCondition` / `DiaBlackboard` / `DiaUtilityAI`
**Size:** S
**Description:** Expose resolved attribute values through a float accessor registry, mirroring the existing pattern noted for the AI decision layer ("float accessor registry bridges blackboard to JSON conditions"). Lets `DiaUtilityAI` scoring curves and `DiaCondition` JSON conditions read `Health`, `Aggression`, etc. directly from an entity's AttributeSet.
**Primary value:** Connects the Attribute system into the already-built AI decision stack (DiaCondition → DiaRules/DiaUtilityAI) so AI behavior can react to gameplay stats without custom per-attribute plumbing per game.

## Coverage Map

Spans all major design axes from explore.md: storage model (1 struct-baseline, 4 archetype/instance, 6 non-numeric), modifier evaluation (2 conditional, 3 dependency-graph), change notification (5), authoring (1, JSON-driven throughout), tooling (7), persistence (8), entity integration (9), and AI/cross-system integration (10). Sizes range S→L (no XL candidate — the full system decomposes into independently shippable M/S increments once the Core MVP lands; only the dependency-graph derivation piece reaches L). Candidate 1 is the load-bearing prerequisite for 2, 3, 4, 5, 6, 9; candidates 7, 8, 10 are integration/tooling layers that can land in any order once Core exists.
