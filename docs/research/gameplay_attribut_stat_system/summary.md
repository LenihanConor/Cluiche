# Research Summary — Gameplay Attribute/Stat System

**Session folder:** docs/research/gameplay_attribut_stat_system/
**Date:** 2026-08-21

## One-Line Answer

Build **DiaAttribute** as a new system — a data-driven gameplay stat framework (schema/definition → AttributeSet instance → modifier stack) for entities like dragons, football-card players, or RPG characters — following DiaEconomy's proven idiom (StringCRC, JSON schema, DiaCondition-gated modifiers, Observer events) without sharing code with it.

## Journey

1. **Explored:** Surveyed gameplay Attribute/Stat systems (Unreal GAS, EVE Dogma, PoE-style flat stat sheets, FUT-style archetype/instance authoring) and found DiaEconomy as a close *structural* analogue already in the codebase — but a semantically different flow/transaction model (pools, income rules, earn/spend/transfer) vs. Attribute's composition/derivation model (base + modifier stack resolved to a value). Also surfaced a hard entity-system constraint: `kMaxComponentTypesPerDomain = 64`, meaning AttributeSet must be one component per entity, not one per attribute.
2. **Ideated:** Generated 10 candidates spanning the Core module itself, conditional/gated modifiers, dependency-graph derived attributes, archetype/instance layering, change notifications, non-numeric typed properties, a visual debugger, save/serialization, entity integration, and an AI/Blackboard bridge. Sizes ranged S→L with no XL — the system decomposes cleanly into independently shippable increments once Core exists.
3. **Evaluated:** Scored all 10 on Engine Value/Game Value/Cost/Risk/Fit. Top scorer in isolation was Attribute Change Notifications (4.30), but a dependency-stack analysis showed it — along with Conditional Modifiers, Entity Integration, and the AI Bridge — is gated behind DiaAttribute Core (MVP), which was promoted to the recommended starting point. Lowest scorer, Dependency-Graph Derived Attributes (2.85), confirmed the earlier judgment against building EVE Dogma-style complexity without a concrete driving use case.
4. **Chose:** Rather than picking a single candidate, the user opted to plan the whole buildable stack (Layers 0–2) as one system with six feature specs in build order, explicitly deferring Archetype/Instance Layering (revisit if a large-shared-population use case appears) and parking the two speculative candidates (Dependency-Graph Derived Attributes, Non-Numeric Typed Properties).

## Chosen Work Item

**Name:** DiaAttribute (system)
**Home module:** New module — sibling to DiaEconomy and DiaBlackboard under the Dia application
**Suggested spec type:** System, with 6 child feature specs
**Estimated size:** System-level; individual features range S (most) to M (Core)

**Feature specs in build order:**
1. DiaAttribute Core — schema/definition/AttributeSet/fixed modifier pipeline, bundled with DiaEntity component integration
2. Conditional Modifiers (DiaCondition-gated)
3. Attribute Change Notifications (Observer events)
4. AI/Blackboard Float Accessor Bridge
5. DiaAttributeVisualDebugger
6. Save/Serialization Support

## Key Insights from Exploration

- DiaEconomy is the right idiom reference (StringCRC, JSON schema, DiaCondition gating, Observer events, DiaCore containers) but the wrong code to generalize from — its verbs (Tick/Earn/Spend/Transfer, rate/cap modifiers) model flow, not value composition. Building Attribute as a "generalized Economy" would bend Attribute's semantics to fit Economy's shape.
- Only one existing concrete precedent (Economy) plus zero built instances of Attribute means it's premature to extract a shared base "data store" module now — wait for a third system with the identical shape before extracting.
- `kMaxComponentTypesPerDomain = 64` and `kMaxEntitiesPerDomain = 1024` are hard constraints that rule out any design where each attribute is its own component — AttributeSet must be a single component per entity holding the whole schema.
- Modifier evaluation order is a known bug class (`(Base+Flat)×Mult` vs `Base×Mult+Flat` give different results) — the fixed pipeline (Base → Flat → Multiplicative → Override → Clamp) must be specified explicitly and tested, not left to insertion order.
- Dependency-graph derived attributes (EVE Dogma-style) and non-numeric typed properties are real capabilities but have no current driving use case in CluicheTest — parked rather than built speculatively.
- Archetype/Instance Layering (schema → archetype → instance, for large shared-base populations) is the one deferred candidate that's cheap to build immediately after Core and expensive to retrofit once Layer 1/2 consumers exist against an instance-only API — worth revisiting before those land if a large-population game mode is already planned.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Dependency-Graph Derived Attributes | Lowest score (2.85); Cost 1, Risk 2; no concrete stat-interdependency use case yet |
| Non-Numeric Typed Properties | Score 3.00; overlaps DiaBlackboard's existing typed-slot storage; no clear driving use case |
| Generalizing DiaEconomy into a shared base | Semantically different systems (flow/transaction vs. composition/derivation); would bend one system's meaning to fit the other |
| Archetype/Instance Layering | Not discarded — deferred; cheap now, expensive later, revisit if a large-shared-population need appears before Layer 1/2 ship |

## References

- docs/research/gameplay_attribut_stat_system/explore.md
- docs/research/gameplay_attribut_stat_system/ideate.md
- docs/research/gameplay_attribut_stat_system/evaluate.md
- docs/research/gameplay_attribut_stat_system/choose.md
