**Spec:** @docs/specs/applications/dia/systems/diarules/diarules.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold project: create `Dia/DiaRules/` directory, `DiaRules.vcxproj`, `DiaRules.vcxproj.filters`, `Docs/dia.diarules.architecture.module.md`; register in `Cluiche.sln` with GUID `{C7D8E9F0-A1B2-3456-CDEF-012345678901}` nested under 3.0-Gameplay | `dia run googletest` compiles clean (no DiaRules files yet) | Done | haiku | 6954 existing tests pass |
| 2 | Implement `RuleActionRegistry` (`RuleActionRegistry.h` + `RuleActionRegistry.cpp`); add GoogleTests `DiaRules/TestRuleActionRegistry.cpp` and wire into GoogleTests.vcxproj | `dia run googletest --filter="DiaRules_ActionRegistry*"` all pass | Done | sonnet | 17/17 pass; pimpl keeps STL out of public header |
| 3 | Implement `RuleDef` + `RuleSet` (`RuleDef.h`, `RuleSet.h`, `RuleSet.cpp`): JSON loading, `Validate()`, `Evaluate()`; add `DiaRules/TestRuleSet.cpp` to GoogleTests | `dia run googletest --filter="DiaRules_RuleSet*"` all pass | Done | sonnet | 12/12 pass; RuleDef move-only; pimpl; unnamed rules exempt from dup-ID check |
| 4 | Implement `RuleSetComponent` (`RuleSetComponent.h` + `RuleSetComponent.cpp`) using `DIA_COMPONENT` macro; add `DiaRules/TestRuleSetComponent.cpp` to GoogleTests | `dia run googletest --filter="DiaRules_Component*"` all pass | Done | sonnet | 9/9 pass; DIA_READONLY, registry unowned |
| 5 | Implement test utilities (`DiaRules/Testing/RulesTestHelpers.h` + `RulesTestHelpers.cpp`): `FireRuleSet`, `AssertActionsFired`, `AssertActionNotFired`; add `DiaRules/TestRulesTestHelpers.cpp` to GoogleTests | `dia run googletest --filter="DiaRules_TestHelpers*"` all pass | Done | sonnet | 7/7 pass; header-only (SD-009); 45 total DiaRules tests pass |
