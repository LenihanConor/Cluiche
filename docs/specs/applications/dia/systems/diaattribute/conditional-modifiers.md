# Feature Spec: Conditional Modifiers

## Parent System
@docs/specs/applications/dia/systems/diaattribute/diaattribute.md

## Problem Statement

Core (`AttributeModifier.when_condition`) carries a field for conditional gating but does not evaluate it — every modifier Core adds is always-on. Real modifiers are frequently conditional: "Poison -25% MoveSpeed while `Poisoned` tag is active," "Berserk +30% Strength below 20% Health." Without gating, the caller is forced to manually `AddModifier`/`RemoveModifier` every time the underlying condition flips, which is exactly the symmetric-lifecycle bug class (dangling/double-remove) explore.md flagged. This feature makes the modifier stay registered and lets its *contribution* switch on and off via `DiaCondition`, without touching its lifecycle.

**Direction note:** this feature *consumes* `DiaCondition::ConditionRegistry` — it evaluates expressions that reference accessors already registered by other systems (blackboard tags, other conditions). It is the opposite direction from Feature 4 (AI/Blackboard Accessor Bridge), which *produces* accessors so other systems can read Attribute's values. The two features do not depend on each other.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `AttributeModifier.when_condition` empty → modifier always applies, matching Core's existing unconditional behavior exactly (no regression) | Add a modifier with empty `when_condition`; behavior identical to Core AC-5/AC-6 |
| AC-2 | Non-empty `when_condition` → the pipeline skips this modifier's contribution when the condition evaluates `false`, without calling `RemoveModifier` — the modifier stays registered, just inactive | Add a conditionally-gated Add modifier with the condition false; assert `GetValue()` excludes it; assert the modifier is still present via a query/count |
| AC-3 | When the condition flips false→true between two `GetValue()` calls, the modifier's contribution appears on the very next resolve — no re-registration required | Flip the backing condition state (e.g. a blackboard tag) between two `GetValue()` calls; assert the second reflects the modifier |
| AC-4 | A condition referencing an accessor unresolvable in the supplied `ConditionRegistry` is rejected at `AddModifier` time via `ConditionExpr::Validate()` — `AddModifier` returns an invalid `ModifierHandle` and logs `DIA_LOG_WARN`; the modifier is never added, so `Evaluate()` never runs against an unresolvable leaf | Add a modifier whose condition references a nonexistent accessor; assert the returned handle is invalid and no entry was added |
| AC-5 | A malformed `when_condition` expression (parse failure in `ConditionExpr::LoadFromJson`) is likewise caught at `AddModifier` time — same rejection path as AC-4 | Add a modifier with unparseable condition text; assert the returned handle is invalid |
| AC-6 | This feature is purely additive to Core's data model — no change to `AttributeModifier`'s field layout; only the pipeline's evaluation step and `AddModifier`-time parse validation are new | Diff the struct before/after this feature; no field changes |
| AC-7 | Equip/unequip symmetry holds regardless of condition state at removal time: adding a conditionally-gated modifier then removing it via its handle leaves `GetValue()` at exactly its pre-add value, whether the condition was true or false at the moment of removal | Add a conditional modifier, remove it while its condition is true; separately, add and remove while false; assert both leave identical resolved values |

## Design

### Parsed-expression caching and eager validation

`when_condition` text is parsed via `Dia::Condition::ConditionExpr::LoadFromJson` once, at `AddModifier` time, and the parsed tree is cached alongside the `ModifierEntry` (not re-parsed on every `GetValue()` call). Immediately after parsing, `AddModifier` calls `parsed_condition.Validate(registry, outErrors)` — per `ConditionExpr`'s actual contract, this checks every leaf slot/field pair is resolvable in the given registry and reports errors without needing to evaluate anything. A parse failure or a validation failure both reject the `AddModifier` call outright (AC-4/AC-5) — this feature does **not** rely on any runtime "fails closed on missing accessor" behavior at `Evaluate()` time, because `ConditionRegistry`'s actual contract (`SD-009`) is the opposite: `GetFloat`/`GetBool` **assert in Debug** and return `0.0f`/`false` in Release on a missing key. Catching unresolvable accessors at `AddModifier` time via `Validate()` avoids ever hitting that assert during normal operation.

```cpp
struct ModifierEntry {
    ModifierHandle                 handle;
    AttributeModifier              modifier;
    Dia::Condition::ConditionExpr  parsed_condition; // default-constructed (always-true-eligible) if when_condition was empty
};
```

### Pipeline change

The resolution pipeline (Core's Design section) adds one filter before summing Add / multiplying Multiply / checking Override: a modifier is included in this resolve only if its `when_condition` was empty, or `parsed_condition.Evaluate(registry) == true`. Because every conditional modifier was validated at `AddModifier` time, `Evaluate()` only ever runs against accessors already confirmed resolvable — the `SD-009` assert path should not be reachable through this feature in normal operation. It remains reachable if the registry's accessor set shrinks after validation (an accessor is removed while a modifier that depends on it is still live) — see Open Design Question #2, which is the accessor-bridge feature's problem, not this one's, since `ConditionRegistry` has no unregister API today.

### Relationship to DiaEconomy's identical unresolved question

DiaEconomy's own system spec left "conditional modifiers evaluated every tick per modifier per instance — dirty-flag instead?" as an open question (its ODQ #1). This feature inherits the same tension and makes the same call DiaEconomy's spec left open: evaluate on every `GetValue()` call for v1, revisit with dirty-flagging only if profiling shows it matters. Do not build dirty-flagging speculatively.

## Open Design Questions

1. **Evaluation cost at scale** — same shape as DiaEconomy's open question: unconditional per-`GetValue()` evaluation is simplest but could matter with many conditional modifiers across many entities. Recommend: ship unconditional evaluation, measure, revisit only if profiling shows a real cost.
2. **Accessor lifetime after validation** — `ConditionRegistry` has no unregister method, so an accessor validated as resolvable at `AddModifier` time cannot currently disappear later through DiaCondition's own API. If the accessor-bridge feature (Feature 4) or any other registry owner ever adds a way to remove accessors, a conditional modifier validated against a now-gone accessor would hit the `SD-009` assert path on its next `Evaluate()`. Not this feature's problem to solve, but any future "remove accessor" capability must account for live conditional modifiers still holding a validated-but-now-stale expression.

## Status

**Status:** `Approved`
