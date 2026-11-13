**Spec:** @docs/specs/applications/dia/systems/diathreading/job-observation.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ThreadPool` (DiaCore) gains optional thread start/stop callbacks | RED-GREEN: ThreadPool unit test (start/stop each worker once) | Done | sonnet | DiaCore change; AD-001 sign-off given 2026-11-12; ThreadHooks ctor; 14 ThreadPool tests pass |
| 2 | `JobContext`, `JobTag`, `JobTimingRecord`, `IJobObserver`; `JobSystem` Initialize/Submit overloads; timing captured around `fn` | RED-GREEN: AC1-AC5, AC9 | Done | sonnet | DiaThreading; 6 observer tests pass; deps OK; RED step skipped (tests written with impl) |
| 3 | `JobInbox` (MPSC, fixed capacity, drop + count) | RED-GREEN: AC7 | Done | sonnet | DiaObservation, pure, DiaCore only (SD-PERF-3); JobInbox 6 tests pass (RED proven); deps OK |
| 4 | `JobObserver` in DiaObservation + wiring in `JobSystemModule`; PU installs its inbox for its thread at start | RED-GREEN: AC8 | Done | sonnet | 57 scoped tests pass; deps OK; no RED run (written with impl). Gaps: traceParent always 0 (Tracer has no current-span accessor); no PU installs an inbox yet (InstallThreadInbox API ready, lands with perf-inspector JobTimingTable); profile_job record deferred to amendment B; SLOW_ rename of JobObserverTest pending |
| 5 | Benchmark + gap audit (submit from an unregistered thread, worker exit mid-job, inbox full, observer null) | AC5, AC6 | Pending | sonnet | Final verification |
