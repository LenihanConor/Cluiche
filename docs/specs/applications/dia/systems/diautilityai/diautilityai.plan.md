**Spec:** @docs/specs/applications/dia/systems/diautilityai/diautilityai.md
**Status:** In Progress

## Design Resolutions

**ODQ #1 — Cooldown tracking location:** Option (b). `UtilitySetComponent` owns a `std::unordered_map<StringCRC, float>` tracking per-action last-fire timestamps. `UtilitySet::Evaluate()` stays stateless (SD-009). Callers that drive `Evaluate()` directly are responsible for their own cooldown logic.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Project scaffold — `DiaUtilityAI/` + `DiaUtilityAIVisualDebugger/` directories; `DiaUtilityAI.vcxproj` + `.filters`; `DiaUtilityAIVisualDebugger.vcxproj` + `.filters`; register both in `Cluiche.sln` under `3.0-Gameplay`; `dia.diautilityai.architecture.module.md` | `dia check deps` clean | Done | haiku | `dia check deps` clean (8 pre-existing unrelated issues) |
| 2 | `ResponseCurve` — `ResponseCurve.h/.cpp`; 6 shapes (`linear`, `quadratic`, `exponential`, `logistic`, `sine`, `step`); `Evaluate(float normalised)` → score [0,1]; `invert` flag; `LoadFromJson`; `GetShape()` | GoogleTest: each shape at key inputs; invert; boundary 0.0/1.0 | Done | sonnet | 27 tests pass |
| 3 | `ActionDef`, `ScorerDef`, `GroupConsiderationContext` — `ActionDef.h` (move-only; `ConditionExpr` member); `ScorerDef.h`; `GroupConsiderationContext.h/.cpp` (`Increment`/`Decrement`/`GetCount`/`Reset`) | GoogleTest: `GroupConsiderationContext` count tracking, cap boundary | Done | sonnet | 12 tests pass |
| 4 | `UtilitySet` sync core — `UtilitySet.h/.cpp`; winner-takes-all `Evaluate()`; product of scorer outputs; prerequisite guard; `max_concurrent` via `GroupConsiderationContext`; `LoadFromJson`; `Validate`; `GetActionCount`; `GetLastFrameScores` (debug-only) | GoogleTest: two-action set (one gated), correct winner, product of scorers, kInvalidCRC on no eligible | Done | sonnet | 11 tests pass |
| 5 | `UtilitySet` async — `EvaluateAsync()` submits evaluation to `AIBudgetScheduler` work item; callback fires on drain | GoogleTest: mock scheduler drains; callback receives correct `UtilitySelection` | Pending | sonnet | |
| 6 | `UtilitySetComponent` — `IComponent` via `DIA_COMPONENT` macro; `SetUtilitySet`/`SetRegistry`/`SetGroupContext`/`Evaluate`/`GetUtilitySet`; cooldown state (`std::unordered_map<StringCRC, float>` last-fire timestamps); add to `DiaUtilityAI.vcxproj` | GoogleTest: component attaches, evaluates, cooldown blocks repeat dispatch within window | Pending | sonnet | Uses `DIA_COMPONENT_REGISTER`; lives in `DiaEntity` dependency |
| 7 | Test utilities — `DiaUtilityAI/Testing/UtilityTestHelpers.h`; `AssertWinner`, `AssertNoSelection`, `AssertCurveOutput`; add to `DiaUtilityAI.vcxproj` | Used in tasks 4/5/6 tests | Pending | haiku | |
| 8 | `PersonalityProfile` + integration — `PersonalityProfile.h/.cpp`; `LoadFromJson`, `GetScoreMultiplier`, `GetEvalPeriodTicks`, `IsValid`; update `UtilitySet::Evaluate()` + `EvaluateAsync()` with optional `const PersonalityProfile*`; post-bias scores in `GetLastFrameScores`; `UtilitySetComponent::SetPersonality`/`GetPersonality` + tick counter eval-period skip; `AssertPersonalityChangesWinner` test utility | GoogleTest: same set + same ctx, no profile vs Aggressive → different winner; period=3 skips; period=1 always runs | Pending | sonnet | Feature spec: `ai-personality.md` |
| 9 | `DiaUtilityAIVisualDebugger` — `UtilityScoreDrawer.h/.cpp`; implements `IVisualDebugger`; reads `GetLastFrameScores()`; draws score bars; `DrawImGui()` score table; `#ifdef DIA_DEBUG` guard; register debugger vcxproj in sln | No automated test (visual-only) | Pending | sonnet | Separate static library |
