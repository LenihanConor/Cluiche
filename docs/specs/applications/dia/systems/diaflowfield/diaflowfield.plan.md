**Spec:** @docs/specs/applications/dia/systems/diaflowfield/diaflowfield.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaFlowField library scaffold: vcxproj, vcxproj.filters, Docs/dia.flowfield.architecture.module.md, register in Cluiche.sln | Build compiles | Done | haiku | Build passes |
| 2 | Implement CFlowFieldGraph concept + FlowCell + FlowField (header-only, no .cpp) | Unit tests: Sample(), SampleWorld(), IsComplete(), GetCellCount() | Done | sonnet | All tests pass |
| 3 | Implement ComputeFlowField<TGraph> (synchronous Dijkstra sweep) + lifecycle logging | Unit tests: 3×3 square grid, 3×3 hex grid, unreachable cells, goal cell itself | Done | sonnet | Fixed hex bounds guard |
| 4 | Implement SquareFlowAdapter + HexFlowAdapter (thin wrappers satisfying CFlowFieldGraph) | Unit tests: adapters satisfy concept, CellToWorldPosition + WorldToCell correctness | Done | sonnet | static_assert in headers |
| 5 | Implement FlowFieldCache (GetOrCompute, Invalidate, InvalidateAll, InvalidateRegion) + logging | Unit tests: cache hit, cache miss, invalidate specific key, invalidate all, invalidate region | Done | sonnet | Uses StringCRC hash specialization |
| 6 | Implement test utilities in DiaFlowField/Testing/: AssertCellDirection, AssertReachable, AssertUnreachable, MockFlowFieldGraph | Used by all GoogleTest suites | Done | sonnet | 8×8 mock, 4-connected |
| 7 | Write GoogleTests: full suite for all public interfaces (TestFlowField.cpp, TestComputeFlowField.cpp, TestFlowFieldCache.cpp, TestFlowFieldAdapters.cpp) | dia run googletest --filter="FlowField*" all pass | Done | sonnet | 26 tests passing |
| 8 | Register DiaFlowField in module registry + update plan/spec to Done | dia docs registry, dia docs spec-done | Done | haiku | Registry regenerated |
