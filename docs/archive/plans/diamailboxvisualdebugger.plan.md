**Spec:** @docs/specs/applications/dia/systems/diamailboxvisualdebugger/diamailboxvisualdebugger.md
**Status:** Done

## API Decisions

- `Mailbox::GetTypeStatsByIndex(int)` added as non-debug accessor per SD-001 — reads parallel `TypeStats` array already maintained by `Send`/`Drain`/overflow logic; returns zeroed stats for out-of-range index
- `GetRegisteredTypeCount()` already exists — no change needed
- Two drawers: `QueueTable` (all types) + `DropAlerts` (types with totalDropped > 0) per SD-002
- `typeKey` rendered as `"0x%08X"` hex per SD-003; type name registration deferred to v2
- `capacity == 0` guard: emit `fillPct = 0` without divide-by-zero
- `std::atomic<bool>` for drawer enables

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `Mailbox::GetTypeStatsByIndex(int typeIndex) const` to `DiaMailbox`; reads the internal parallel `TypeStats` array at position `typeIndex`; returns default-constructed `TypeStats{}` when out of range | Unit test: register 2 types, `GetTypeStatsByIndex(0).capacity > 0`, `GetTypeStatsByIndex(2)` returns zeroed struct; `GetRegisteredTypeCount()==2` | Done | sonnet | Added to Mailbox.h + Mailbox.cpp; reads from TypedQueueDescriptor directly |
| 2 | Scaffold `DiaMailboxVisualDebugger` module: `MailboxVisualDebugger.h/.cpp`; vcxproj + filters; module YAML; sln under `3.1-Gameplay-Tools` | `dia check debugger-contract` PENDING, not ERROR; build succeeds | Done | haiku | GUID {A3B4C5D6-E7F8-9012-A3B4-C5D6E7F89012} |
| 3 | Implement `MailboxVisualDebugger` identity, `HasWorldDrawers()=false`, `GetJSONState()` iterating `0..GetRegisteredTypeCount()` via `GetTypeStatsByIndex(i)`; build `types` array (typeKey as hex, capacity, currentCount, fillPct, totalSent, totalDropped, totalDrained, hasDrops); emit drawers + stats (typeCount, totalDropped sum); `fillPct = clamp(round(currentCount/capacity×100), 0, 100)`; `capacity==0` → `fillPct=0`; gate sections on drawer enables | JSON matches schema; 0 types → empty array | Done | sonnet | dropAlerts emitted as separate key when DropAlerts enabled |
| 4 | Implement `OnCommand("toggle",...)` with `std::atomic<bool>` for `mQueueTableEnabled` and `mDropAlertsEnabled`; gate JSON sections; `OnCommand("setScale",...)` no-op | AC-11,12 pass | Done | haiku | |
| 5 | Write mandatory test shapes in `Tests/GoogleTests/DiaMailboxVisualDebugger/TestMailboxVisualDebugger.cpp`: DrawerGate (disable QueueTable → removes `types` key), PrimitiveType (HasWorldDrawers=false), ScaleSensitivity (no-op), JSONRoundTrip, OnCommandRoundTrip | All 5 shapes pass | Done | sonnet | |
| 6 | Write domain-specific test shapes: Types_CountMatchesGetRegisteredTypeCount, Types_ArrayLengthMatchesTypeCount, Types_CapacityFromTypeStats, Types_CurrentCountFromTypeStats, Types_FillPct_Computed, Types_FillPct_ClampedAt100, Types_TotalDropped_FromTypeStats, Types_HasDrops_TrueWhenDropped, Stats_TotalDropped_SumOfAll, DrawerGate_DropAlerts, Toggle_DropAlerts, NoTypes_EmptyArray_NoAssert, CapacityZero_NoAssert | All 13 shapes pass | Done | sonnet | |
| 7 | `dia check debugger-contract` full pass | Exit code 0 | Done | haiku | All 7 checks PASS; 2 pre-existing FAILs on unrelated modules |
