**Spec:** @docs/specs/applications/dia/systems/diastatemachinevisualdebugger/diastatemachinevisualdebugger.md
**Status:** Done

## API Decisions

- No new DiaStateMachine APIs required — `IStateMachineInspectable` was designed for this
- Domain implements `ITransitionListener` to capture live guard results; registered at construction, cleared at destruction
- Guard results cache (`mLastGuardResults`) is Sim-PU-only — `OnTransition` and `GetJSONState` both run on Sim PU, no cross-thread issue
- `OnCommand` drawer booleans are `std::atomic<bool>` (arrives on Render PU)
- Logical drawers `StateList` + `TransitionHistory` are panel sections, not `IVisualDebugger` instances; `HasWorldDrawers()=false`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold `DiaStateMachineVisualDebugger` module: `StateMachineVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Done | haiku | Files created; sln wiring deferred to separate step |
| 2 | Implement `StateMachineVisualDebugger` class: identity methods, `HasWorldDrawers()=false`, `GetDrawerCount()=0`, constructor calling `mInspectable.SetTransitionListener(this)`, destructor calling `SetTransitionListener(nullptr)`, `ITransitionListener::OnTransition()` caching `guardResults` into `mLastGuardResults`, `ITransitionListener::OnTransitionFailed()` setting `mLastTransitionFailed=true` | Compiles; `AC-4` passes; destructor test confirms listener cleared | Done | sonnet | |
| 3 | Implement `GetJSONState()`: emit drawers array + stats (stateCount, currentState, historyLength); emit `states` array from `GetAllStates()` with `active` flag cross-referenced against `GetCurrentStateId()`; emit `history` array from `GetTransitionHistory()`; emit `lastGuardResults` from cache; emit `lastTransitionFailed`; gate each section on `mStateListEnabled` / `mTransitionHistEnabled` | JSON matches schema in spec; disabled drawer removes its section key | Done | sonnet | |
| 4 | Implement `OnCommand("toggle", {drawer:name})` with `std::atomic<bool>` for `mStateListEnabled` and `mTransitionHistEnabled`; `OnCommand("setScale",...)` no-op | AC-11,12 pass | Done | haiku | |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaStateMachineVisualDebugger/TestStateMachineVisualDebugger.cpp`: DrawerGate (disable StateList → removes states key), PrimitiveType (HasWorldDrawers=false, GetDrawerCount=0), ScaleSensitivity (setScale no-op, no crash), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Done | sonnet | Uses real FlatStateMachine |
| 6 | Write domain-specific test shapes: States_CountMatchesGetAllStates, States_CurrentState_MarkedActive, History_MatchesGetTransitionHistory, GuardResults_CapturedOnTransition, GuardResults_Empty_BeforeAnyTransition, TransitionFailed_FlagSet, TransitionFailed_FlagCleared_AfterSuccess, Toggle_TransitionHistory, DrawerGate_TransitionHistory, NoStates_EmptyArrays_NoAssert, Destructor_ClearsListener, SetScale_NoOp | All 12 shapes pass | Done | sonnet | |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Done | haiku | All 20 tests pass; 20/20 green |
