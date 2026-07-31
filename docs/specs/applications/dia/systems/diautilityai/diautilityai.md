# System Spec: DiaUtilityAI

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaUtilityAI is the score-based action selection layer for the Dia engine. Where DiaRules fires every matching rule, DiaUtilityAI selects the single best action each evaluation by scoring all eligible candidates and returning the winner (winner-takes-all, v1).

Each action candidate (`ActionDef`) has a prerequisite guard (from DiaCondition), one or more response curves that map context values to scores (multiplied together), a cooldown, and an optional max-concurrent cap. `UtilitySet` is a loadable collection of `ActionDef`s. `GroupConsiderationContext` tracks active action counts across a squad so the cap is enforced group-wide, not just per-entity.

Score overlay visualisation ships as a separate `DiaUtilityAIVisualDebugger` module — same pattern as `DiaRigidBody2DVisualDebugger`.

**Research:** @docs/research/ai_gameplay_tools/ideate.md (Candidate 2), @docs/research/ai_gameplay_tools/evaluate.md

**Dependency chain:**
`DiaUtilityAI → DiaCondition (ConditionExpr, IConditionContext)`
`DiaUtilityAI → DiaRules (RuleActionRegistry — action dispatch)`
`DiaUtilityAI → DiaAIBudget (AIBudgetScheduler — async evaluation path)`
`DiaUtilityAI → DiaCore (StringCRC, DynamicArrayC, Json)`
`DiaUtilityAI → DiaEntity (UtilitySetComponent via IComponent)`
`DiaUtilityAIVisualDebugger → DiaUtilityAI + DiaCore (IVisualDebugger, IDebugDraw)`

## Responsibilities

- Provide a `ResponseCurve` value type: maps a normalised float input (0–1) to a score via a named easing shape; data-driven from JSON
- Support the following curve shapes in v1: `linear`, `quadratic`, `exponential`, `logistic`, `sine`, `step`
- Provide an `ActionDef` value type: `StringCRC` action ID, `ConditionExpr` prerequisite, one or more `ResponseCurve` scorers with an `IConditionContext` slot/field source per scorer, cooldown in seconds, and `max_concurrent` cap (0 = unlimited)
- Provide a `UtilitySet` class that loads from JSON and evaluates all eligible `ActionDef`s, returning the single highest-scoring action (winner-takes-all)
- Score eligibility: prerequisite guard must pass and cooldown must have expired; skip actions at or over `max_concurrent` against the supplied `GroupConsiderationContext`
- Return a `UtilitySelection` (winning `StringCRC` action ID + score, or `kInvalidCRC` + 0.0f if nothing eligible)
- Dispatch the winning action via `RuleActionRegistry` from within `Evaluate()`
- Provide a sync `Evaluate()` path and an async `EvaluateAsync()` path that submits to `AIBudgetScheduler` and delivers the result via callback
- Provide a `GroupConsiderationContext` value type: tracks active action counts by `StringCRC`; `Increment`/`Decrement` called by game code; read during eligibility check
- Provide a `UtilitySetComponent` (`IComponent`) for attaching a `UtilitySet` to a `DiaEntity`
- Provide a `DiaUtilityAIVisualDebugger` separate static library: implements `IVisualDebugger`, reads last-frame scores from `UtilitySet`, draws per-action score bars via `IDebugDraw`; guarded by `#ifdef DIA_DEBUG`
- Provide test utilities under `DiaUtilityAI/Testing/`: `MockUtilityContext`, `AssertWinner`, `AssertNoSelection`, curve evaluation helpers
- Identify all action IDs via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.diautilityai.architecture.module.md` YAML module documentation
- Provide `DiaUtilityAI.vcxproj` and `DiaUtilityAIVisualDebugger.vcxproj` static libraries registered in `Cluiche.sln`

## Non-Responsibilities

- All-matching rule evaluation — DiaRules
- Condition/expression evaluation logic — DiaCondition
- Blackboard slot registration — DiaBlackboard
- Budget enforcement or time-slicing logic — DiaAIBudget
- Thread-safe evaluation — caller's responsibility; `Evaluate()` is single-threaded
- Top-N action selection (fire best N simultaneously) — deferred; winner-takes-all only in v1
- Personality biases or difficulty scaling of utility scores — future DiaAIPersonality system
- Behaviour trees or hierarchical task networks — separate systems

## Public Interfaces

### ResponseCurve

```cpp
namespace Dia::UtilityAI {
    enum class CurveShape {
        kLinear,
        kQuadratic,
        kExponential,
        kLogistic,
        kSine,
        kStep
    };

