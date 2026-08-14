**Spec:** @docs/specs/applications/dia/systems/diablackboardvisualdebugger/diablackboardvisualdebugger.md
**Status:** Todo

## API Decisions

- No `DiaBlackboard` API changes required — `Blackboard::VisitSlots(fn)` is the data source
- Hex fallback is v1 default per SD-001 — shows correct data even when no formatter registered
- `RegisterFormatter` uses C-style function pointer per SD-002 — avoids STL allocation; max 16 formatters
- Formatters not invoked when drawer disabled per SD-003 — skip `VisitSlots` entirely when `mSlotTableEnabled == false`
- `std::atomic<bool>` for `mSlotTableEnabled` (OnCommand arrives on Render PU)

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold `DiaBlackboardVisualDebugger` module: `BlackboardVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Todo | haiku | |
| 2 | Implement `BlackboardVisualDebugger` identity, `HasWorldDrawers()=false`, `GetJSONState()` calling `VisitSlots()` to build slots array; each slot entry has key (CRC as hex string), type (typeTag pointer as hex, or formatter-provided name), value (hex fallback `0x????????` on first 4 bytes, or formatter output); emit drawers + `stats.slotCount`; skip `VisitSlots` call when `mSlotTableEnabled==false` | JSON schema matches spec; empty blackboard → empty slots array | Todo | sonnet | |
| 3 | Implement `RegisterFormatter(typeTag, fn)`: append to `mFormatters`; in `GetJSONState()` look up typeTag before emitting value — call fn if found, else hex fallback | Formatter output appears in JSON for registered type; hex for unregistered | Todo | sonnet | |
| 4 | Implement `OnCommand("toggle", {drawer:"SlotTable"})` with `std::atomic<bool>`; `OnCommand("setScale",...)` no-op | AC-11,12 pass; disabled SlotTable removes `slots` key | Todo | haiku | |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaBlackboardVisualDebugger/TestBlackboardVisualDebugger.cpp`: DrawerGate (disable SlotTable → no `slots` key), PrimitiveType (HasWorldDrawers=false), ScaleSensitivity (no-op), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Todo | sonnet | Register 2 slots on a synthetic Blackboard |
| 6 | Write domain-specific test shapes: Slots_CountMatchesRegistered, Slots_KeyPresentForEachSlot, Slots_HexFallback_NoFormatter, Slots_FormattedValue_WithFormatter, Slots_FormatterOutput_TruncatedToBuf, EmptyBlackboard_EmptySlots_NoAssert, MultipleFormatters_CorrectDispatch, FormatterNotCalled_WhenDrawerDisabled | All 8 shapes pass | Todo | sonnet | |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Todo | haiku | |
