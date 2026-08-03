**Spec:** @docs/specs/applications/dia/systems/diascalarfield/diascalarfield.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Project scaffold — `DiaScalarField.vcxproj`, `.vcxproj.filters`, `Cluiche.sln` entry under 3.0-Gameplay, `Docs/` folder, module doc `dia.scalarfield.architecture.module.md` | `dia pipeline --target googletest` builds | Done | haiku | Build passes |
| 2 | Core types — `CellIndex`, `CFieldTopology` concept, `SquareFieldTopology`, `HexFieldTopology`, `ScalarFieldLogChannel.h` | static_assert concept tests | Done | sonnet | Build passes; static_asserts in topology headers |
| 3 | `UniformDecayPolicy` + `DiaScalarField<T, P>` template skeleton — double-buffered float arrays, blocked cell bitset, static modifier map, value clamping, `Tick()` | Unit tests: construction, Tick propagates, blocked cells stop propagation | Done | sonnet | BFS cell indexing; UniformDecayPolicy uses int index (informational only) |
| 4 | Write Shape API — `WritePoint`, `WriteRadial` (FalloffCurve), `WriteBox`; queued writes flushed before `Tick()` | Unit tests: shapes written then ticked produce correct values | Done | sonnet | FalloffCurve enum + 3 methods added to DiaScalarField.h |
| 5 | Query API — `GetGradient()`, `FindLocalMaxima()`, `FindCellsAboveThreshold()` | Unit tests: gradient direction correct, spatial queries return expected cells | Done | sonnet | Output type DynamicArrayC<CellIndex, 1024> |
| 6 | `Combine()` static utility + type aliases `SquareScalarField` / `HexScalarField` | Unit tests: weighted combine, negative weights | Done | sonnet | WeightedField nested struct; DynamicArrayC<WeightedField, 32> |
| 7 | Test utilities — `DiaScalarField/Testing/ScalarFieldTestHelpers.h`: `AssertCellValue`, `AssertGradientDirection`, `MockPropagationPolicy`, static_assert | Tests for test utilities themselves | Done | sonnet | Templated AssertCellValue/GradientDirection; MockPropagationPolicy stores last call |
| 8 | Optional adaptors — `Adaptors/RulesPropagationPolicy.h` (header-only, no vcxproj dep on DiaRules); `ScalarFieldOverlay` adaptor (DiaVisualDebugger) | Compile-only: include in isolation without DiaRules / without DiaVisualDebugger | Done | sonnet | None includes in vcxproj; ScalarFieldOverlay Draw() is stub pending ForEachCell |
| 9 | GoogleTests suite — `Cluiche/Tests/GoogleTests/DiaScalarField/` covering all public API, test utilities, golden paths, boundary, invariant, stress | `dia run googletest --filter="ScalarField*"` all pass | Pending | sonnet | |
| 10 | Wire GoogleTests.vcxproj — add `DiaScalarField.lib` to linker deps (Debug + Release), add all test `.cpp` files to project | `dia pipeline --target googletest` builds and passes | Pending | haiku | |
