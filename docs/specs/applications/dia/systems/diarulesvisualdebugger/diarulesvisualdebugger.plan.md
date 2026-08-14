**Spec:** @docs/specs/applications/dia/systems/diarulesvisualdebugger/diarulesvisualdebugger.md
**Status:** Done

## API Decisions

- `RuleSet::GetLastFireReport()` is already specced as a prereq in the spec — must be added to `DiaRules` before building the debugger
- `RuleSet::GetAllRuleIds()` also required (shows unfired rules in panel per SD-003)
- `RuleFireEntry` struct defined in `DiaRules/RuleSet.h` under `#ifdef DIA_DEBUG`
- Full rule inventory shown (fired + unfired) per SD-003; fired-only would hide "does the rule exist?" question
- Current-frame snapshot only — no C++ ring buffer per SD-004
- `std::atomic<bool>` for `mFireLogEnabled`

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `RuleFireEntry` struct + `GetLastFireReport()` + `GetAllRuleIds()` to `DiaRules/RuleSet.h` under `#ifdef DIA_DEBUG`; populate `mLastFireReport` during `Evaluate()`; clear on entry to each `Evaluate()` call | Unit test: evaluate RuleSet with 2 rules (1 fires), `GetLastFireReport()` returns 1 entry with correct ruleId + action IDs; `GetAllRuleIds()` returns 2 | Done | sonnet | `RuleFireEntry` + `GetLastFireReport()` already in DiaRules; used `GetRuleCount()`/`GetRuleAt(i)` instead of `GetAllRuleIds()` — existing API sufficient |
| 2 | Scaffold `DiaRulesVisualDebugger` module: `RulesVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Done | haiku | GUID {A4B6C8D0-E2F4-A6B8-C0D2-E4F6A8B0C2D4}; wired into sln + GoogleTests.vcxproj |
| 3 | Implement `RulesVisualDebugger` identity, `HasWorldDrawers()=false`, `GetJSONState()`: emit drawers (FireLog); emit stats (ruleCount, fired); emit rules array — all rule IDs from `GetAllRuleIds()` cross-referenced against `GetLastFireReport()`; unnamed rules emitted as `"(unnamed)"`; gate rules section on `mFireLogEnabled` | JSON matches schema in spec; unfired rules appear with `fired:false` | Done | sonnet | Claimed-set pattern handles unnamed (kZero) rules correctly; actions emitted from fire report when fired, from RuleDef otherwise |
| 4 | Implement `OnCommand("toggle",{drawer:"FireLog"})` with `std::atomic<bool>`; `OnCommand("setScale",...)` no-op | AC-11,12 pass | Done | haiku | `std::atomic<bool> mFireLogEnabled{true}`; setScale is no-op |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaRulesVisualDebugger/TestRulesVisualDebugger.cpp`: DrawerGate (disable FireLog → removes rules key), PrimitiveType (HasWorldDrawers=false), ScaleSensitivity (no-op), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Done | sonnet | 20/20 tests pass |
| 6 | Write domain-specific test shapes: FiredRule_InRulesArray_WithFiredTrue, UnfiredRule_InRulesArray_WithFiredFalse, Stats_RuleCount_Matches_GetAllRuleIds, Stats_Fired_Matches_FireCount, ActionsArray_MatchesFireEntry, UnnamedRule_EmittedAs_Unnamed, NoRulesEvaluated_AllFiredFalse_NoAssert, FireLog_Disabled_Removes_Rules_Key | All 8 shapes pass | Done | sonnet | 3-rule fixture: attack fires, flee doesn't, unnamed fires (health=80) |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Done | haiku | 20/20 green |
