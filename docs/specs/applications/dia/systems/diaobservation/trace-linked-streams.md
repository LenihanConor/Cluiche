# Feature Spec: Trace-Linked Streams (stream write to read links, edge stats)

**Parent:** @docs/specs/applications/dia/systems/diaobservation/diaobservation.md
**Depends on:** @trace-spans.md (Tracer, `SpanRecord`), @perf-live-control.md (Open Question 1 spike, 2026-11-16). Consumed by @perf-inspector.md (AC5 live half, tasks 4 and 6)
**Status:** `Done` — 2026-11-17. **Plan:** @trace-linked-streams.plan.md (Done)

## Summary

A stream write records the writer's open span; the reader's span records a **flow link** back to it. Each thread keeps its own span tree, and the link joins them, so a chain that crosses threads (Sim builds, Main parses and pushes) shows as one chain in Perfetto and in the Perf Inspector. The same envelope carries per-stream edge statistics (bytes, rate, data age, write and read cost). `FrameStreamStore` and `EventStreamStore` are both covered.

## Problem

A cost that spans two threads through a stream never appears as one thing: the S-1 card path (`GetJSONState` on Sim, then `PushDomainStatesToPanel` on Main) is invisible as a chain, so the Perf Inspector shows "chain unavailable" against a live game.

## Decisions taken (spike 2026-11-16, recorded in `perf-live-control.md` Open Question 1)

- **Option A, in the store.** `FrameStreamStore<T>::Slot` gains `traceId`, `spanId` and `writeSteadyNs`. The payload `T` and the ~90 stream users are untouched. `Event<T>` already carries an envelope (`sequence`, `timestampUs`) and gains the same link fields.
- **Flow link, not parent.** `SpanRecord` gains `linkTraceId` and `linkSpanId`. The reader's span keeps its own parent; the link never changes the reader's span stack.
- **Always compiled, tracer-gated** (user, 2026-11-16). The link fields exist in Release too; with the tracer off the write path costs one flag test. AC1 gates this. If it fails, the fallback is gating the fields behind `DIA_DEBUG` (a one-line spec change plus a layout split).
- **Both store kinds in this feature**, as separate tasks with separate benchmarks.

## Goals

1. Record, at `Write`/`Send`, the calling thread's current trace id and open span id (when the tracer is started and a span is open).
2. Expose them to the reader through a second accessor; plain `FetchLatest()` is unchanged.
3. Record the link on the reader's span and emit it through the trace sinks and `dia perf convert` (Perfetto flow events).
4. Per-stream edge statistics as metrics with `MetricInfo`: bytes per write, write rate, data age at read, write cost, read cost.
5. Add the missing `DIA_TRACE_ZONE`s on the S-1 chain so it has spans to link: `GetJSONState`, `DomainStateSnapshotPublisher::Update`, `PushDomainStatesToPanel`.

## Non-Goals

