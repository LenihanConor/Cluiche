**Spec:** @docs/specs/applications/dia/systems/diarulesvisualdebugger/diarulesvisualdebugger.md
**Status:** Todo

## API Decisions

- `RuleSet::GetLastFireReport()` is already specced as a prereq in the spec — must be added to `DiaRules` before building the debugger
- `RuleSet::GetAllRuleIds()` also required (shows unfired rules in panel per SD-003)
- `RuleFireEntry` struct defined in `DiaRules/RuleSet.h` under `#ifdef DIA_DEBUG`
- Full rule inventory shown (fired + unfired) per SD-003; fired-only would hide "does the rule exist?" question
- Current-frame snapshot only — no C++ ring buffer per SD-004
- `std::atomic<bool>` for `mFireLogEnabled`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `RuleFireEntry` struct + `GetLastFireReport()` + `GetAllRuleIds()` to `DiaRules/RuleSet.h` under `#ifdef DIA_DEBUG`; populate `mLastFireReport` during `Evaluate()`; clear on entry to each `Evaluate()` call | Unit test: evaluate RuleSet with 2 rules (1 fires), `GetLastFireReport()` returns 1 entry with correct ruleId + action IDs; `GetAllRuleIds()` returns 2 | Todo | sonnet | `mLastFireReport` stored as `DynamicArrayC<RuleFireEntry, 16>` member; cleared and re-populated each `Evaluate()` |
| 2 | Scaffold `DiaRulesVisualDebugger` module: `RulesVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Todo | haiku | |
| 3 | Implement `RulesVisualDebugger` identity, `HasWorldDrawers()=false`, `GetJSONState()`: emit drawers (FireLog); emit stats (ruleCount, fired); emit rules array — all rule IDs from `GetAllRuleIds()` cross-referenced against `GetLastFireReport()`; unnamed rules emitted as `"(unnamed)"`; gate rules section on `mFireLogEnabled` | JSON matches schema in spec; unfired rules appear with `fired:false` | Todo | sonnet | |
| 4 | Implement `OnCommand("toggle",{drawer:"FireLog"})` with `std::atomic<bool>`; `OnCommand("setScale",...)` no-op | AC-11,12 pass | Todo | haiku | |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaRulesVisualDebugger/TestRulesVisualDebugger.cpp`: DrawerGate (disable FireLog → removes rules key), PrimitiveType (HasWorldDrawers=false), ScaleSensitivity (no-op), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Todo | sonnet | Synthetic `RuleSetComponent` with controlled `GetLastFireReport` / `GetAllRuleIds` |
| 6 | Write domain-specific test shapes: FiredRule_InRulesArray_WithFiredTrue, UnfiredRule_InRulesArray_WithFiredFalse, Stats_RuleCount_Matches_GetAllRuleIds, Stats_Fired_Matches_FireCount, ActionsArray_MatchesFireEntry, UnnamedRule_EmittedAs_Unnamed, NoRulesEvaluated_AllFiredFalse_NoAssert, FireLog_Disabled_Removes_Rules_Key | All 8 shapes pass | Todo | sonnet | |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Todo | haiku | |
