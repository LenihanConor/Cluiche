# Feature Spec: Streams Migration (no cross-PU mutex)

## Parent System
@docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md

**Prerequisite:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-queries.md (Done).

## Summary

Remove `CellLayerViewerDomain::mMutex` and `StateMutex()`. Everything that reaches the viewer from the Main thread (DebugServer queries, DiaAPI commands, `debug.domain_state`, test pointer injection) moves onto streams, per the rule that every PU boundary is crossed by streams. The viewer then lives wholly on SimPU and needs no locking.

## Problem Statement

`CellLayerViewerDomain` holds a non-recursive mutex through its whole `Update` and exposes `StateMutex()` so Main-thread callers can read SimPU state: the four mirror queries (`MirrorQueries.cpp`, 5 lock sites), `debug.domain_state` (`GetJSONState`), and about 16 DiaAPI commands registered with `Dia::API::RegisterCommandJson` that run on Main. This stalls Main behind SimPU work, deadlocks if a callback re-enters the domain, and breaks the platform rule (memory `pu-boundary-streams`). `CellLayersPointerQueue` is a mutex FIFO for the same reason. Sim-Hosted Queries now provides the mechanism.

## Goals

1. Mirror queries (`cells.layer.list`, `cells.select.get`, `cells.select.stats`, `cells.layer.values`) are sim-hosted queries.
2. The viewer's DiaAPI commands are sim-hosted commands, registered on `SimQueryHost` and **no longer** in the DiaAPI `CommandRegistry`.
3. `debug.domain_state` for the viewer returns the latest snapshot the viewer published to a `FrameStream` while requested (see Resolved Question 1); `GetJSONState` on Main reads no live state.
4. `CellLayersPointerQueue` stays a FIFO (one pointer frame per Sim frame) but loses its mutex, because push and pop are both on SimPU.
5. `mMutex`, `StateMutex()` and every `ScopedLock` on it are deleted.
6. Reply sizes fit the 16 KB cap: `cells.layer.values` and `cells.select.get` chunks drop to 1,000 each.

## Non-Goals

- Other domains (`UtilityAIDebugDomain`, `MessageBusDebugDomain`, DomainRegistry maps): backlog debt.
- Streaming chunks to clients (ordered topic push): possible later.
- Changing query or command names, arguments (other than chunk limits) or result shapes.
- The panel command path (`OnCommand`, already over the `DebugPanelCommand` EventStream).

## Behaviour changes (Features 1 and 5 amended in place on approval)

| Area | Before | After |
|------|--------|-------|
| `cells.layer.values` max count | 196,608 cells per call | 1,000 cells per call (about 5.3 KB base64); a full Large grid (6,656 cells) takes 7 calls |
| `cells.select.get` chunk | 8,192 indices per call | 1,000 indices per call |
| Query latency | 0 frames, runs on Main | 1 to 2 frames (request to Sim, reply back) |
| `debug.domain_state` freshness | live | up to one Sim frame old |
| Viewer DiaAPI commands | `Dia::API::CommandRegistry`, run on Main | `SimQueryHost` commands, run on SimPU; not in `CommandRegistry` |

Clamp behaviour for over-limit requests is unchanged (clamp and warn once).

## Acceptance Criteria

