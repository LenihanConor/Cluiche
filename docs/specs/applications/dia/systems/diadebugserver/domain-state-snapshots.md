# Feature Spec: Debug Domain State Snapshots

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

**Siblings (same migration):** @docs/specs/applications/dia/systems/diadebugserver/sim-registered-queries.md, @docs/specs/applications/dia/systems/diadebugserver/debug-layer-broadcast-removal.md
**Origin:** backlog row "Cross-PU reads still use mutexes or raw cross-thread reads (debt, 2026-10-30)"; the user ruled on 2026-10-30 that every PU boundary is crossed by streams. The Economy inspector `NotifySubscribers` race from the same row was fixed on 2026-11-02 (`DebugServer::PublishTopicOrdered`, commit `b2a01f2d`).
**Precedent:** the DiaCellLayers Streams Migration (`ViewerStateSnapshot`, FrameStream with `simFrame`, demand-driven fallback).
**Design chosen by the user:** 2026-11-02: a FrameStream snapshot per domain (not demand-driven only, not a per-domain mix).
**AD-001 sign-off:** given by the user on 2026-11-02 (at spec approval) for `DiaVisualDebugger` (`IDebugDomain` additions, new snapshot types), `VisualDebuggerModule` and `DebugPanelPageModule` (CluicheGameBaseline), and the `UtilityAIDebugDomain` / `MessageBusDebugDomain` owners. 

## Summary

Stop `DebugPanelPageModule` (Main PU) from reading the Sim-owned `DiaDebugDomainRegistry` and calling `IDebugDomain::GetJSONState()` on Sim-owned domains from the Main thread. `VisualDebuggerModule` (Sim PU) serialises each domain's panel state on SimPU into a FrameStream snapshot stamped with `simFrame`; Main builds the debug panel payload and answers `debug.domain_state` from the latest snapshot only.

## Problem Statement

Today `VisualDebuggerModule` (Sim) publishes its `DiaDebugDomainRegistry` as a ServiceStream. `DebugPanelPageModule` (Main) reads that registry directly: it walks the registry's two `std::unordered_map`s (`VisitAll`, `FindDomain`) and calls `GetJSONState()` on each domain, from the Main thread, while SimPU may be registering domains or mutating the state those domains read. Specific hazards:

- `DiaDebugDomainRegistry` maps are unlocked and read cross-thread (`BuildDomainStatesRoot`, `QueryDomainState`).
- `UtilityAIDebugDomain` guards `mScoresEnabled` with an atomic only because Main reads it; the `UtilitySet` it reports on is SimPU-owned and unguarded.
- `MessageBusDebugDomain::GetJSONState` reads `Bus::GetLastTickLedger()` and the `LedgerHistory` ring while SimPU's bus ticks. Its header says "on the sim thread", which stopped being true when the panel moved to a Main module.
- `debug.domain_state` runs on Main and reaches the same objects.

## Goals

1. Domain state crosses the PU boundary only as a value, on a FrameStream. No Main-thread call into a Sim-owned domain, registry or ledger.
2. `GetJSONState()` is called only on SimPU, by `VisualDebuggerModule`, once per Sim frame at most.
3. The debug panel and `debug.domain_state` keep their current JSON shape (`BuildDomainStatesRoot`, `_id/_displayName/_group/_description/_accent`), plus a `simFrame` field.
4. Sim cost is bounded: snapshots are produced only while someone is looking (panel visible or a `debug.domain_state` request seen within a window), with a hard per-frame byte budget.
5. Domains stay unaware of streams: the snapshot is built by the module, not by each domain.
6. Domain commands (`OnCommand`) keep their existing Main to Sim EventStream path.

## Non-Goals

- Changing the `IDebugDomain` interface, the panel JSON schema or the panel UI.
- The Visual Debugger world drawers (`HasWorldDrawers`, `Register/GetDrawer`); they already run on the render path owned by their PU.
- Sim-hosting `debug.domain_state` as a query (rejected: a 2 s round trip is wrong for a live panel). It reads the snapshot.
- The DiaCellLayers viewer's own snapshot (already migrated).

## API Design

```cpp
namespace Dia::VisualDebugger {
    // One serialised frame of every domain's panel state, produced on SimPU.
    struct DomainStateSnapshot {
        uint64_t simFrame;
        uint32_t domainCount;
        uint32_t jsonBytes;       // length of the packed JSON below
        uint32_t flags;           // bit 0: truncated (budget hit), bit 1: stale-domain skipped
        char     json[kMaxSnapshotBytes];   // {"<displayName>": {state...}, ...}
    };
    static constexpr uint32_t kMaxSnapshotBytes = 192 * 1024;   // sized by Task 1 measurement
}
```

- Producer: `VisualDebuggerModule::DoUpdate` (Sim), a `FrameStreamWriter<DomainStateSnapshot>`.
- Consumer: `DebugPanelPageModule` (Main), a `FrameStreamReader<DomainStateSnapshot>`. `Console::BuildDomainStatesRoot` and `QueryDomainState` take the parsed snapshot, not the registry.
- Demand: a Main to Sim `EventStream` message `domain_state.interest {window_ms}` sent while the panel is open or a query arrives (reuses the existing Main to Sim command path in `VisualDebuggerModule`).

