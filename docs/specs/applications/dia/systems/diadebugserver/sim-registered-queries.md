# Feature Spec: Sim-Registered Queries

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

**Prerequisite:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-queries.md (Done).
**Siblings (same migration):** @docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.md, @docs/specs/applications/dia/systems/diadebugserver/debug-layer-broadcast-removal.md
**Composes with:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-announcement-resync.md and @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-command-help.md. Build after both, because all three touch the sim-hosted registration path and Command Help will make a description mandatory for every sim-hosted name.
**Origin:** backlog row "Cross-PU reads still use mutexes or raw cross-thread reads (debt, 2026-10-30)": "other `QueryRegistry` handlers registered by SimPU modules (QueryRegistry has no lock)".
**Design chosen by the user:** 2026-11-02: make them sim-hosted and veto SimPU registration (not a lock on `QueryRegistry`).
**AD-001 sign-off:** given by the user on 2026-11-02 (at spec approval) for `QueryRegistry` / `DebugServer` (DiaDebugServer) and for each migrated registrant (DiaValueSchema, DiaAssetRuntime, CluicheGameBaseline `EntityInspectorModule`, the Economy on-demand sources, CluicheTest).

## Summary

A `QueryRegistry` handler runs on Main (the DebugServer thread) but several SimPU modules register handlers that read SimPU state. This feature moves every such registrant to `SimQueryHost` so the handler runs on SimPU, and makes `QueryRegistry` refuse registration from anything that is not the Main thread so the bug cannot return.

## Problem Statement

`QueryRegistry` has no lock and its handlers are invoked from `DebugServer::HandleCommand` on Main. A module that registers a handler from SimPU (a) mutates the registry's table from a second thread, and (b) has its handler read SimPU-owned state from Main. The sim-hosted mechanism (Done) exists for exactly this, and the DiaCellLayers viewer already uses it.

