**Spec:** @docs/specs/applications/dia/systems/diaaibudgetvisualdebugger/diaaibudgetvisualdebugger.md
**Status:** Done

## API Decisions

- `AIBudgetResult.perSystem` DynamicArrayC added under `#ifdef DIA_DEBUG` per SD-001 — Release struct size unchanged
- `AIBudgetScheduler::GetLastBudgetMs()` added as non-debug accessor per SD-002 — useful for tests regardless of build
- Debugger takes `const AIBudgetScheduler&` + `const AIBudgetResult&`; caller updates result each frame after `Update()` per SD-003
- Fill bar colour tiers (green/amber/red) live in panel JS, not C++ per SD-004
- `budgetMs == 0` guard: emit `fillPct = 0` without divide-by-zero

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `SystemTimingEntry` struct + `perSystem` array to `AIBudgetResult` under `#ifdef DIA_DEBUG`; add `GetLastBudgetMs()` to `AIBudgetScheduler`; update `AIBudgetScheduler::Update()` to time each system's `Tick()` and populate `perSystem` in DIA_DEBUG builds | Unit test: run scheduler with 2 systems, `perSystem[0].ran == true`, `perSystem[0].timeMs > 0`, `GetLastBudgetMs()` returns the value passed to `Update()` | Done | sonnet | `IAIBudgetedSystem::GetId()` required — confirm it exists; add if absent; SystemTimingEntry+perSystem (DIA_DEBUG), GetLastBudgetMs() (non-debug), Update() populates perSystem. 60 tests pass. |
| 2 | Scaffold `DiaAIBudgetVisualDebugger` module: `AIBudgetVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Done | haiku | Scaffold created: .h, .cpp stubs, vcxproj, vcxproj.filters, module YAML. Registered in sln under 3.1-Gameplay-Tools. Build: 0 errors. |
| 3 | Implement `AIBudgetVisualDebugger` identity, `HasWorldDrawers()=false`, `GetJSONState()` emitting drawers + stats (usedMs, budgetMs, fillPct, systemsRun, systemsDeferred, registeredCount) + systems array from `perSystem`; `fillPct = clamp(round(usedMs/budgetMs×100), 0, 100)`; guard `budgetMs==0` → `fillPct=0` | JSON matches schema; `budgetMs==0` → no crash | Done | sonnet | Full GetJSONState: drawers, stats (usedMs/budgetMs/fillPct/systemsRun/systemsDeferred/registeredCount), systems array. fillPct clamped+guarded. Build: 0 errors. |
| 4 | Implement `OnCommand("toggle",...)` with `std::atomic<bool>` for `mBudgetBarEnabled` and `mSystemTimingsEnabled`; gate JSON sections accordingly; `OnCommand("setScale",...)` no-op | AC-11,12 pass; disabled BudgetBar removes `stats.fillPct` from JSON | Done | haiku | OnCommand toggle for BudgetBar+SystemTimings with atomic<bool>; setScale no-op. Build: 0 errors. |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaAIBudgetVisualDebugger/TestAIBudgetVisualDebugger.cpp`: DrawerGate, PrimitiveType (HasWorldDrawers=false), ScaleSensitivity (setScale no-op), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Done | sonnet | 5 mandatory shapes pass: DrawerGate, PrimitiveType, ScaleSensitivity, JSONRoundTrip, OnCommandRoundTrip. |
| 6 | Write domain-specific test shapes: Stats_UsedMs_MatchesResult, Stats_BudgetMs_MatchesScheduler, Stats_FillPct_Computed, Stats_FillPct_ClampedAt100, Stats_SystemsRun_Matches, Stats_SystemsDeferred_Matches, Systems_CountMatchesPerSystem, Systems_RanFlag_Accurate, Systems_TimeMs_Accurate, DrawerGate_SystemTimings, Toggle_SystemTimings, NoSystems_EmptyArray_NoAssert, BudgetZero_NoAssert | All 13 shapes pass | Done | sonnet | All 13 domain shapes pass (18 total). Stats, systems, drawer gates, BudgetZero guard. |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Done | haiku | dia check debugger-contract: DiaAIBudgetVisualDebugger ALL PASS (AC2/AC4/AC8/AC10/AC11-12/AC13). 2 pre-existing failures in unrelated modules. |