    // Maps a normalised float input [0,1] → score [0,1].
    // Additional parameters (exponent, midpoint, steepness) are curve-specific.
    class ResponseCurve {
    public:
        float Evaluate(float normalisedInput) const;

        // JSON format: { "shape": "quadratic", "exponent": 2.0, "invert": false }
        static ResponseCurve LoadFromJson(const Json::Value& node);

        CurveShape GetShape() const;
    };
}
```

### ActionDef

```cpp
namespace Dia::UtilityAI {
    struct ScorerDef {
        Dia::Core::StringCRC slot;    // IConditionContext slot to read
        Dia::Core::StringCRC field;   // field within slot
        float inputMin;               // raw value mapped to 0
        float inputMax;               // raw value mapped to 1
        ResponseCurve curve;
    };

    struct ActionDef {
        Dia::Core::StringCRC actionId;
        Dia::Condition::ConditionExpr prerequisite;
        Dia::Core::DynamicArrayC<ScorerDef> scorers;  // product of all scorer outputs
        float cooldownSeconds;                         // 0 = no cooldown
        int   maxConcurrent;                           // 0 = unlimited
    };
}
```

### GroupConsiderationContext

```cpp
namespace Dia::UtilityAI {
    // Tracks active action counts across a group (e.g. a squad).
    // Game code owns and drives Increment/Decrement; UtilitySet reads counts
    // during eligibility checks.
    // NOT a singleton — caller creates, owns, and passes by reference.
    class GroupConsiderationContext {
    public:
        void Increment(Dia::Core::StringCRC actionId);
        void Decrement(Dia::Core::StringCRC actionId);
        int  GetCount(Dia::Core::StringCRC actionId) const;
        void Reset();
    };
}
```

### UtilitySet

```cpp
namespace Dia::UtilityAI {
    struct UtilitySelection {
        Dia::Core::StringCRC actionId;  // kInvalidCRC if nothing eligible
        float score;                    // 0.0f if nothing eligible
    };

    using UtilityResultCallback = void(*)(UtilitySelection result, void* userData);

    class UtilitySet {
    public:
        // Sync evaluation: score all eligible actions, dispatch the winner,
        // return the selection. Dispatches winner via registry immediately.
        UtilitySelection Evaluate(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            GroupConsiderationContext* group = nullptr) const;  // nullptr = no group cap

        // Async: submit evaluation work to AIBudgetScheduler.
        // Callback fires on the calling thread when the scheduler drains the work item.
        void EvaluateAsync(
            Dia::Condition::IConditionContext& ctx,
            const Dia::Rules::RuleActionRegistry& registry,
            void* actionContext,
            Dia::AIBudget::AIBudgetScheduler& scheduler,
            UtilityResultCallback callback,
            void* callbackUserData,
            GroupConsiderationContext* group = nullptr);

        // JSON format: { "actions": [ { "id": "...", "prerequisite": <ConditionExpr>,
        //   "scorers": [...], "cooldown": 0.5, "max_concurrent": 2 } ] }
        static UtilitySet LoadFromJson(const Json::Value& root);

        bool Validate(
            const Dia::Condition::ConditionRegistry& registry,
            Dia::Core::DynamicArrayC<const char*>& outErrors) const;

        int GetActionCount() const;

