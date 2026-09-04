# Research: Evaluate — Gameplay Attribute/Stat System

**Input:** docs/research/gameplay_attribut_stat_system/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability
- **Game Value (0.20):** Improves CluicheTest as a demo or testbed
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure and PD-001 through PD-007

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 5. Attribute Change Notifications | 4 | 4 | 4 | 5 | 5 | **4.30** |
| 1. DiaAttribute Core (MVP) | 5 | 4 | 3 | 4 | 5 | **4.15** |
| 2. Conditional Modifiers via DiaCondition | 4 | 4 | 4 | 4 | 5 | **4.15** |
| 9. AttributeSet–DiaEntity Component Integration | 4 | 4 | 4 | 3 | 5 | **4.00** |
| 10. AI/Blackboard Float Accessor Bridge | 4 | 4 | 4 | 4 | 4 | **4.00** |
| 7. DiaAttributeVisualDebugger | 3 | 3 | 4 | 4 | 5 | **3.70** |
| 8. Save/Serialization Support | 4 | 4 | 3 | 3 | 4 | **3.60** |
| 4. Archetype/Instance Layering | 4 | 3 | 3 | 3 | 4 | **3.40** |
| 6. Non-Numeric Typed Properties | 3 | 3 | 3 | 3 | 3 | **3.00** |
| 3. Dependency-Graph Derived Attributes | 5 | 3 | 1 | 2 | 3 | **2.85** |

## Top 3 Candidates

### Rank 1: DiaAttribute Core (MVP) (score: 4.15 — promoted ahead of raw ranking)
**Why:** Candidate 5 (Attribute Change Notifications) technically scored 0.15 higher, but it is a feature *on top of* Core — it cannot be built, scored, or evaluated as standalone work without Core existing first. Every other high-scoring candidate (2, 5, 9, 10) is gated behind Candidate 1. Core alone gets the highest Engine Value (5) of any candidate because it unblocks the rest of the cluster, follows DiaEconomy's proven idiom almost exactly (satisfying PD-001 StringCRC and PD-004 no-STL directly, same as `EconomySchema.h`), and its Cost/Risk numbers (3/4) reflect genuine but bounded M-sized effort with a working reference implementation to copy from.
**Watch out for:** Scope discipline — it would be easy to pull Candidate 4 (archetype/instance layering) or Candidate 6 (non-numeric types) into "MVP" during implementation. Per the earlier over-engineering discussion, Core should ship float-only, instance-only (no archetype layer), fixed-pipeline modifiers — the smallest schema/definition/modifier-stack/component slice that Change Notifications and Conditional Modifiers can build on immediately after.

### Rank 2: Attribute Change Notifications (score: 4.30)
**Why:** Highest raw score of any candidate — it's cheap (S), nearly risk-free (it's a direct copy of `IEconomyObserver`/`OnPoolChanged`, an already-proven Observer pattern in this codebase), and it closes the exact gap explore.md flagged: Blackboard's poll-only model is a worse fit for Attribute consumers (UI health bars, AI stat-threshold reactions) than an event-based one.
**Watch out for:** Needs Core's `AttributeSet` to exist first; sequence immediately after Candidate 1, not concurrently, since the event payload shape depends on Core's final value-resolution API.

### Rank 3: Conditional Modifiers via DiaCondition (score: 4.15)
**Why:** Ties Core on raw score, S-sized, and reuses an already-integrated adaptor pattern (DiaEconomy's `when_condition` → DiaCondition) almost verbatim. Closes the "dangling modifier" pitfall identified in explore.md by forcing symmetric add/remove lifecycle for sourced modifiers (equip/unequip, status effect apply/expire).
**Watch out for:** Same sequencing dependency as Rank 2 — needs Core's modifier-stack shape finalized first so the DiaCondition gate wraps a stable modifier-application API.

## Recommendation

**DiaAttribute Core (MVP)** is the recommended next work item, despite Attribute Change Notifications scoring marginally higher in isolation — the evaluation surfaces that the entire top tier (Candidates 1, 2, 5, 9, 10, all scoring 4.0+) is really one cluster: a cheap, low-risk, PD-001/PD-004-compliant foundation (Core) immediately followed by three equally cheap, equally proven-pattern increments (Notifications, Conditional Modifiers, Entity integration). The clear loser is Candidate 3 (Dependency-Graph Derived Attributes, 2.85) — its Cost (1) and Risk (2) scores confirm the earlier judgment that building EVE Dogma-style dependency tracking now, against zero real games with deep stat-interdependency needs, would be speculative complexity the codebase doesn't yet need; it should stay parked until a concrete game surfaces the requirement.

## Appendix: Dependency Stack

The score ranking above answers "what's cheap and valuable in isolation." It doesn't answer "what needs to exist first." Layering the same 10 candidates by dependency instead of score gives a build-order view:

```
Layer 3 — Parked / speculative (no current driver, revisit only if a real game needs it)
   3. Dependency-Graph Derived Attributes  ← needs 1's value-resolution API, but complexity/risk say "wait"
   6. Non-Numeric Typed Properties          ← needs 1's storage shape; overlaps DiaBlackboard, low urgency

Layer 2 — Tooling & persistence (want Layer 1 pieces underneath them)
   7. DiaAttributeVisualDebugger    ← wants 5 (events to drive the overlay) + 2 (show active modifiers)
   8. Save/Serialization Support    ← wants 2 (modifier source/duration shape) to serialize meaningfully

Layer 1 — Cheap increments directly on Core (order between these three is flexible)
   2. Conditional Modifiers (DiaCondition)     ← needs 1's modifier-stack API finalized
   5. Attribute Change Notifications           ← needs 1's AttributeSet value-resolution API stable
  10. AI/Blackboard Float Accessor Bridge       ← needs 1's resolved values to expose

Layer 0 — Foundation (nothing else can exist without this)
   1. DiaAttribute Core (MVP)
   9. AttributeSet–DiaEntity Component Integration  ← really PART of 1, not after it —
                                                        Core isn't usable on an entity without this wiring
```

**Structural wrinkle — Candidate 4 (Archetype/Instance Layering) doesn't sit cleanly in this stack.** It's a storage-model change *underneath* Core, not a feature on top of it. It scored below the top cluster (3.40), but it's the one candidate that's cheap to do right after Layer 0 and expensive to retrofit later, because by the time Layers 1–2 exist, they've all been built against an instance-only API — reshaping storage under them means touching every consumer. The real decision isn't "is it worth building" but "do we believe we'll need it before or after Layer 1/2 land." Defer it if there's no near-term hundreds-of-shared-archetype-instances use case; build it immediately after Layer 0 if one is already known to be coming.
