# Feature Spec: Job Attribution and Step Summaries

**Parent:** @docs/specs/applications/dia/systems/diadeferredstep/diadeferredstep.md (Feature 5)
**Status:** Done (2026-11-18)
**Plan:** @docs/specs/applications/dia/systems/diadeferredstep/job-attribution.plan.md
**Depends on:** Features 1–4. Part B needs @docs/specs/applications/dia/systems/diaobservation/perf-inspector.md
Task 8 (`JobTimingTable`), which is `Planned`, not built.

## Summary

Makes deferred-step work legible in the Perf Inspector and in Perfetto. Two halves that can land
independently:

**Part A — attribution (buildable now).** Every coordinator and every range is submitted with a
`JobTag` carrying the channel as `owner` and the kick key as `tag`, so the already-built job
observation stamps each `job.run` profile scope with the channel. Worker time in Perfetto splits by
channel — weather versus ecology — instead of reading as one anonymous blob. Needs one new overload
in DiaThreading, `TrySubmit(fn, tag)`.

**Part B — summaries (gated).** One `StepSummaryRecord` per step instance, written on SimPU at apply
time into the PU's `JobTimingTable`: the full timeline (kick → first range start → ready → apply)
plus CPU time across ranges, cap wait, worker wait, range count, inline fallbacks and stall. That is
the row the Inspector's Workers view shows when you click a step.

DiaDeferredStep measures every number in the summary itself, so Part B depends on `JobTimingTable`
only as a *destination*, never as a timing source.

## Goals

- "Which channel spent that 18 ms of worker time?" is answerable from a Perfetto trace.
- "Why was this step late?" is answerable from one record: cap wait says another channel held the
  cap, worker wait says the pool was busy, run time says the step itself is simply heavy.
- Cost stays inside SD-PERF-7: one record per step instance, written on SimPU, not per range.

## Design

### Part A — the tag

```cpp
// Dia::Threading::JobTag, already defined in DiaThreading/JobContext.h
JobTag{
    owner      = channelId.GetValue(),          // the channel's StringCRC value
    tag        = uint32_t(kickKey & 0xFFFFFFFF),// low 32 bits of the kick key
    range      = rangeIndex,                    // kCoordinatorRange (0xFFFF) for the coordinator
    rangeCount = rangeCount                     // 0 for the coordinator
}
```

`JobTag::tag` is 32 bits and `StepKey` is 64, so the key is truncated. For a game-day index that is
~11.7 million years of play, and the field is for grouping a trace, not for correctness — but it is
documented as lossy so nobody later treats it as the key.

`JobSystem` has `Submit(fn, tag)` but only `TrySubmit(fn)`. The capped executor must use the
non-asserting form (a shutdown race is normal there), so DiaThreading gains one overload forwarding
to the existing private `SubmitImpl(fn, &tag)` — three lines, no behaviour change for existing
callers, AD-001 sign-off given in the interview (user decision 2026-11-13).

### Why the summary measures its own timings

