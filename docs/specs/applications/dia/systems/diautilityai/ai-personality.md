# Feature Spec: AI Personality

**Parent System:** @docs/specs/applications/dia/systems/diautilityai/diautilityai.md

**Research:** @docs/research/ai_gameplay_tools/ideate.md (Candidate 6)

## Summary

Add a `PersonalityProfile` value type to DiaUtilityAI that biases action scores and evaluation cadence without changing any `ActionDef` definitions. Profiles are named archetypes (e.g. Aggressive, Defensive, Economic) defined in JSON as per-action score weight multipliers and an integer evaluation period (ticks). A profile is passed directly to `UtilitySet::Evaluate()` as an optional last parameter; when present, multipliers are applied after all `ResponseCurve` scorers and before winner selection. `UtilitySetComponent` stores a profile pointer and passes it through on every `Evaluate()` call.

This is a thin data layer on top of the existing evaluation pipeline — no new registry, no new callbacks. The same `UtilitySet` produces different behaviour depending on which profile is active.

## Goals

- Let one `UtilitySet` definition produce distinct AI archetypes via data-only profiles
- Keep `ActionDef` and `ResponseCurve` definitions unchanged — profiles are additive multipliers, not overrides
- Support hot-swapping profiles at runtime (e.g. difficulty change mid-session, AI "mood" shift)
- Ship as part of `DiaUtilityAI.vcxproj` — no new project required

## Non-Goals

- Biasing DiaRules firing — rules are boolean, no score to multiply
- Biasing HTN method selection — different design surface, out of scope
- "Cheating" (fog access, hidden information) — belongs in game code or DiaVisibility
- Difficulty scaling of reaction time or input latency — not a utility concern
- CluicheTest entity integration — covered by the AIDecisionTestStage backlog item

## Public Interface

### PersonalityProfile

```cpp
namespace Dia::UtilityAI {
    struct ActionBias {
        Dia::Core::StringCRC actionId;
        float scoreMultiplier;  // applied after all ResponseCurve scorers, before winner selection
    };

    // Immutable after LoadFromJson. Shared across entities with the same archetype.
    class PersonalityProfile {
    public:
        // JSON format: see schema below
        static PersonalityProfile LoadFromJson(const Json::Value& root);

        Dia::Core::StringCRC GetName() const;

        // Score multiplier for a given action. Returns 1.0f if the action has no bias entry.
        float GetScoreMultiplier(Dia::Core::StringCRC actionId) const;

        // Evaluation period in ticks. 1 = every tick, 2 = every other tick, N = every Nth tick.
        // Default: 1 if absent from JSON.
        int GetEvalPeriodTicks() const;

        bool IsValid() const;
    };
}
```

### UtilitySet::Evaluate() — updated signature

`const PersonalityProfile*` added as an optional last parameter (default `nullptr`). When non-null, the profile's `GetScoreMultiplier()` is applied to each action's computed score after all `ResponseCurve` scorers and before winner selection. `nullptr` = no biasing; existing call sites are unaffected.

```cpp
namespace Dia::UtilityAI {
    class UtilitySet {
    public:
        // Sync evaluation with optional personality biasing.
        // personality == nullptr → behaviour identical to previous signature.
        UtilitySelection Evaluate(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            GroupConsiderationContext* group = nullptr,
            const PersonalityProfile* personality = nullptr) const;

        // Async path also accepts personality. Captured at submission time;
        // caller ensures the profile pointer outlives the callback.
        void EvaluateAsync(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            Dia::AIBudget::AIBudgetScheduler& scheduler,
            UtilityResultCallback callback,
            void* callbackUserData,
            GroupConsiderationContext* group = nullptr,
            const PersonalityProfile* personality = nullptr);

        // ... all other methods unchanged ...
    };
}
```

Post-bias scores (i.e. scores after personality multipliers are applied) are what gets stored in `GetLastFrameScores()` — this reflects the actual decision basis rather than the raw scorer output.

### UtilitySetComponent extension

Two new methods on `UtilitySetComponent` (no signature changes to existing methods). `Evaluate()` internally passes `mPersonality` as the last argument to `UtilitySet::Evaluate()`. The component also tracks a tick counter; evaluation is skipped when `mEvalCounter % profile->GetEvalPeriodTicks() != 0`.

```cpp
namespace Dia::UtilityAI {
    class UtilitySetComponent : public Dia::Core::IComponent {
    public:
        // ... existing API unchanged ...

        // Set active personality profile. nullptr clears it (no biasing, period = 1).
        // Profile must outlive the component.
        void SetPersonality(const PersonalityProfile* profile);
        const PersonalityProfile* GetPersonality() const;
    };
}
```

### JSON Schema

```json
{
  "name": "Aggressive",
  "eval_period_ticks": 1,
  "biases": [
    { "action": "Attack",   "multiplier": 1.5 },
    { "action": "Flee",     "multiplier": 0.2 },
    { "action": "Defend",   "multiplier": 0.6 },
    { "action": "Expand",   "multiplier": 0.8 }
  ]
}
```

```json
{
  "name": "Defensive",
  "eval_period_ticks": 3,
  "biases": [
    { "action": "Attack",   "multiplier": 0.5 },
    { "action": "Flee",     "multiplier": 1.2 },
    { "action": "Defend",   "multiplier": 1.8 },
    { "action": "Expand",   "multiplier": 1.0 }
  ]
}
```

### Test Utility extension