Exact types are fixed in Task 1 after measuring the real payload of the 14 migrated domains.

## Acceptance Criteria

1. `GetJSONState()` of every domain is called only on SimPU (test: a domain that records the calling thread id; Main never matches).
2. `DebugPanelPageModule` holds no `ServiceStreamReader<DiaDebugDomainRegistry>`; the `DomainRegistry` ServiceStream is removed or becomes Sim-private.
3. The panel payload for N domains built from a snapshot is identical to today's, apart from the added `simFrame` (golden JSON test over the 14 existing domains).
4. `debug.domain_state {id}` and `{}` answer from the latest snapshot: known id returns that domain's state, unknown or non-string id returns `{"error": ...}` unchanged (existing `TestDomainStateQuery` cases pass against the snapshot form).
5. No snapshot is produced while no consumer has expressed interest within the window; interest resumes production within one Sim frame.
6. Over-budget: when the serialised state exceeds `kMaxSnapshotBytes` the snapshot sets the truncated flag, keeps whole domains only (never a cut JSON), and logs a Warning at most once per second plus increments `dia.debugvisualiser.domain_snapshot.truncated`.
7. A domain registered or unregistered on SimPU appears or disappears in the next snapshot; Main never sees a half-registered domain (multi-thread test with register/unregister churn while Main reads).
8. `UtilityAIDebugDomain` no longer needs `std::atomic`; `mScoresEnabled` is a plain `bool` mutated only by `OnCommand` on SimPU (removal covered by a test that toggles it via the command stream).
9. `MessageBusDebugDomain` reads `GetLastTickLedger()` / `GetLedgerHistory()` only inside `GetJSONState()` on SimPU; the stale "no cross-PU connection" comment is corrected.
10. Stale data is explicit: if the producer stops (module stopped, Sim stalled > 5 s) the panel shows the last snapshot's `simFrame` and the health reporter `dia.debugvisualiser.domain_snapshot` goes Degraded, same pattern as `ViewerStateHealth`.
11. Metrics: `published`, `bytes`, `truncated`, `reads_pending`, `age_ms`; Info logs on interest start and stop.
12. A ThreadSanitizer-style stress test (Sim thread churning domains and state, Main thread reading snapshots) passes under `dia check sanitizer`.
13. No `std::mutex`, `Core::Mutex` or `std::atomic` added across the PU boundary; the existing atomic in `UtilityAIDebugDomain` is removed.

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform (CLAUDE.md) | Thread safety | PU boundaries are crossed by streams only | The only cross-PU data is `DomainStateSnapshot` on a FrameStream and the interest/command EventStream |
| Dia | AD-001 | Module changes need sign-off | Given 2026-11-02 (see header) |
| Dia | AD-002 | No STL in public APIs | Snapshot is a POD with a fixed char buffer; the registry's `std::unordered_map` stays private to its Sim owner |
| System | DDS-001 | Generic system | Snapshot is domain-agnostic JSON; no domain-specific code in the producer |
| System | DDS-005, DDS-010 | JSON for all messages | Snapshot payload is the existing panel JSON |

## Open Design Questions

1. **Buffer size versus real payloads.** 14 domains, some with tables (MessageBus history, UtilityAI scores). If the sum regularly exceeds the budget, truncating whole domains hides data silently. Task 1 measures first; the fallback is per-domain snapshots on separate topics so a big domain cannot starve the others.
2. **Interest signalling (decided 2026-11-02: interest window).** "Panel visible" is a Main-side fact. Sending an interest event every 500 ms is cheap but is one more Main to Sim message; the alternative is always producing at a low rate (every 8th frame), which is simpler but costs Sim time with no viewer. Chosen by the user: interest window.
3. **Domains that act on state.** A few domains mutate state inside `GetJSONState()` (paging, active tab). Moving the call to SimPU makes that safe, but Task 1 must audit each of the 14 for hidden Main-thread assumptions (for example UI-thread-only data).

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Audit the 14 domains (thread assumptions, payload size), measure total payload, fix the snapshot type and budget; write the plan | M |
| 2 | `DomainStateSnapshot`, packer (whole-domain budget, truncated flag), pure unit tests (ACs 3, 6) | M |
| 3 | `VisualDebuggerModule` producer, interest handling, metrics and health (ACs 1, 5, 7, 10, 11) | L |
| 4 | `DebugPanelPageModule` and `QueryDomainState` read the snapshot; remove the registry reader (ACs 2, 4) | M |
| 5 | Remove the `UtilityAIDebugDomain` atomic and fix the `MessageBusDebugDomain` threading comment (ACs 8, 9) | S |
| 6 | Stress test and `dia check sanitizer` (AC 12) | M |
| 7 | Exhaustive test pass (gap audit, edge tests, mutation check) | M |
| 8 | Docs: module docs, registry, close the backlog row | S |

## Status

`In Progress` (Tasks 1-4 Done, 5-8 pending). Plan: @docs/specs/applications/dia/systems/diadebugserver/domain-state-snapshots.plan.md
