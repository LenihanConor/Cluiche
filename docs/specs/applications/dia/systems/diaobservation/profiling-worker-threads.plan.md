# Plan: Profiling Frame Instrumentation — Amendment 2026-11-12 (worker threads and offline analysis)

**Spec:** @docs/specs/applications/dia/systems/diaobservation/profiling-frame-instrumentation.md (section "Amendment 2026-11-12")
**Status:** In Progress
**Scope:** B1-B3 only. Prereqs: Job Observation Done (tasks 1-5), profiling plan task 1 (A1a, subject on ScopeRecord) Done. B1/B2 also need profiling plan task 2 (A1b: `subject` in `profile.jsonl`, name resolution at write time).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **A1b** (profiling plan task 2): `profile.jsonl` emits `"subject"`; Module/PU `Update` pass instance ids | AA1, AA12 | Done | sonnet | Tracked in profiling-frame-instrumentation.plan.md task 2; listed here as the gate for 3-4; 475 scoped tests pass, deps OK, RED proven; subject name table (256, process-wide) falls back to hex once ZZ_ table-full test has run |
| 2 | **B3.** `dia perf convert` (DiaCLI, pure Python, streaming): thread tracks, `X` scope events with subject args, `profile_job` flow events, trips/markers as instants, `--out/--from/--to` | AB4, AB5 | Done | sonnet | Runs against today's `profile.jsonl` plus a fixture with `thread_name`/`profile_job` lines (format per spec B1/B2). AB6 manual check at the end; 9 pytest (8 pass + AB5 slow gated by DIA_RUN_SLOW): 210MB peak 0.1MB tracemalloc, 122s; AB6 Perfetto manual check pending; trip/marker field names assumed (_TS_KEYS), fix at A3 |
| 3 | **B1.** `Profiler::RegisterThreadName`, `thread_name` records in `profile.jsonl`, `JobObserver::OnWorkerStart` names `Worker N` (job.run root scope with owner subject already built) | RED-GREEN: AB1, AB2 | Not Started | sonnet | Needs task 1 |
| 4 | **B2.** `profile_job` record from the worker ring via the drain; none when profiling off | RED-GREEN: AB3 | Not Started | sonnet | Needs task 3. Never written from a PU thread (SD-PERF-2) |
| 5 | Gap audit, edge tests, flood benchmark for ring drops (open question 1), converter edge cases (empty file, truncated last line, unknown owner hex, missing thread_name) | AB1-AB5 | Not Started | sonnet | No mutation checks |
