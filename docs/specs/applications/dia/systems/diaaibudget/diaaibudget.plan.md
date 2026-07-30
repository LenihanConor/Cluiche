**Spec:** @docs/specs/applications/dia/systems/diaaibudget/diaaibudget.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaAIBudget module directory + IAIBudgetedSystem.h + AIBudgetScheduler.h/.cpp | Unit: Register/Unregister/GetRegisteredCount; Update calls systems in order; stops when budget exhausted; UpdateBudgeted(0.0f) no-op | Pending | sonnet | |
| 2 | Create AIBudgetModule.h/.cpp (Module subclass; OnConfigure reads budgetUs; DoStart registers metrics; DoUpdate calls scheduler; DoStop nulls metrics) | Unit: OnConfigure defaults; metrics registered; Update calls scheduler with converted ms | Pending | sonnet | |
| 3 | Create DiaAIBudget.vcxproj + DiaAIBudget.vcxproj.filters; register in Cluiche.sln under 3.0-Gameplay | Build: dia pipeline --target googletest compiles clean | Pending | haiku | |
| 4 | Write GoogleTests: TestAIBudgetScheduler.cpp + TestAIBudgetModule.cpp; add to GoogleTests.vcxproj | dia run googletest --filter="DiaAIBudget*" all pass | Pending | sonnet | |
| 5 | Create dia.aibudget.architecture.module.md YAML + update module registry | dia docs registry runs clean | Pending | haiku | |