`JobObserver` delivers each `JobTimingRecord` to **the submitting thread's** inbox, and threads with
no inbox have their records counted and dropped (`JobObserver.h`: "Threads without an inbox submit
with inbox == nullptr; their records are only counted"). Range jobs are submitted by the coordinator,
which is a worker thread with no inbox — so their raw records would be dropped today.

That does not matter, because DiaDeferredStep can measure everything itself from Feature 2's
`RangeTiming`:

| Summary field | Source |
|---|---|
| cap wait | `admitNs - enqueueNs` (gate admission — only DiaDeferredStep can see this) |
| worker wait | `startNs - admitNs` (submitted → range body began) |
| CPU time | `sum(endNs - startNs)` across ranges |
| first range start | `min(startNs)` |
| ready | the coordinator returned from `Run` |
| kick, apply, stall | the scheduler, on SimPU |

Profiling attribution is unaffected by the inbox gap: the `job.run` profile scope carries the owner
as its subject and is pushed to the worker's own job ring, drained by the profile writer. So Part A
works with no job-observation change at all.

### Part B — the record

```cpp
// Dia/DiaObservation/Profile/StepSummaryRecord.h — lives in DiaObservation so JobTimingTable
// can hold it without DiaObservation depending on DiaDeferredStep.
namespace Dia::Observation::Profile {

    struct StepSummaryRecord
    {
        uint32_t owner            = 0;   // channel StringCRC value
        uint32_t kickKeyLow       = 0;   // low 32 bits, as in JobTag
        uint64_t applyKey         = 0;
        uint64_t kickNs           = 0;
        uint64_t firstRangeStartNs= 0;
        uint64_t readyNs          = 0;   // Run returned
        uint64_t applyNs          = 0;
        uint64_t cpuNs            = 0;   // summed across ranges
        uint64_t capWaitNs        = 0;   // summed across ranges
        uint64_t workerWaitNs     = 0;   // summed across ranges
        uint64_t stallNs          = 0;
        uint16_t rangeCount       = 0;
        uint16_t inlineFallbacks  = 0;
        uint16_t stolenRanges     = 0;
        uint16_t flags            = 0;   // kStalled, kRanInlineWholly, kRestoredFromSave
    };
}
```

Written once, on SimPU, inside `Apply` handling — the same place the stall is recorded — via
`JobTimingTable::AddStepSummary(record)`. A step restored from a save has no range timings, so it is
written with `kRestoredFromSave` and zeroed worker fields rather than being silently omitted.

## Acceptance Criteria

### Part A — DiaThreading overload

1. `JobSystem::TrySubmit(JobFn fn, const JobTag& tag)` exists and forwards to `SubmitImpl(fn, &tag)`,
   matching `Submit(fn, tag)`'s attribution behaviour exactly.
2. It returns an invalid handle (no assert) before `Initialize` and after `Shutdown`, exactly as
   `TrySubmit(fn)` does.
3. Existing `Submit` / `TrySubmit` callers are unaffected: `dia run googletest` for the DiaThreading
   suite is green with no test changes.

### Part A — tagging

4. Every range job is submitted with `owner` = the channel's `StringCRC` value, `tag` = the low 32
   bits of the kick key, `range` = its range index and `rangeCount` = the split size.
5. The coordinator job is submitted with the same `owner` and `tag`, `range = kCoordinatorRange`
   (0xFFFF) and `rangeCount = 0`, so a trace can tell the coordinator from its ranges.
6. A range that runs on the coordinator thread (stolen, or after a failed `TrySubmit`) is not
   submitted at all, so it produces no job record — its time is still in the step summary, flagged.
7. Two channels running concurrently produce job records whose `owner` values differ, and each range's
   `range` / `rangeCount` identify it uniquely within its step.
8. A profiling session over the CluicheTest stage yields `job.run` scopes on worker `thread_id`s
   whose subject is the channel id, and `dia perf convert` turns them into a Perfetto trace where
   worker time is attributable per channel.

### Part B — summary content

9. Each applied step writes exactly one `StepSummaryRecord`. A step that is kicked and applied
   produces one; a step rejected at `Kick` produces none; a step discarded by `Unregister` or by a
   load produces none.
10. `kickNs <= firstRangeStartNs <= readyNs <= applyNs` holds for every record from a normally
    launched step.
11. `cpuNs`, `capWaitNs` and `workerWaitNs` are the sums over that step's ranges, and `rangeCount`
    matches the executor's `GetLastRangeCount()` for a single-pass step.
12. The timeline reconciles: `firstRangeStartNs - kickNs` is accounted for by the coordinator's own
    cap wait plus worker wait, within clock resolution. Asserted with a tolerance, not exactly.
13. A late step's record has `stallNs > 0` and the `kStalled` flag, and `applyNs - readyNs` is
    approximately the stall — the stall and the apply are on the same Sim entry.
14. A step that completed early has `stallNs == 0` and `readyNs < applyNs` by roughly the slack.
15. With `worker_cap` = 1 and two channels kicked on the same key, the second channel's record has
    `capWaitNs > 0`, and on an otherwise idle pool its `workerWaitNs` is near zero — cap pressure and
    pool pressure are distinguishable.
16. With `worker_cap` equal to the worker count and an idle pool, `capWaitNs` is 0 for every record.
17. A step run wholly inline (no job system, Feature 2 AC 17) writes a record with
    `kRanInlineWholly`, `rangeCount = 1`, `workerWaitNs = 0`, `capWaitNs = 0` and `cpuNs` equal to
    the inline run — so a serial fallback is visible rather than looking like a free step.
18. A step restored from a save writes a record with `kRestoredFromSave`, its real `applyKey`,
    `stallNs = 0` and zeroed range fields.
19. `inlineFallbacks` counts only ranges that ran inline after a failed `TrySubmit`; `stolenRanges`
    counts ranges the coordinator took because the gate was full. The two are never conflated.

### Part B — destination and cost

20. The record is written on SimPU, synchronously, in the apply path — never from a worker thread.
21. One record per step instance reaches the PU's `JobTimingTable`; no per-range record is published
    from DiaDeferredStep (SD-PERF-7).
22. With no `JobTimingTable` available (a test harness, or a game without the Perf Inspector), the
    summary is computed and discarded without allocating, and the step still applies normally.
23. Writing a summary costs no measurable frame time: a 1000-step soak shows no regression in SimPU
    frame time against the same stage with summaries disabled.
24. `DeferredStepModule` exposes the last N (16) summaries in memory for tests and for a future debug
    domain, independent of whether a `JobTimingTable` consumed them.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaThreading/JobSystem.h/.cpp` | New `TrySubmit(JobFn, const JobTag&)` overload (AD-001 sign-off) |
| `Dia/DiaThreading/dia.threading.architecture.module.md` | Document the overload |
| `Dia/DiaDeferredStep/CappedParallelExecutor.cpp` | Tag every submitted range; use the new overload |
| `Dia/DiaDeferredStep/JobStepLauncher.cpp` | Tag the coordinator with `kCoordinatorRange` |
| `Dia/DiaDeferredStep/StepSummary.h/.cpp` | New — builds a `StepSummaryRecord` from `RangeTiming` plus the scheduler's kick/ready/apply/stall times; owns the ring of the last 16 |
| `Dia/DiaDeferredStep/DeferredStepScheduler.h/.cpp` | Record `kickNs`, `readyNs`, `applyNs`, `stallNs` and hand them to the summary builder at apply |
| `Dia/DiaDeferredStep/DeferredStepModule.h/.cpp` | Resolve the `JobTimingTable` (or none), forward summaries, expose the ring |
| `Dia/DiaObservation/Profile/StepSummaryRecord.h` | New — the record type, so `JobTimingTable` need not know DiaDeferredStep (AD-001 sign-off) |
| `Dia/DiaObservation/Profile/JobTimingTable.h/.cpp` | `AddStepSummary` + storage — **owned by perf-inspector Task 8**; this feature consumes it and must not fork it |
| `Cluiche/Tests/GoogleTests/DiaThreading/TestJobSystem.cpp` | Overload tests (AC 1–3) |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestStepSummary.cpp` | New |
| `Cluiche/Tests/GoogleTests/DiaDeferredStep/TestJobAttribution.cpp` | New |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SD-PERF-7 (perf-inspector) | Telemetry cost budgets and wire rules | One record per step instance, built and written on SimPU at apply; no per-range record leaves the module (AC 20–23) |
| DFS-004 | The JobSystem stays FIFO with no priorities | The only DiaThreading change is a non-asserting tagged submit; no scheduling behaviour is touched |
| DFS-002 | `Run` reads only the captured copy | Summary building reads `RangeTiming`, which the coordinator wrote before `Run` returned; the scheduler reads it on SimPU after `IsComplete` is true — a happens-before the `Wait`/completion already establishes |
| PD-004 / AD-002 | No STL in public APIs | `StepSummaryRecord` is a POD; the ring is a `DynamicArrayC` |
| AD-001 | Sign-off for engine API changes | Two: `JobSystem::TrySubmit(fn, tag)` and `StepSummaryRecord` in DiaObservation. Both signed off 2026-11-13 |
| PU-boundaries-are-streams | No mutex-guarded cross-PU reads | Summaries go into SimPU's own `JobTimingTable` and reach Main through the PU's existing snapshot, which perf-inspector already specifies |

## Test Plan

- **GoogleTests** (`--filter=DeferredStepSummary*:DeferredStepAttribution*:JobSystem*`, `--shards 2`):
  - the overload: tagged attribution matches `Submit(fn, tag)`; invalid handle before init and after
    shutdown; the existing DiaThreading suite unchanged
  - tagging: owner/tag/range/rangeCount per range, coordinator sentinel, stolen ranges untagged, two
    concurrent channels distinguishable — read back through a test `IJobObserver` that captures every
    `JobContext`
  - summary: one per apply and none for the non-apply paths; field ordering; sum reconciliation;
    stalled, early, inline-wholly and restored-from-save variants; `inlineFallbacks` versus
    `stolenRanges`
  - cap versus pool pressure: AC 15 and AC 16 with a forced cap
  - no `JobTimingTable`: summaries still built, nothing allocated, step applies
- **Perfetto check (AC 8):** run the CluicheTest `DeferredStep` stage with profiling on, then
  `dia perf convert <session-dir>`, and confirm the channel appears as the subject on worker-thread
  `job.run` scopes. This is a manual inspection recorded in the plan notes, not an automated test.
- **Soak (AC 23):** 1000 steps with summaries on versus off, comparing SimPU frame time. Release
  build, since this is the one performance claim in the feature.
- **Gap audit** as the final task: every flag, every "no record written" path and both sign-off
  surfaces covered.

## Open Design Questions

1. **Worker-submitted job records are dropped today.** `JobObserver` routes a `JobTimingRecord` to the
   submitting thread's inbox, and a worker has none, so every range's raw record is counted as
   unowned and thrown away. This feature deliberately does not depend on that path — it measures its
   own timings — but it means the Inspector's raw job ring will never show deferred-step ranges, only
   the step summary and the profile scopes. If that turns out to be wanted, the fix belongs in
   job-observation (an explicit inbox on submit, or routing by `owner`), not here. Worth a backlog
   row against perf-inspector.
2. **`JobTag::tag` is 32 bits and the kick key is 64.** Truncation is harmless for grouping, but a
   summary carries both `kickKeyLow` (to join against job records) and the full `applyKey`, which is
   mildly confusing. The alternative is a per-step sequence number as the tag and the keys only in the
   summary — cleaner joins, one more concept. Decide when the Inspector UI is built against real data.
3. **Part B could ship before perf-inspector Task 8.** Everything except `AddStepSummary` is
   independent, and AC 22 already requires the no-table path to work. Building Part B first and
   wiring the destination later is viable; it just means the feature sits Done-but-unobservable for a
   while. The plan should sequence Part A, then Part B's builder, then the wiring, so only the last
   task is blocked.
4. **Should the last-16 ring (AC 24) become a debug domain?** It is exactly the data a
   `DiaDeferredStepVisualDebugger` would show, and the system spec already names that as a possible
   follow-up. Keeping it as a plain accessor for now avoids pre-building a domain nobody has asked
   for, and `IDebugDomain` can wrap it later with no change here.

## Resolved Decisions

1. **DiaDeferredStep measures its own timings** rather than correlating against `JobTimingRecord`s.
   Cap wait is only visible inside the gate anyway, and the worker-inbox gap would otherwise block
   the whole feature. `JobTimingTable` is a destination, not a source.
2. **`TrySubmit(fn, tag)` is added to DiaThreading** (user decision 2026-11-13, AD-001 sign-off). The
   alternatives — calling the asserting `Submit(fn, tag)` behind a hand-rolled state check, or
   shipping untagged ranges — either duplicate what `TrySubmit` exists for or defeat the feature's
   purpose.
3. **`StepSummaryRecord` lives in DiaObservation** so `JobTimingTable` can store it without
   DiaObservation depending on DiaDeferredStep. DiaDeferredStep fills it in.
4. **One record per step instance, written on SimPU at apply** — SD-PERF-7's cost rule, and it is also
   the only point where kick, ready, apply and stall are all known.
