**Spec:** @docs/specs/applications/dia/systems/diacondition/diacondition.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Project scaffold: DiaCondition.vcxproj, .vcxproj.filters, Docs/module.md, register in Cluiche.sln | Build compiles clean | Done | sonnet | GUID collision fixed; GUID={D4E5F6A7-B8C9-4012-DEFA-123456789012} |
| 2 | IConditionContext + ConditionRegistry: headers + cpp, add to vcxproj | Unit: Register/GetFloat/GetBool | Done | sonnet | pimpl for STL isolation; destructor added post-review |
| 3 | ConditionExpr: JSON loading, AND/OR/NOT tree evaluation, Validate, IsValid | Unit: leaf ops, composites, malformed JSON | Done | sonnet | pimpl + move-only; removed dead IsInteriorOp post-review |
| 4 | ConditionGuardAdapter: RegisterAsGuard free function, add DiaStateMachine reference | Unit: guard fires correctly via CallbackRegistry | Done | haiku | template<int N> thunk table; 16 slots; DIA_ASSERT on overflow |
| 5 | Test Utilities: Testing/ConditionTestHelpers.h (MockConditionContext, AssertExprResult) | Consumed by task 6 tests | Done | haiku | header-only with inline impls |
| 6 | GoogleTests: TestConditionRegistry, TestConditionExpr, TestConditionGuardAdapter — TDD, add to GoogleTests.vcxproj | dia run googletest --filter="DiaCondition*" | Done | sonnet | 64 tests initial + 33 exhaustive gap-fill = 97 total; 7 suites |
