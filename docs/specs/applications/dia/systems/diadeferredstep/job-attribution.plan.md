# Plan: Job Attribution and Step Summaries (DiaDeferredStep Feature 5)

**Spec:** @docs/specs/applications/dia/systems/diadeferredstep/job-attribution.md
**Status:** Not Started
**System plan:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.plan.md (row 5)

Starts once Feature 4 is Done. **Task 7 is the only task blocked on another system**
(perf-inspector Task 8, `JobTimingTable` — `Planned`, not built). Tasks 1–6 and 8 are not blocked, so
this plan sequences them so only the last wiring step waits.

## Implementation Patterns

### Part A — the DiaThreading overload (Task 1)
- Add `JobHandle TrySubmit(JobFn fn, const JobTag& tag);` next to the existing `TrySubmit(JobFn)` and
  implement it as `return SubmitImpl(std::move(fn), &tag);` — identical to `Submit(fn, tag)` except it
  does not assert. `SubmitImpl` already takes `const JobTag*`, so there is no new machinery.
- Do not touch `Submit`, `TrySubmit(fn)` or `SubmitImpl`. The existing DiaThreading suite must pass
  **unchanged** (AC 3) — that is the evidence the change is additive.
- Update `dia.threading.architecture.module.md` to document the overload. AD-001 sign-off is already
  given (2026-11-13).

### Part A — tagging (Task 2)
- `JobTag{ owner = channelId.GetValue(), tag = uint32_t(kickKey & 0xFFFFFFFF), range, rangeCount }`.
- `kCoordinatorRange = 0xFFFF` with `rangeCount = 0` marks the coordinator. Define the constant in
  `DeferredStepTypes.h` so the executor and the launcher share it.
- Feature 2 deliberately left both submit calls in one place each (`CappedParallelExecutor.cpp` for
  ranges, `JobStepLauncher.cpp` for the coordinator), so this should be a two-line change plus the
  tag construction.
- **The key truncation is lossy** — comment it at the construction site, and name the summary field
  `kickKeyLow` so nobody later reads it as the key.
- A stolen or inline range is never submitted, so it produces no job record at all (AC 6). Its time
  still reaches the summary through `RangeTiming`.

### Part B — the record (Tasks 4–5)
- `Dia/DiaObservation/Profile/StepSummaryRecord.h` is a POD in `Dia::Observation::Profile`, living in
  DiaObservation so `JobTimingTable` can store it without DiaObservation depending on
  DiaDeferredStep (AD-001 sign-off given). DiaDeferredStep fills it in.
- Add a `static_assert` on `sizeof`, matching `JobTimingRecord`'s precedent in `JobContext.h`, so the
  wire size cannot drift silently.
- `StepSummary.h/.cpp` in DiaDeferredStep holds the pure builder:
  `BuildStepSummary(const LaunchInfo&, const RangeTiming* , uint32_t count, const StepTimes&)` →
  `StepSummaryRecord`. Pure function, no I/O, so Tasks 4–5 are testable with no table, no module and
  no job system.
- **Every number is measured inside DiaDeferredStep** (resolved decision 1): cap wait
  `admitNs - enqueueNs`, worker wait `startNs - admitNs`, CPU `sum(endNs - startNs)`,
  first start `min(startNs)`. `JobTimingRecord` is **not** consulted — worker-submitted records are
  dropped today (`JobObserver.h`: threads without an inbox have their records counted only), and cap
  wait is invisible outside the gate anyway.
- `inlineFallbacks` (a failed `TrySubmit`) and `stolenRanges` (the gate was full) are counted from
  distinct `RangeTiming` flags and must never be merged (AC 19).
- Flags: `kStalled`, `kRanInlineWholly`, `kRestoredFromSave`. A restored step has no range timings, so
  it is built with zeroed range fields rather than omitted (AC 18).

### Part B — scheduler and module wiring (Tasks 3, 6, 7)
- The scheduler already records the stall; add `kickNs`, `readyNs` and `applyNs` to `ChannelEntry` and
  pass them to the builder at apply time. `readyNs` is the moment completion was first observed —
  Feature 1's poll pass already finds it, so store the timestamp alongside `completionSeenKey`.
- The summary is built and written **on SimPU, synchronously, in the apply path** — the only point
  where kick, ready, apply and stall are all known (AC 20), and the one that satisfies SD-PERF-7.
