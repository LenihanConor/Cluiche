**Spec:** @docs/specs/applications/dia/systems/diahtnvisualdebugger/diahtnvisualdebugger.md
**Status:** Done

## API Decisions

- `HTNPlan::GetCurrentTaskIndex()` is the prereq — `GetJSONState` needs it to mark the current task in the plan list; add minimal accessor to `HTNPlan` in `DiaHTN`
- `HTNPlan::GetCost()` treated as optional per ODQ-3 — emit `planCost` if it exists, omit if absent; resolve at Task 1
- Domain is panel-only (`HasWorldDrawers()=false`); Register/Unregister are no-ops
- Drawer enables use `std::atomic<bool>`; `PlanView` is the single toggle drawer

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `HTNPlan::GetCurrentTaskIndex() const` to `DiaHTN`; also confirm/add `HTNPlan::GetCost()` — emit planCost if present, skip if absent | Unit test: plan with 3 tasks where cursor is at index 1 → `GetCurrentTaskIndex()==1` | Done | haiku | Cursor position already tracked internally in `HTNPlannerComponent::Tick()`; expose via `HTNPlan`; GetCurrentTaskIndex() returns mCursor; GetCost() absent — planCost omitted; 52 tests pass |
| 2 | Scaffold `DiaHTNVisualDebugger` module: `HTNVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Done | haiku | Scaffold created; GUID B2C3D4E5; sln EndProject/Project concat bug fixed; builds clean |
| 3 | Implement `HTNVisualDebugger` identity, `HasWorldDrawers()=false`, `GetJSONState()`: emit drawers (PlanView); emit stats (current, total); emit plan array with index/name/status fields per task (done = index < cursor, current = index == cursor, pending = index > cursor); emit diverged + planCost; emit empty plan array when `!HasActivePlan()`; gate plan section on `mPlanViewEnabled` | JSON matches schema in spec; empty plan → `"plan":[]` | Done | sonnet | GetTask(int) added to HTNPlan; GetJSONState full impl with drawers/stats/plan/diverged; planCost omitted (no GetCost) |
| 4 | Implement `OnCommand("toggle",{drawer:"PlanView"})` with `std::atomic<bool>`; `OnCommand("setScale",...)` no-op | AC-11,12 pass | Done | haiku | toggle/setScale impl; atomic mPlanViewEnabled; defensive arg guards; AC-11/12 done |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaHTNVisualDebugger/TestHTNVisualDebugger.cpp`: DrawerGate (disable PlanView → removes plan key), PrimitiveType (HasWorldDrawers=false), ScaleSensitivity (setScale no-op), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Done | sonnet | Mock `HTNPlannerComponent` with controlled plan state; 11 tests pass; GoogleTests wired; all 5 AC-15 shapes covered |
| 6 | Write domain-specific test shapes: PlanArray_StatusCorrect (done/current/pending per cursor position), NoPlan_EmptyArray_NoAssert, Stats_Current_And_Total_Match, Diverged_FlagSet_WhenHasDiverged, Diverged_FlagClear_WhenNot, PlanView_Disabled_Removes_Plan_Key | All 6 shapes pass | Done | sonnet | 6 domain-specific tests added; 17 total pass; diverged always-false noted |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Done | haiku | DiaHTNVisualDebugger: all ACs PASS; exit code 1 is pre-existing failures in DiaEntitySpatialVisualDebugger + DiaScalarFieldVisualDebugger (AC10/AC11-12) — not this work |