        // Last-frame scores — read by DiaUtilityAIVisualDebugger.
        // Only populated after at least one Evaluate() call.
        void GetLastFrameScores(
            Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& outIds,
            Dia::Core::DynamicArrayC<float>& outScores) const;
    };
}
```

### UtilitySetComponent

```cpp
namespace Dia::UtilityAI {
    class UtilitySetComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"UtilitySetComponent"};

        void SetUtilitySet(UtilitySet&& utilitySet);
        void SetRegistry(const Dia::Rules::RuleActionRegistry* registry);
        void SetGroupContext(GroupConsiderationContext* group);  // optional

        // Evaluate with the given context and action context.
        UtilitySelection Evaluate(
            Dia::Condition::IConditionContext& ctx,
            void* actionContext) const;

        const UtilitySet* GetUtilitySet() const;
    };
}
```

### DiaUtilityAIVisualDebugger (separate module)

```cpp
// DiaUtilityAIVisualDebugger/UtilityScoreDrawer.h
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>
namespace Dia::UtilityAI { class UtilitySet; }

namespace Dia::UtilityAI {
    class UtilityScoreDrawer : public Dia::Debug::IVisualDebugger {
    public:
        explicit UtilityScoreDrawer(const UtilitySet& utilitySet,
                                    const Dia::Core::IDebugContext& debugCtx);
        Dia::Core::StringCRC GetLayerName() const override; // "utilityai.scores"
        void Draw(Dia::Core::IDebugDraw& draw) override;    // per-action score bars
        void DrawImGui() override;                          // score table
    private:
        const UtilitySet& mUtilitySet;
    };
}
#endif
```

### JSON Schema

```json
{
  "actions": [
    {
      "id": "Attack",
      "prerequisite": { "slot": "enemy", "field": "visible", "op": "==", "value": true },
      "scorers": [
        {
          "slot": "health", "field": "value",
          "input_min": 0.0, "input_max": 100.0,
          "curve": { "shape": "linear", "invert": false }
        },
        {
          "slot": "enemy", "field": "distance",
          "input_min": 0.0, "input_max": 20.0,
          "curve": { "shape": "quadratic", "exponent": 2.0, "invert": true }
        }
      ],
      "cooldown": 0.5,
      "max_concurrent": 2
    },
    {
      "id": "Flee",
      "prerequisite": { "slot": "health", "field": "value", "op": "<", "value": 20.0 },
      "scorers": [
        {
          "slot": "health", "field": "value",
          "input_min": 0.0, "input_max": 20.0,
          "curve": { "shape": "logistic", "invert": true }
        }
      ],
      "cooldown": 1.0,
      "max_concurrent": 0
    }
  ]
}
```

### Test Utilities

```cpp
// DiaUtilityAI/Testing/UtilityTestHelpers.h
namespace Dia::UtilityAI::Testing {
    // Assert that Evaluate returns a specific winner.
    void AssertWinner(const UtilitySet& set,
                      Dia::Condition::IConditionContext& ctx,
                      const Dia::Rules::RuleActionRegistry& registry,
                      Dia::Core::StringCRC expectedActionId);

    // Assert that Evaluate returns no eligible action.
    void AssertNoSelection(const UtilitySet& set,
                           Dia::Condition::IConditionContext& ctx,
                           const Dia::Rules::RuleActionRegistry& registry);