1. `grep` finds no `Mutex`, `std::mutex`, `ScopedLock` or `StateMutex` in `Dia/DiaCellLayers/` production code; no test fixture depends on them.
2. The four mirror queries are registered through `SimQueryHostModule::RegisterQuery` from the stage on SimPU and are absent from the Main-side `QueryRegistry`; each returns the same result shape as before.
3. Each viewer DiaAPI command is registered as a sim-hosted command with unchanged name, args and result; none remains in the DiaAPI `CommandRegistry`; the `cells.layer.list` shadowing bug stays fixed (one handler for the name).
4. A pointer-injection command (`cluichetest.celllayers.pointer_frames`) is sim-hosted; `CellLayersPointerQueue` has no mutex and pops one frame per Sim frame.
5. `GetJSONState` on the viewer, when called by `debug.domain_state`, returns the snapshot published by the viewer; the published JSON is the same as the previous live `GetJSONState` output for the same state, plus a `simFrame` field.
6. The snapshot is written once per Sim frame to a `FrameStream` with a fixed 32 KB slot (Task 1 measured up to 23.8 KB compact at the Large grid, 20 layers; the 16 KB reply cap applies to queries only); if the JSON exceeds the slot the snapshot becomes `{"error":"snapshot too large"}` and a Warning is logged once.
7. `cells.layer.values` and `cells.select.get` clamp to 1,000 and echo the clamped `count`; every reply is under the 16 KB cap for the Large grid with 20 layers; the `@dia/editor-ui` `cells.ts` helpers and Vitest conformance fixtures are updated for the new chunk size (including the shared `layer_values.json` and selection fixtures).
8. All existing GoogleTests, Vitest and E2E for DiaCellLayers pass after migration; the `cells.layer.values` E2E pages through a full layer and matches the C++ fixtures; the viewer E2E still covers commands, selection, inspector, query and mirror.
9. Read-only guarantee (Feature 5 AC 12) still holds: queries do not change viewer state.
10. Re-entrancy test: a layer `read` delegate or overlay `draw` callback that calls back into the domain no longer deadlocks (the hazard recorded in the Feature 1 exhaustive pass).
11. Metrics `dia.celllayers.mirror.*` keep counting; Sim-hosted counters (`dia.debugserver.sim.*`) show the traffic.

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Measure the largest realistic `GetJSONState` (Large grid, 20 layers, inspector, query, selection) and the reply sizes at 1,000-chunk; confirm they fit 16 KB. Notes only | S |
| 2 | Re-home `MirrorQueries` onto `SimQueryHost` (stage registers on SimPU); drop its lock and `StateMutex()` use; chunk limits to 1,000; tests, fixtures and Vitest updated | L |
| 3 | Viewer DiaAPI commands become sim-hosted commands (one registration helper that takes a `SimQueryHostModule`); remove them from `CommandRegistry`; unit and E2E updated | L |
| 4 | Snapshot publisher: viewer writes `GetJSONState` JSON to a `FrameStream` each Sim frame; Main-side `GetJSONState` adapter for `debug.domain_state` reads it; size-cap path | M |
| 5 | `CellLayersPointerQueue` loses its mutex; pointer-frame injection command becomes sim-hosted | S |
| 6 | Delete `mMutex`, `StateMutex()`, locks; reentrancy tests; module doc and Features 1 and 5 spec/plan amendments | M |
| 7 | Finalize: exhaustive audit (resumes plan Task 10a for Features 3 to 5), sanitizer, E2E, spec Done | M |

## Files Affected (expected)

`Dia/DiaCellLayers/CellLayerViewerDomain.{h,cpp}`, `MirrorQueries.{h,cpp}`, `ViewStatePublisher` (unchanged), `Testing/*`; `Dia/DiaEditorUI/src/cells.ts` and its tests; `Cluiche/CluicheTest/Modules/TestStages/CellLayersViewerTestStageModule.{h,cpp}`, `CellLayersPointerQueue.h`; the CluicheTest `.diaapp` (stream for the snapshot); `Cluiche/Tests/E2E/scenarios/cluichetest/celllayers/`; GoogleTests under `DiaCellLayers`.

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Dia system | CL-005 | Generic, named for cells, no game types | Unchanged; only the transport changes |
| Dia system | CL-006 | No singletons; the registry is a `ServiceStream` | The snapshot is a `FrameStream`, not a singleton |
| Dia DebugServer | DDS-001, DDS-005, DDS-010 | Generic server, JSON over WebSocket | Sim-hosted handlers return JSON |
| Platform rule | (CLAUDE.md Thread Safety) | PU boundaries are crossed by streams only | Satisfied: no mutex crosses a PU |
| Sim-Hosted Queries | AC 7, 11 | 16 KB reply cap; no mutex for the feature | Chunk limits set to fit; the feature's own state has no lock |

**Conflict note:** removing the viewer commands from the DiaAPI `CommandRegistry` drops the in-process CLI path for them. Wire clients are unaffected.

## Resolved Design Questions

1. **Snapshot cost (resolved 2026-11-01).** Task 1 measured 3.3-4 ms per `GetJSONState` in Debug, so per-frame publish is out. Demand-driven publishing is used: publish only while `debug.domain_state` was requested in roughly the last 2 s (a small request signal back to SimPU). Recorded in the plan notes.
2. **Command ordering (resolved).** Rely on in-order delivery: one request stream drained in order means a later request sees an earlier one's effect. Tests only await each reply. One test proves the ordering (box select, then `cells.select.get` returns the new selection).
3. **Frame stamp (resolved).** The snapshot JSON carries `simFrame`, so clients comparing it with query replies can see the one-frame skew. Query replies are unchanged. Add to AC 5 (shape equals the previous live output plus the `simFrame` field) and to the `@dia/editor-ui` types.

## Status

`Done` (2026-11-02). Plan: [streams-migration.plan.md](streams-migration.plan.md). Viewer mutex and `StateMutex()` deleted; mirror queries and viewer commands sim-hosted; `debug.domain_state` reads a FrameStream snapshot; full GoogleTests, `dia check sanitizer` (ASan) and the celllayers E2E pass.
