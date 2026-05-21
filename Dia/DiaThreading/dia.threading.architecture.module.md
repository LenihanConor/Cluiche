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
  Registers and updates dia.jobs.* metrics directly via MetricRegistry on
  Initialize/Submit/completion.

intent: >
  Own the full observability lifecycle for job execution — registration, per-submit
  increment, per-completion gauge update — so JobSystemModule is a thin lifecycle
  adapter only.

responsibilities:
  - Provide JobSystem: Submit, Wait, IsComplete, Initialize, Shutdown
  - Provide JobHandle: refcounted per-job completion token
  - Register dia.jobs.{queue_depth,active_workers,submitted,completed} on Initialize
  - Increment/update metrics at Submit time and job completion time
  - Null metric pointers on Shutdown

non_responsibilities:
  - Thread, ThreadPool, Mutex, Atomic — remain in DiaCore
  - Priority scheduling, work-stealing, or parent/child fan-out

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
    - dia.observation
  forbidden:
    - dia.applicationflow
---
