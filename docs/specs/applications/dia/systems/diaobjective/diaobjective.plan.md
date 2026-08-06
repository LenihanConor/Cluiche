**Spec:** @docs/specs/applications/dia/systems/diaobjective/diaobjective.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold — DiaObjective.vcxproj, .filters, module.md, sln registration | Build succeeds | Done | haiku | |
| 2 | Core types — IObjectiveObserver.h, ObjectiveDef.h (enums, RewardPayload, ObjectiveDef) | Manual review | Done | haiku | |
| 3 | ObjectiveSet — ObjectiveSet.h + ObjectiveSet.cpp (LoadFromJson, Validate, Evaluate, observers, queries) | Build + tests | Pending | sonnet | |
| 4 | ObjectiveSetComponent — ObjectiveSetComponent.h + .cpp + vcxproj entries | Build | Pending | sonnet | |
| 5 | Test utilities — DiaObjective/Testing/ObjectiveTestHelpers.h (CapturingObserver) + vcxproj | Build | Pending | haiku | |
| 6 | GoogleTests — DiaObjective test files + GoogleTests.vcxproj wiring | dia run googletest | Pending | sonnet | |