Candidates found by grep (to be confirmed in Task 1; each registrant's owning PU decides whether it is a violation): `ValueSchemaModule`, `EntityInspectorModule`, `AssetRuntimeDebugCommands`, the Economy `OnDemandSourceBase` sources (`InspectorDataSourceBase::OnDemandSourceBase::Activate` registers on `GetQueryRegistry()`), the CluicheTest CellLayersViewer stage.

## Goals

1. Every query whose handler touches SimPU state is registered through `SimQueryHost` and answers on SimPU.
2. `QueryRegistry::Register` / `Unregister` from a thread other than the DebugServer's owner thread is rejected with an Error log (reuses the existing register veto hook), so a future SimPU registrant fails loudly in its first test.
3. Queries that genuinely belong to Main (panel, DebugServer own state) are unchanged.
4. The client-visible names, arguments and reply shapes of migrated queries do not change.

## Non-Goals

- Adding a lock to `QueryRegistry`.
- New sim-hosted features (help metadata is Command Help; resync is Announcement Resync).
- Migrating `debug.domain_state` (see Debug Domain State Snapshots).
- Making sim-hosted error replies carry `success:false` (separate loose end).

## API Design

```cpp
// QueryRegistry gains an owner-thread check, installed by the DebugServer.
// Existing veto hook is reused; the new rule is added to SimNameVeto's sibling:
// Built (amended in place): no Dia thread-id type exists, so the id is QueryRegistry::ThreadId (uint32_t,
// the OS thread id); AD-002 is met because no std type appears in the API.
void QueryRegistry::SetOwnerThread(ThreadId id);   // DebugServer::Start sets it, Stop calls ClearOwnerThread()
static ThreadId QueryRegistry::CurrentThreadId();
bool QueryRegistry::Register(...);    // now returns bool
bool QueryRegistry::Unregister(...);  // now returns bool (true = removed)
// Register/Unregister from a different thread: Error log, metric, returns false, nothing changes.
```

Migrated registrants change `registry.Register(name, handler)` to `simQueryHost.RegisterQuery(name, handler)` (plus, once Command Help lands, a description). The handler body is untouched; only the PU it runs on changes, which is the point.

## Acceptance Criteria

1. Task 1 produces a table of every `QueryRegistry` / `RegisterQuery` caller with its owning PU; any SimPU caller of `QueryRegistry` is migrated or has a documented reason it reads no SimPU state.
2. Each migrated query is reachable by the same name over the wire and returns the same JSON shape (E2E or loopback test per registrant, comparing to the pre-migration golden).
3. `QueryRegistry::Register` called from a non-owner thread returns false, logs an Error, and leaves the table unchanged (test using a `std::thread`).
4. The same rule applies to `Unregister`.
5. The owner thread is set when the DebugServer starts and cleared on stop; a registry used standalone in a unit test with no owner set accepts any thread (existing tests keep passing).
6. A handler registered through `SimQueryHost` is executed on the SimPU thread (test records the thread id) and is never executed on Main.
7. (Amended) The Task 1 audit found no Economy on-demand sources: the Economy sources derive `ChangeDetectedSourceBase`, and `OnDemandSourceBase` had no subclass, so it was deleted together with the uncalled `RegisterAssetRuntimeQueryHandlers`. `EntityInspectorModule` is the only migrated registrant.
8. No cross-PU `std::mutex`, `Core::Mutex` or atomic is added; names registered at module start are announced via the existing announce path (Resync keeps them consistent).
9. Metrics: count of queries rejected for thread ownership (`dia.debugserver.query.wrong_thread_register`); an Error log with the name and calling module.
10. The backlog row item "QueryRegistry handlers registered by SimPU modules" is closed with this feature.

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform (CLAUDE.md) | Thread safety | PU boundaries are crossed by streams only | Handlers move to SimPU via `SimQueryHost` (EventStream request/reply) |
| Dia | AD-001 | Module changes need sign-off | Given 2026-11-02 (see header) |
| Dia | AD-002 | No STL in public APIs | The owner-thread id uses a Dia thread-id type, not `std::thread::id` |
| System | DDS-001, DDS-008 | Generic system; forward to DiaAPI/query registries | Name resolution order (sim-hosted, QueryRegistry, DiaAPI) is unchanged |
| System | DDS-005, DDS-010 | JSON for all messages | Replies stay JSON |

## Open Design Questions

1. **Queries that read both Main and Sim state.** `EntityInspectorModule` may combine DebugServer state (Main) and entity data (Sim). Splitting it into two queries changes client-visible names. Task 1 decides per registrant; recommendation is to keep the name on the Sim side and have the Main part published as a topic.
2. **Registration timing.** A handler registered before the DebugServer stream is connected is not announced until `Update`. Resync (Approved) covers a lost announcement but not a registration made before the host exists; Task 1 checks every migrated module's start order.
3. **Latency-sensitive queries.** Sim-hosted replies have a 2 s timeout and one-frame latency. If a migrated query is polled by a UI (Economy), it may need to become a published topic instead. Decide per registrant in Task 1.

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Audit all `QueryRegistry` / `RegisterQuery` callers and owning PUs; classify each (migrate, Main-only, publish as topic); write the plan | M |
| 2 | `QueryRegistry` owner-thread check and tests (ACs 3 to 5, 9) | M |
| 3 | Migrate DiaValueSchema, DiaAssetRuntime registrants and tests (ACs 2, 6) | M |
| 4 | Migrate `EntityInspectorModule`, Economy on-demand sources and the CluicheTest stage (ACs 2, 6, 7) | L |
| 5 | Exhaustive test pass (gap audit, edge tests, mutation check, sanitizer) | M |
| 6 | Docs: module docs, registry, close the backlog row | S |

## Status

`Done`

**Outcome (2026-11-03):** Plan: @docs/specs/applications/dia/systems/diadebugserver/sim-registered-queries.plan.md. Scope shrank after the Task 1 audit: one migrated registrant (`EntityInspectorModule`), two dead registrants deleted (`RegisterAssetRuntimeQueryHandlers`, `OnDemandSourceBase`), ValueSchema stays Main-only.
