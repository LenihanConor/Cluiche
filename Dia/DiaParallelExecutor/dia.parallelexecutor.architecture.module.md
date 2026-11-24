---
schema: dia.module.v1
module_id: dia.parallelexecutor
name: DiaParallelExecutor
path: Dia/DiaParallelExecutor
parent_module_id: dia.root
layer: foundation/core
status: active
maturity: dev
version: "0.1"
dependencies:
  required: []
  forbidden: []
public_api:
  headers:
    - DiaParallelExecutor/IParallelExecutor.h
    - DiaParallelExecutor/SerialExecutor.h
  namespaces:
    - Dia::ParallelExecutor::
  entry_points: []
---
# DiaParallelExecutor

`IParallelExecutor` (split a range across workers and join) and `SerialExecutor` (one range on the calling thread).
Extracted from DiaDeferredStep so consumers such as DiaCellFieldBake can use the executor interface without pulling in
savegame, application flow and streams. `CappedParallelExecutor` stays in DiaDeferredStep.

## Responsibilities
- Define the parallel-for contract and the serial fallback.

## Non-responsibilities
- Thread pools, job scheduling, worker caps (DiaThreading, DiaDeferredStep).
