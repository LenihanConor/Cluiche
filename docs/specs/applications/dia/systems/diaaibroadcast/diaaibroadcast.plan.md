**Spec:** @docs/specs/applications/dia/systems/diaaibroadcast/diaaibroadcast.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Project scaffold — `Dia/DiaAICallout/` directory, `DiaAICallout.vcxproj`, `DiaAICallout.vcxproj.filters`, YAML module doc, register in `Cluiche.sln` | Build succeeds (static lib) | Pending | haiku | No DiaEntitySpatial dep; uses DiaGeometry2D + DiaCore only. ODQ-2 resolved: linear scan. |
| 2 | `Callout` + `CalloutHandle` types — `Callout.h` value struct (kind, position, radius, faction, ttl, payload), `CalloutHandle.h/cpp` (index+gen safe ref, IsValid, IsClaimed, Get) | Unit: handle invalidation on expiry, IsValid false after generation mismatch | Pending | sonnet | |
| 3 | `CalloutRegistry` internal store + `Emit()` + `GetLiveCount()` — slot pool (index+generation), Emit returns handle, GetLiveCount correct | Unit: emit returns valid handle, live count increments | Pending | sonnet | |
| 4 | `Query()` — linear scan over live callouts, filter by kind + distance ≤ radius + faction (kInvalidCRC = any), unclaimed-only in results, out-param `DynamicArrayC<CalloutHandle>` | Unit: query returns matching unclaimed, excludes claimed, excludes wrong kind/faction/distance | Pending | sonnet | SD-004 (unclaimed only), SD-005 (unordered) |
| 5 | `Claim()` / `Release()` — Claim atomically marks one claimer (fails if already claimed), Release returns to unclaimed pool (no-op on expired handle) | Unit: double-claim returns false, release makes it queryable again, wrong claimer can't release | Pending | sonnet | SD-001 (exclusive claim), SD-003 (explicit release) |
| 6 | `Update(float dt)` / TTL expiry — tick TTLs on all live callouts, remove entries at TTL ≤ 0, handles to expired callouts become invalid | Unit: TTL countdown, expiry removes from live count, handle IsValid false after expiry | Pending | sonnet | |
| 7 | Test utilities — `DiaAICallout/Testing/CalloutTestHelpers.h`: `EmitTestCallout`, `AssertInQueryResults`, `AssertNotInQueryResults` | Unit: helpers work correctly (TestHelpers self-test) | Pending | haiku | SD-007 (ships with library) |
| 8 | GoogleTests — `Cluiche/Tests/GoogleTests/DiaAICallout/` covering all public API surface (Emit, Query, Claim, Release, Update, helpers) | `dia run googletest --filter="DiaAICallout*"` all pass | Pending | sonnet | |
| 9 | vcxproj sync + registry — `dia docs registry`, verify all headers/cpps in vcxproj, `dia check sln-sync` | Registry entry present, sln clean | Pending | haiku | |
| 10 | **[Debugger]** `DiaAICalloutVisualDebugger` project scaffold — `Dia/DiaAICalloutVisualDebugger/` directory, vcxproj, vcxproj.filters, YAML module doc, register in `Cluiche.sln` | Build succeeds (static lib) | Pending | haiku | #ifdef DIA_DEBUG guarded; depends on DiaAICallout + DiaVisualDebugger |
| 11 | **[Debugger]** `CalloutRegistryDebugger` — `IDebugDomain` impl: identity (domainId="aicallout", group="AIBehavior"), `GetJSONState` emits live callout table (kind, position, radius, faction, ttl, claimed), `HasWorldDrawers()=true` with circle drawers for callout radii | Unit: GetJSONState has correct keys, IDebugDomain contract test shapes | Pending | sonnet | |
| 12 | **[Debugger]** Debugger GoogleTests — IDebugDomain contract tests | `dia run googletest --filter="DiaAICalloutDebugger*"` all pass | Pending | haiku | |
| 13 | **[Debugger]** Debugger vcxproj sync + registry — `dia docs registry`, verify entries, `dia check sln-sync` | Registry entry present, sln clean | Pending | haiku | |