    // Evaluate a single ResponseCurve and assert output within tolerance.
    void AssertCurveOutput(const ResponseCurve& curve,
                           float input,
                           float expectedOutput,
                           float tolerance = 0.001f);
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Response Curve | `ResponseCurve` — maps normalised float input to score via named easing shape. Six shapes in v1. JSON-loadable. | [response-curve.md](response-curve.md) | Draft |
| Action Definition | `ActionDef` — prerequisite guard, scored inputs, cooldown, max-concurrent cap. `ScorerDef` binds a `ResponseCurve` to a context slot/field with normalisation range. | [action-def.md](action-def.md) | Draft |
| Utility Set | `UtilitySet` — JSON-loadable collection of `ActionDef`s. Winner-takes-all sync `Evaluate()`. Dispatches winning action via `RuleActionRegistry`. Exposes last-frame scores for debug. | [utility-set.md](utility-set.md) | Draft |
| Async Evaluation | `EvaluateAsync()` — submits evaluation work to `AIBudgetScheduler`; result delivered via callback. Prevents utility evaluation from starving other SimPU work. | [async-evaluation.md](async-evaluation.md) | Draft |
| Group Consideration | `GroupConsiderationContext` — tracks active action counts across a group; enforces `max_concurrent` cap group-wide, not just per-entity. | [group-consideration.md](group-consideration.md) | Draft |
| Utility Set Component | `UtilitySetComponent` (`IComponent`) — attaches a `UtilitySet` + registry + optional group context to a `DiaEntity`. | [utility-set-component.md](utility-set-component.md) | Draft |
| Score Overlay | `DiaUtilityAIVisualDebugger` separate static library — `UtilityScoreDrawer` implements `IVisualDebugger`, reads last-frame scores, draws per-action score bars + ImGui table. `#ifdef DIA_DEBUG` guarded. | [score-overlay.md](score-overlay.md) | Draft |
| Test Utilities | `DiaUtilityAI/Testing/` — `AssertWinner`, `AssertNoSelection`, `AssertCurveOutput`. Ships with library; consumer opt-in via include. | [test-utilities.md](test-utilities.md) | Draft |
| AI Personality | `PersonalityProfile` — per-action score multipliers + eval period (ticks). Applied by `UtilitySet::Evaluate()` after scorers, before winner selection. JSON-loadable named archetype. | [ai-personality.md](ai-personality.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaCondition** — `ConditionExpr` (prerequisite guards), `IConditionContext` (scorer input source), `ConditionRegistry`
- **DiaRules** — `RuleActionRegistry` (action dispatch — reuses the same handler table pattern)
- **DiaAIBudget** — `AIBudgetScheduler` (async evaluation path)
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `Json`, `IComponent`, `ComponentFactoryRegistry`, `DIA_ASSERT`
- **DiaEntity** — `UtilitySetComponent` via `IComponent`

**DiaUtilityAIVisualDebugger additionally requires:**
- **DiaCore** — `IVisualDebugger`, `IDebugDraw`, `IDebugContext` (all in `DiaCore/DebugDraw/`)

**Explicitly excluded from DiaUtilityAI (core):**
- **DiaVisualDebugger** — no direct dependency; debug drawing lives in the separate `DiaUtilityAIVisualDebugger` module
- **DiaApplicationFlow** — no lifecycle dependency; evaluation called by game code within phases/modules

**Dependents:**
- Game code (CluicheTest and future games) — registers action handlers, attaches `UtilitySetComponent`, manages `GroupConsiderationContext`
- Future DiaAIPersonality — may bias scores before winner selection

## Out of Scope

- Top-N action selection (fire best N simultaneously) — deferred; winner-takes-all only in v1
- Personality / difficulty biasing of utility scores — future DiaAIPersonality system
- Behaviour trees or GOAP — separate systems
- Thread-safe evaluation — caller synchronizes
- Hot-reload of utility definitions while evaluating — reload on next construct
- Integer or string value types in scorer inputs — float only in v1 (consistent with DiaCondition)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Winner-takes-all selection only in v1 | Covers the common strategy game case cleanly. Top-N overlaps with DiaRules' all-matching model; defer until a concrete use-case exists. | UtilitySet | Accepted | Yes |
| SD-002 | Scorer inputs normalised to [0,1] before curve evaluation | Curves are always defined over [0,1]; normalisation range (`input_min`, `input_max`) is per-scorer in JSON. Decouples curve shape definition from the scale of the underlying data. | ResponseCurve, ActionDef | Accepted | Yes |
| SD-003 | Multiple scorers per action — product of outputs | Multiplicative combination means any scorer approaching 0 suppresses the action. Additive would require weights and normalisation. Product is simpler to reason about and tune. | ActionDef | Accepted | Yes |
| SD-004 | `GroupConsiderationContext` is explicit, not a singleton | Avoids hidden global state. Same pattern as `ConditionRegistry` and `RuleActionRegistry`. Squad-level context is caller-owned; per-entity-only callers pass `nullptr`. | GroupConsiderationContext | Accepted | Yes |
| SD-005 | `Evaluate()` dispatches the winner via `RuleActionRegistry` inline | Keeps dispatch and selection together — the score table is only meaningful if the winner is acted on immediately. Async path fires the callback with the result; caller dispatches if needed. | UtilitySet | Accepted | Yes |
| SD-006 | Last-frame scores stored inside `UtilitySet`, read by visual debugger | Avoids a separate score cache outside the set. `GetLastFrameScores()` is a read-only accessor; the debugger module has no write access. Scores are only stored if `DIA_DEBUG` is defined. | UtilitySet, Score Overlay | Accepted | Yes |
| SD-007 | Score overlay ships as `DiaUtilityAIVisualDebugger`, a separate static library | Consistent with `DiaRigidBody2DVisualDebugger` and `DiaAnimation2DVisualDebugger`. `DiaUtilityAI` has zero dependency on `DiaVisualDebugger` or `DiaGraphics`. | Score Overlay | Accepted | Yes |
| SD-008 | Async path submits to `AIBudgetScheduler` — no internal threading | Consistent with the platform threading model (PD-002). `DiaUtilityAI` does not spin its own threads. The scheduler controls when the work item runs. | Async Evaluation | Accepted | Yes |
| SD-009 | `UtilitySet` is immutable after `LoadFromJson()` | Consistent with `RuleSet` (DiaRules SD-005) and `ConditionExpr` (DiaCondition SD-004). Pure data-in/result-out; `Evaluate()` is re-entrant. | UtilitySet | Accepted | Yes |
| SD-010 | Caller owns JSON parsing; `LoadFromJson` takes `Json::Value&` | Consistent across DiaStateMachine, DiaRules, DiaCondition. | UtilitySet | Accepted | Yes |
| SD-011 | Test utilities ship inside `DiaUtilityAI/Testing/` | Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All action IDs, scorer slot/field keys use `StringCRC`. `UtilitySetComponent::kUniqueId` is a `StringCRC` constant. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `Evaluate()` and `EvaluateAsync()` are called from within a phase/module by game code. No internal lifecycle. Async work runs on SimPU via `AIBudgetScheduler`. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters and public collections use `DynamicArrayC`. Internal implementation may use STL. |
| PD-005 | Platform | x64 only | Both `DiaUtilityAI.vcxproj` and `DiaUtilityAIVisualDebugger.vcxproj` target x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | Both `.vcxproj` and `.vcxproj.filters` files created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | Neither `.vcxproj` may override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any diagnostic output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.diautilityai.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::UtilityAI::` namespace. |

## Open Design Questions

1. **Cooldown tracking location** — `ActionDef` defines a cooldown duration but something must track the per-entity last-fire timestamp. `UtilitySet::Evaluate()` is currently stateless (SD-009). Options: (a) caller passes a `CooldownState` map by reference, (b) cooldown tracking moves into `UtilitySetComponent`, (c) `UtilitySet` gains mutable cooldown state (breaks SD-009). Revisit before implementing the Cooldown feature.

2. **Async callback thread** — `EvaluateAsync()` says "callback fires on the calling thread when the scheduler drains the work item." Clarify: is the callback fired on the SimPU thread that drains the scheduler, or posted back to the thread that submitted the work? Depends on `AIBudgetScheduler` semantics — confirm when DiaAIBudget is implemented.

3. **Score normalisation across actions** — with multiplicative scorers and varying curve shapes, raw scores across different actions may not be comparable on the same scale. Should `UtilitySet` normalise scores before winner selection (divide by action count, or softmax), or leave raw product scores as-is? Keeping raw scores is simpler to tune but can make curve authoring non-obvious. Decide when the first CluicheTest consumer is wired up.

## Status

**Status:** `Done`

**Plan:** @docs/specs/applications/dia/systems/diautilityai/diautilityai.plan.md