- `ServiceStream` stores (not examined; add later if a traced chain needs one).
- Changing payload structs or adding a wire format on streams (SD-PERF-6 stands).
- Making the reader's span a child of the writer's.
- Fixing the store's documented overwrite race (see Open Design Questions).
- The Perf Inspector UI (its own tasks consume this).

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|--------------|
| AC1 | **Write-path cost.** With the tracer off, `FrameStreamStore::Write` and `EventStreamStore::Send` cost no more than 5 ns over the pre-change baseline; the tracer-on overhead is reported. Measured in Release as well as Debug. Stays inside SD-PERF-7 budgets with the S-1 stage running | Benchmark test (reports always; fails above 2x the 5 ns budget) |
| AC2 | **Frame link round trip.** A traced `FrameStream` write gives the reader a link with the writer's `traceId` and `spanId`, across threads. With no open span on the writer, or the tracer off, the reader sees no link | Unit tests incl. a two-thread test |
| AC3 | **Event link round trip.** Same as AC2 through the `Event<T>` envelope. The link survives frame batching; a dropped event leaves no stale link on a later one | Unit tests |
| AC4 | **Flow link in the trace.** A reader span opened with a link records `linkTraceId`/`linkSpanId` in `SpanRecord`; its parent and trace id are unchanged. The trace file sink writes the link; `dia perf convert` emits a Perfetto flow event pair | Unit tests + converter golden file |
| AC5 | **Edge stats.** Per stream: bytes per write, write rate, data age at read, write cost and read cost, registered as metrics with `MetricInfo` (description and unit). Data age uses a fake clock in tests | Unit tests |
| AC6 | **S-1 chain linked.** `GetJSONState`, the snapshot publish and `PushDomainStatesToPanel` appear as spans joined by flow links in a live session; the chain total is reported as a share of one budget. Closes Perf Inspector AC5's live half | E2E on the S-1 stage |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Write-path benchmark: baseline `FrameStreamStore::Write` and `EventStreamStore::Send` cost (tracer off) before any change; record numbers in the plan | AC1 (baseline) | Planned | sonnet | Gates tasks 2-4. If the budget cannot be met, stop and flag the `DIA_DEBUG` fallback to the user |
| 2 | `Tracer` and `SpanRecord`: expose current trace and span id to callers; `linkTraceId`/`linkSpanId`; link-carrying `ScopedZone` constructor; sinks and `TraceFileSink` write the link | AC4 (unit) | Planned | sonnet | Dia/DiaObservation. AD-001 sign-off needed (shared engine type) |
| 3 | `FrameStreamStore` Slot link fields; `StreamReader::FetchLatestLinked(StreamLink&)` (or `LastLink()`); tracer-gated capture; re-run benchmark | AC1, AC2 | Planned | sonnet | Dia/DiaStreams. AD-001 sign-off |
| 4 | `Event<T>` envelope link fields and `EventStreamReader` accessor; batching and drop behaviour; re-run benchmark | AC1, AC3 | Planned | sonnet | Dia/DiaStreams |
| 5 | Edge stats in both stores (relaxed atomics) + metric registration with `MetricInfo` | AC5 | Planned | sonnet | Replaces the "no data only" `FrameStreamDiagnostics`; no per-frame logging (SD-PERF-2) |
| 6 | `dia perf convert`: emit Perfetto flow events from link fields | AC4 | Planned | haiku | Golden file test |
| 7 | S-1 chain: trace zones on `GetJSONState`, `DomainStateSnapshotPublisher::Update`, `PushDomainStatesToPanel`; use the linked accessor on the Main side | AC6 (unit part) | Planned | sonnet | Baseline/DiaVisualDebugger call sites |
| 8 | **Gap audit, edge tests, Release measurement** (explicit task): ring wrap, no-open-span writer, multi-writer stores, tracer start/stop mid-run, link with a full open-span stack | All | Planned | sonnet | No mutation checks |
| 9 | **Verification.** Full `dia run googletest --shards 2`, `dia check deps`, Release benchmark, live E2E for AC6 (quiet machine, kill leftover `CluicheTest.exe`); update Perf Inspector plan tasks 4 and 6 to drop "chain unavailable" | AC1-AC6 | Planned | sonnet | Heavy gates once, here |

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| SD-PERF-6 | Trace ids ride existing payloads; no new wire format on streams | Ids ride the store's slot and event envelope, not the payload `T`; no wire format is added. Payload structs are untouched. The spec text "stream trace linking uses the trace pillar's ids" is met by reusing `traceId`/`spanId` |
| SD-PERF-2 | Telemetry never writes at tick rate | Edge stats are metrics (counters, gauges), never logs; link capture writes into the slot only |
| SD-PERF-7 | Telemetry cost budgets (detached < 0.02 / 0.05 ms, attached Sim < 0.05 / 0.1 ms, Main < 0.1 / 0.25 ms per frame) | AC1 measures the write path against these; task 1 gates the build on it |
| AD-001 | Engine edits need sign-off | Tasks 2 and 3 edit shared Dia headers (`SpanRecord`/`Tracer`, `FrameStreamStore`); **sign-off given 2026-11-16** for tasks 2, 3 and 4 (including the `Event<T>` envelope) |

## Open Design Questions

1. **Writer with no open span.** Recommended: record no link, and the reader treats it as absent. Alternative: open a synthetic root span for the write, which adds cost and noise on every untraced write. Revisit if too many chains show no link because call sites lack `DIA_TRACE_ZONE`.
2. **Overwritten slot.** If the writer wraps the 8-slot ring while a reader holds a pointer, the link may belong to a newer write than the payload read. This is the same accepted race the store already documents, so this feature records it rather than fixing it. Revisit if the Inspector shows chains that visibly mismatch their data.
3. **Slot growth in Release.** The always-compiled choice adds 24 bytes per slot to every frame stream in shipped builds (negligible next to `DomainStateSnapshot` at ~128 KB per slot, but the layout change touches every stream). If AC1 passes only narrowly, reconsider `DIA_DEBUG` gating.
