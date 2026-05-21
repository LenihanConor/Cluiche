---
schema: dia.module.v1
module_id: dia.threading
name: DiaThreading
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaThreading
language: cpp
parent_module_id: dia

summary: >
  Task-based parallelism extracted from DiaCore. Owns JobSystem and JobHandle.
  Provides metric accessors (GetQueueDepth, GetSubmittedCount, GetCompletedCount,
  GetActiveJobCount) for use by JobSystemModule.

intent: >
  Isolate the high-level job abstraction from DiaCore so that modules depending
  on DiaThreading (e.g. CluicheGameBaseline/JobSystemModule) can also depend on
  DiaObservation without creating a circular dependency.

responsibilities:
  - Provide JobSystem: Submit, Wait, IsComplete, Initialize, Shutdown
  - Provide JobHandle: refcounted per-job completion token
  - Expose metric accessors: GetQueueDepth, GetSubmittedCount, GetCompletedCount,
    GetActiveJobCount, GetWorkerCount

non_responsibilities:
  - Thread, ThreadPool, Mutex, Atomic — remain in DiaCore
  - Priority scheduling, work-stealing, or parent/child fan-out
  - Metric registration — that is JobSystemModule's responsibility

public_api:
  headers:
    - DiaThreading/JobSystem.h
  namespaces:
    - Dia::Threading
  entry_points:
    - JobSystem
    - JobHandle
    - JobFn

dependencies:
  required:
    - dia.core
  forbidden:
    - dia.observation
    - dia.applicationflow
---