- `DeferredStepModule` owns a `DynamicArrayC<StepSummaryRecord, 16>` ring (AC 24) and forwards to a
  `JobTimingTable*` **if one is available**. No table → build, ring, discard, no allocation (AC 22).
  Keep the forwarding behind one `if` so Task 7 is genuinely the only blocked edit.

### Testing
- A test `IJobObserver` that captures every `JobContext` is how AC 4–7 are asserted — install it on a
  real `JobSystem` via `Initialize(n, nullptr, &observer)` and read back the tags.
- AC 12's reconciliation is asserted **with a tolerance**, not exactly: `steady_clock` resolution and
  scheduler jitter make an exact identity false. State the tolerance in the test name.
- AC 23's soak is the only performance claim in the feature, so it runs in **Release**.

### Verification cadence
- Per task: `dia run googletest --filter="DeferredStepSummary*:DeferredStepAttribution*:JobSystem*" --shards 2`.
- `JobSystem*` stays in the filter because Task 1 touches DiaThreading.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | RED → GREEN: `JobSystem::TrySubmit(JobFn, const JobTag&)` + module doc. Run the **existing** DiaThreading suite unchanged | AC 1–3: tagged attribution matches `Submit(fn, tag)`; invalid handle before `Initialize` and after `Shutdown` with no assert; existing suite green with no test edits | Not Started | sonnet | AD-001 sign-off given 2026-11-13. Additive only — do not refactor `SubmitImpl` |
| 2 | RED → GREEN: `kCoordinatorRange`; tag every range and the coordinator; stolen/inline ranges untagged | AC 4–7: owner/tag/range/rangeCount per range via a capturing `IJobObserver`; coordinator sentinel; no record for stolen ranges; two concurrent channels distinguishable | Not Started | sonnet | Two-line change if Feature 2 kept the submits centralised; if not, centralise them first |
| 3 | RED → GREEN: `StepTimes` on the scheduler — `kickNs`, `readyNs` (from the existing poll pass), `applyNs`, reusing the stall figure | Ordering `kickNs <= readyNs <= applyNs` on a normal step; `readyNs` equals the first completion observation; a restored step has no `readyNs` | Not Started | sonnet | Small, but it is the data every later task consumes |
| 4 | `Dia/DiaObservation/Profile/StepSummaryRecord.h` + `static_assert` on size. RED → GREEN: `BuildStepSummary` as a pure function — sums, min, flags | AC 10–11, 19: field ordering; `cpuNs`/`capWaitNs`/`workerWaitNs` as sums; `rangeCount` matches the split; `inlineFallbacks` vs `stolenRanges` kept distinct | Not Started | sonnet | AD-001 sign-off given. Pure function — no table, no module, no job system needed |
| 5 | RED → GREEN: the summary variants | AC 9, 13–18: one record per apply and none for rejected kicks, `Unregister` or a load discard; stalled (`kStalled`, `stallNs > 0`, apply on the same Sim entry); early (`readyNs < applyNs` by the slack); wholly inline; restored from save; AC 12's reconciliation with a stated tolerance | Not Started | sonnet | AC 12 is a tolerance test by nature — say so in the test name |
| 6 | RED → GREEN: module wiring — the last-16 ring, the no-table path | AC 21–22, 24: one record per step instance and no per-range record published; with no table the summary is still built, nothing allocates, the step applies; the ring is readable and wraps at 16 | Not Started | sonnet | Keep the table forwarding behind a single `if` so task 7 is a one-line change |
| 7 | **Blocked on perf-inspector Task 8.** Wire `JobTimingTable::AddStepSummary`; confirm the record reaches Main through the PU's existing snapshot | AC 20 end-to-end: a record written on SimPU appears in the table; nothing is written from a worker thread | Not Started | sonnet | Do **not** fork `JobTimingTable` — if it is still unbuilt when the rest is Done, leave this row Blocked and close the feature without it |
| 8 | Exhaustive test pass + close-out: gap audit of every AC, every flag, every "no record written" path; the Perfetto check (AC 8 — run the stage with profiling, `dia perf convert`, confirm the channel as subject on worker `job.run` scopes; manual, recorded in notes); AC 23's Release soak (1000 steps, summaries on vs off); `dia check sanitizer` over the filter; `dia check deps`; observation scan; update the system spec features table | All ACs, quoted; the soak figures in notes | Not Started | sonnet | AC 8 is manual inspection, not an automated test. AC 23 must be Release |
