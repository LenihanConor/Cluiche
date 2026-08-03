**Spec:** @docs/specs/applications/dia/systems/diaflowfield/diaflowfield.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaFlowField library scaffold: vcxproj, vcxproj.filters, Docs/dia.flowfield.architecture.module.md, register in Cluiche.sln | Build compiles | Pending | haiku | |
| 2 | Implement CFlowFieldGraph concept + FlowCell + FlowField (header-only, no .cpp) | Unit tests: Sample(), SampleWorld(), IsComplete(), GetCellCount() | Pending | sonnet | |
| 3 | Implement ComputeFlowField<TGraph> (synchronous Dijkstra sweep) + lifecycle logging | Unit tests: 3×3 square grid, 3×3 hex grid, unreachable cells, goal cell itself | Pending | sonnet | |
| 4 | Implement SquareFlowAdapter + HexFlowAdapter (thin wrappers satisfying CFlowFieldGraph) | Unit tests: adapters satisfy concept, CellToWorldPosition + WorldToCell correctness | Pending | sonnet | |
| 5 | Implement FlowFieldCache (GetOrCompute, Invalidate, InvalidateAll, InvalidateRegion) + logging | Unit tests: cache hit, cache miss, invalidate specific key, invalidate all, invalidate region | Pending | sonnet | |
| 6 | Implement test utilities in DiaFlowField/Testing/: AssertCellDirection, AssertReachable, AssertUnreachable, MockFlowFieldGraph | Used by all GoogleTest suites | Pending | sonnet | |
| 7 | Write GoogleTests: full suite for all public interfaces (TestFlowField.cpp, TestComputeFlowField.cpp, TestFlowFieldCache.cpp, TestFlowFieldAdapters.cpp) | dia run googletest --filter="FlowField*" all pass | Pending | sonnet | |
| 8 | Register DiaFlowField in module registry + update plan/spec to Done | dia docs registry, dia docs spec-done | Pending | haiku | |
