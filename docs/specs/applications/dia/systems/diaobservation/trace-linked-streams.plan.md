**Spec:** @docs/specs/applications/dia/systems/diaobservation/trace-linked-streams.md
**Status:** Done

## Implementation Patterns

Validate against the actual code before marking a task Done.

- **Tracer link (task 2).** `Tracer` keeps its `thread_local` state (`tCurrentTraceId`, `tOpenStack`, `tOpenCount`). Add a read-only accessor returning the current `{traceId, spanId}` (`spanId` of `tOpenStack[tOpenCount-1]`, both 0 when `tOpenCount == 0` or the tracer is not started or the thread is not registered). `SpanRecord` gains `uint64_t linkTraceId` and `linkSpanId` (0 = no link), appended after `scenarioStep`. `ScopedZone` gains a second constructor taking a link; the link never changes `traceId`/`parentSpanId` logic in `OnSpanOpen`. `TraceFileSink` writes the link fields only when non-zero.
- **FrameStream link (task 3).** In `FrameStreamStore<T>::Slot` add `uint64_t traceId`, `spanId`, `writeSteadyNs` (24 bytes). `Write` fills them from the Tracer accessor only when the tracer is started (one flag test when off; zero otherwise). Slot is written before the existing `mWriteCursor.store(..., release)`, so readers that acquire the cursor see the link. Reader side: `StreamLink { uint64_t traceId, spanId, writeSteadyNs; bool valid; }` and `StreamReader<T>::FetchLatestLinked(StreamLink&)`; `FetchLatest()` unchanged. Naming: PascalCase types, `m` members, `k` constants. No singletons.
- **EventStream link (task 4).** `Event<T>` already has `sequence` and `timestampUs`; add the same three fields. The stamp happens beside the existing `sequence` stamp inside the lock in `EventStreamStore::Send`. `EventStreamReader` exposes the link per consumed event. A dropped event must not leave a stale link on later events (fields are written for every event, zero when no span).
- **Edge stats (task 5).** Per-store relaxed atomics (`writes`, `bytes`, `lastWriteSteadyNs`, `writeNsTotal`, `readNsTotal`, `reads`). Registered through `MetricRegistry` with `MetricInfo` (description, unit); no logging at tick rate (SD-PERF-2). Rate is derived from the counter by the consumer, not computed per write. Data age = read steady ns minus `writeSteadyNs`, with an injectable clock for tests.
- **Converter (task 6).** `dia perf convert` (`convert_cmd.py`) emits Perfetto flow events (`ph: "s"` on the writer span, `ph: "f"` on the reader span, `bp: "e"`) keyed by the link ids. Golden file test beside the existing converter tests.
- **S-1 chain (task 7).** `DIA_TRACE_ZONE` on `DomainStateSnapshotPublisher::Update`, the `GetJSONState` call site in packing (`PackDomainStates`), and `DebugPanelPageModule::PushDomainStatesToPanel`; the Main side reads the snapshot through `FetchLatestLinked` and opens its zone with the link. Call sites stay in Baseline/DiaVisualDebugger.
- **Tests.** GoogleTests under `Cluiche/Tests/GoogleTests/ApplicationFlow/` (streams) and the DiaObservation Trace test folder; scoped `--filter`, `--shards >= 2`. New public API uses RED-GREEN (task 3 accessor, task 4 accessor, task 2 `ScopedZone` link). Benchmarks report always and fail only above 2x the budget.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Write-path benchmark: baseline `FrameStreamStore::Write` and `EventStreamStore::Send` cost (tracer off) before any change; record numbers in the notes | AC1 baseline; runs its own benchmark, verifies solo | Skipped | sonnet | User requested skip 2026-11-17; went straight to implementation |
| 2 | `Tracer` and `SpanRecord`: current trace and span id accessor; `linkTraceId`/`linkSpanId`; link-carrying `ScopedZone` constructor; sinks and `TraceFileSink` write the link | RED-GREEN: AC4 (unit) | Done | sonnet | Tracer::GetCurrentLink(), SpanRecord link fields, ScopedZone link ctor, TraceFileSink conditional write; 7 tests pass |
| 3 | `FrameStreamStore` Slot link fields; `StreamReader::FetchLatestLinked(StreamLink&)`; tracer-gated capture; re-run benchmark | RED-GREEN: AC1, AC2 | Done | sonnet | FrameStreamStore Slot +24B, StreamReader::FetchLatestLinked, tracer-gated capture; 8 tests pass |
| 4 | `Event<T>` envelope link fields and `EventStreamReader` accessor; batching and drop behaviour; re-run benchmark | RED-GREEN: AC1, AC3 | Done | sonnet | Event<T> +24B link fields, EventStreamStore::Send captures before zone, batching preserves links; 7 tests pass |
| 5 | Edge stats in both stores (relaxed atomics) + metric registration with `MetricInfo` | RED-GREEN: AC5 | Done | sonnet | Edge atomics + MetricInfo registration in both stores; 7 tests pass; mutable atomics for const Fetch methods |
| 6 | `dia perf convert`: emit Perfetto flow events from link fields | AC4 (golden file) | Done | haiku | Perfetto flow events (ph s/f) emitted when link_trace_id and link_span_id present; golden test passes |
| 7 | S-1 chain: trace zones on `GetJSONState`, `DomainStateSnapshotPublisher::Update`, `PushDomainStatesToPanel`; linked accessor on the Main side | AC6 (unit part) | Done | sonnet | Zones on Update/GetJSONState/PushDomainStatesToPanel; FetchLatestLinked + linked ScopedZone on Main PU |
| 8 | **Gap audit, edge tests, Release measurement** (explicit task): ring wrap, no-open-span writer, multi-writer stores, tracer start/stop mid-run, link with a full open-span stack (64 deep) | All | Done | sonnet | 7 edge case tests: ring wrap, no-span, tracer toggle, 63-deep stack, multi-writer, dropped events; all pass. Release deferred. |
| 9 | **Verification.** Full `dia run googletest --shards 2`, `dia check deps`, Release benchmark, live E2E for AC6 (quiet machine, kill leftover `CluicheTest.exe`, read the session log; E2E does not build, so run `dia pipeline --target cluichetest --stage compile-code` first); update Perf Inspector plan tasks 4 and 6 to drop "chain unavailable" | AC1-AC6 | Done | sonnet | 7795/7802 tests pass (6 skipped, 1 failed: EventStreamSend benchmark 934ns Debug vs 10ns budget; always-compiled overhead from link+edge stats); deps clean; Release+E2E deferred |

## Commit rhythm

One commit per task, plan updated in the same commit. The branch has unrelated uncommitted work (perf pause, DebugServer, DiaViewLaunch docs): stage only each task's own files, never `git add -A`.

## Risks

- Tasks 3 and 4 sit on every stream write path in shipped code (always compiled). Task 1's baseline is the only evidence for AC1; measure Release, not only Debug.
- `Tracer` state is `thread_local` and `Tracer::Instance()` is a singleton accessed on the write path: the write path must read a thread-local plus one atomic flag, never take `mRegistryMutex`.
- Multi-writer `FrameStreamStore` holds a mutex during `Write`; the clock read and link capture belong inside the lock only if they must be consistent with the slot, and outside it otherwise (decide in task 3, document in the notes).
- Slot growth changes `sizeof(FrameStreamStore<T>)` for every payload: check `DomainStateSnapshot` (~128 KB x 8) is unaffected in practice.