```cpp
// DiaUtilityAI/Testing/UtilityTestHelpers.h (extension)
namespace Dia::UtilityAI::Testing {
    // Assert that a profile changes the winner compared to no profile.
    void AssertPersonalityChangesWinner(
        const UtilitySet& set,
        const PersonalityProfile& profile,
        Dia::Condition::IConditionContext& ctx,
        const Dia::Rules::RuleActionRegistry& registry,
        Dia::Core::StringCRC expectedWinnerWithProfile);
}
```

## Binding Decisions

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| SD-001 | DiaUtilityAI | Winner-takes-all selection only in v1 | Complies. Personality biases scores but selection still returns one winner. |
| SD-005 | DiaUtilityAI | `Evaluate()` dispatches the winner via `RuleActionRegistry` inline | Complies. Profile is applied inside `Evaluate()` before dispatch — the winning action dispatched is already personality-biased. |
| SD-009 | DiaUtilityAI | `UtilitySet` immutable after `LoadFromJson()` | Complies. Profile is a separate value type passed at call time, never stored inside `UtilitySet`. |
| PD-001 | Platform | StringCRC for all entity/component IDs | Complies. `ActionBias::actionId` uses `StringCRC`. |
| PD-004 | Platform | No STL containers in public APIs | Complies. `PersonalityProfile` stores biases in `DynamicArrayC<ActionBias>`. |

## Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| FD-001 | Multiplier applied after scorers, before winner selection | Keeps `ActionDef` and `ResponseCurve` definitions archetype-agnostic. Game designers tune curves once; personality authors tune multipliers separately. |
| FD-002 | Missing action in biases → multiplier defaults to 1.0f | Profile only needs to list the actions it wants to differentiate. Unlisted actions behave as if no profile is active. |
| FD-003 | `PersonalityProfile` is immutable after `LoadFromJson()` | Consistent with `UtilitySet`, `RuleSet`, `ConditionExpr`. Shared safely across entities of the same archetype. Hot-swap = `SetPersonality()` with a different pre-loaded profile pointer. |
| FD-004 | Eval period expressed as `eval_period_ticks` integer, not a float scalar | The float formula (`counter % round(1/f)`) is broken for non-inverse-integer values (e.g. 0.75 → `round(1.33) = 1` → never skips). An integer period is deterministic, trivially implementable (`counter % evalPeriod != 0`), and easier to tune. |
| FD-005 | Eval frequency tracked by `UtilitySetComponent`, not inside `UtilitySet::Evaluate()` | `UtilitySet` stays stateless (SD-009). The component owns the frame counter. Callers that drive `Evaluate()` directly (not via the component) always evaluate every call — intentional. |
| FD-006 | Ships inside `DiaUtilityAI.vcxproj`, not a separate module | The feature is small (one value type + two new methods on an existing component). Splitting to a new project adds vcxproj overhead with no benefit. |
| FD-007 | Profile passed as optional parameter to `UtilitySet::Evaluate()`, not applied post-hoc by the component | Keeps all winner-selection logic in one place. Post-processing in the component would require re-running winner selection after `Evaluate()` already dispatched the action, which is incorrect. |
| FD-008 | Post-bias scores stored in `GetLastFrameScores()` | Reflects the actual decision basis; pre-bias scorer values are derivable independently. Biased scores are what the AI actually decided on. |

## Open Design Questions

1. **EvaluateAsync profile lifetime** — The async path captures the `PersonalityProfile*` at submission time. If the profile is hot-swapped between submission and callback, the work item still runs with the original pointer. Is this the right semantic (snapshot at submission), or should the component post the *new* profile pointer? Recommendation: snapshot at submission (simpler, no locking needed), but confirm when async path is wired to a real CluicheTest consumer.

2. **Multiplier of 0.0f vs prerequisite guard** — A multiplier of `0.0f` effectively suppresses an action for a personality, but the action still passes the prerequisite check and participates in scoring before being zeroed. Should the spec explicitly document this as the intended "soft disable" path, or should `0.0f` short-circuit scoring entirely? Decision not blocking — either way is correct, but it affects whether `GetLastFrameScores()` emits a 0.0 entry for that action.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `PersonalityProfile` — `LoadFromJson`, `GetScoreMultiplier`, `GetEvalPeriodTicks`, `IsValid`; `DynamicArrayC<ActionBias>` storage | GoogleTest: load valid JSON, assert multiplier lookup, assert default 1.0f for unlisted action, assert period defaults to 1 | Draft | sonnet | |
| 2 | `UtilitySet::Evaluate()` — add optional `const PersonalityProfile*` parameter; apply `GetScoreMultiplier()` after scorers, before winner selection; store post-bias scores in last-frame cache | GoogleTest: same UtilitySet + same context, no profile vs Aggressive profile → different winner | Draft | sonnet | Uses `AssertPersonalityChangesWinner` from task 4 |
| 3 | `UtilitySetComponent` — `SetPersonality`/`GetPersonality`; pass `mPersonality` through to `Evaluate()`; tick counter + eval period skip | GoogleTest: period=3 → `Evaluate()` called only on every 3rd tick; period=1 → called every tick | Draft | sonnet | |
| 4 | Test utility `AssertPersonalityChangesWinner` in `DiaUtilityAI/Testing/UtilityTestHelpers.h` | Used in task 2 test | Draft | haiku | |
| 5 | vcxproj — add `PersonalityProfile.h/.cpp` to `DiaUtilityAI.vcxproj` + filters | `dia check deps` clean | Draft | haiku | |

## Status

`Approved`
