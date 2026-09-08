**Spec:** @docs/specs/applications/dia/systems/diaaibudget/diaaibudget.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaAIBudget module directory + IAIBudgetedSystem.h + AIBudgetScheduler.h/.cpp | Unit: Register/Unregister/GetRegisteredCount; Update calls systems in order; stops when budget exhausted; UpdateBudgeted(0.0f) no-op | Done | sonnet | AIBudgetResult struct added to return telemetry from Update() |
| 2 | Create AIBudgetModule.h/.cpp (Module subclass; OnConfigure reads budgetUs; DoStart registers metrics; DoUpdate calls scheduler; DoStop nulls metrics) | Unit: OnConfigure defaults; metrics registered; Update calls scheduler with converted ms | Done | sonnet | StartResult/StopResult fully qualified; jsoncpp include path added to vcxproj |
| 3 | Create DiaAIBudget.vcxproj + DiaAIBudget.vcxproj.filters; register in Cluiche.sln under 3.0-Gameplay | Build: dia pipeline --target googletest compiles clean | Done | haiku | GUID {C9D0E1F2-A3B4-5678-90AB-CDEF01234567} |
| 4 | Write GoogleTests: TestAIBudgetScheduler.cpp; add to GoogleTests.vcxproj | dia run googletest --filter="DiaAIBudget*" all pass | Done | sonnet | 16/16 tests passing |
| 5 | Create dia.aibudget.architecture.module.md YAML + update module registry | dia docs registry runs clean | Done | haiku | layer: domain/gameplay/ai |
