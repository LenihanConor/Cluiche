---
schema: dia.module.v1
module_id: dia.deferredstep
name: DiaDeferredStep
path: Dia/DiaDeferredStep
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: dev
version: "0.1"
dependencies:
  required:
    - dia.core
    - dia.observation
    - dia.threading
    - dia.savegame
  forbidden: []
public_api:
  headers: []
  namespaces:
    - Dia::DeferredStep::
  entry_points: []
---
# DiaDeferredStep

Async world-step scheduler: one deferred step per channel, with Kick/Tick/DrainAll lifecycle. IDeferredStep contract, IStepLauncher seam, DeferredStepScheduler, and executor abstractions (IParallelExecutor + SerialExecutor).

Namespace: `Dia::DeferredStep`. Runs on SimPU (deferred step channel subscribers).

## Responsibilities

- IDeferredStep contract for async step work
- IStepLauncher seam for kicking deferred steps
- DeferredStepScheduler: one step per channel, Kick/Tick/DrainAll
- IParallelExecutor + SerialExecutor abstractions

## Non-Responsibilities

- Threading or job system (future features)
- Save/load (feature scope)
- Module wiring (application)
