# Research: Choice — Gameplay Attribute/Stat System

**Date:** 2026-08-21
**Chosen candidate:** DiaAttribute System — the full buildable stack (Layers 0-2), planned as one system rather than a single feature

## Rationale

The evaluation surfaced that Candidates 1, 2, 5, 9, and 10 all scored 4.0+ and form one dependency cluster: a cheap, low-risk, PD-001/PD-004-compliant foundation (Core, bundled with its required Entity integration) immediately followed by three equally cheap, equally proven-pattern increments. Rather than choosing just the top-ranked standalone item (Core MVP) and re-running this funnel per increment, the user opted to plan the whole buildable stack — Layers 0 through 2 from the evaluate.md dependency appendix — as a single system with the candidates as its feature specs. This matches the project's own plan-workflow guidance: "System plans — one plan per system spec; tasks are the feature specs themselves."

Two candidates are explicitly excluded rather than silently dropped:
- **Candidate 4 (Archetype/Instance Layering)** is deferred, not parked — it scored below the top cluster (3.40) and CluicheTest has no near-term hundreds-of-shared-archetype-instances use case yet, but it was flagged as the one storage-model change that's cheap before Layer 1/2 exist and expensive to retrofit after. Revisit before committing Layer 1/2 specs if a concrete large-population use case appears.
- **Candidates 3 (Dependency-Graph Derived Attributes) and 6 (Non-Numeric Typed Properties)** are parked — lowest Cost/Risk scores (2.85, 3.00), no concrete driving use case, and building them now repeats the over-engineering mistake already identified earlier in this research session (extracting complexity ahead of a real need).

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 4. Archetype/Instance Layering | Deferred (not ruled out) — cheap now, expensive to retrofit later; revisit if a large-shared-archetype use case appears before Layer 1/2 ship |
| 3. Dependency-Graph Derived Attributes | Lowest score (2.85) — Cost 1, Risk 2; no game currently needs deep stat-dependency chains; EVE Dogma-style complexity would be speculative |
| 6. Non-Numeric Typed Properties | Score 3.00 — overlaps DiaBlackboard's existing typed-slot storage; no clear driving use case beyond speculative species/tag/curve data |
| Generalizing DiaEconomy into a shared base | Ruled out earlier in Explore/discussion — Economy is a flow/transaction model (pools, income rules, earn/spend/transfer), Attribute is a composition/derivation model (base + modifier stack); sharing code would bend one system's semantics to fit the other |

## Pre-Spec Commitments

- New module: **DiaAttribute** (not a generalization of or dependency on DiaEconomy — same idiom, no shared code)
- Float-only, instance-only (no archetype layer) for the Core feature — explicitly resist scope creep toward Candidate 4 or 6 during Core's spec/implementation
- Modifier evaluation is a **fixed pipeline** (Base → Flat additive → Multiplicative → Override → Clamp), not a dependency graph — Candidate 3's dependency-graph approach stays out of scope for this system entirely unless re-opened by a future research session
- AttributeSet is **one component per entity** (not one component per attribute), respecting `kMaxComponentTypesPerDomain = 64`
- Follow DiaEconomy's proven idiom throughout: StringCRC keys (PD-001), JSON schema-driven definitions, DiaCondition-gated conditional modifiers, Observer-based change events, DiaCore containers only (PD-004)
- Build order within the system: Core+Entity integration first (feature 1, bundled with 9), then Conditional Modifiers (2), Change Notifications (5), and AI/Blackboard Bridge (10) in any order, then Visual Debugger (7) and Save/Serialization (8) once the above land

## Next Step

Run `/spec-system` for **DiaAttribute**, then `/spec-feature` for each of the six feature specs in build order:
1. DiaAttribute Core (schema/definition/AttributeSet/fixed modifier pipeline + DiaEntity component integration)
2. Conditional Modifiers (DiaCondition-gated)
3. Attribute Change Notifications (Observer events)
4. AI/Blackboard Float Accessor Bridge
5. DiaAttributeVisualDebugger
6. Save/Serialization Support

Suggested parent: new system under the Dia application, sibling to DiaEconomy and DiaBlackboard.
