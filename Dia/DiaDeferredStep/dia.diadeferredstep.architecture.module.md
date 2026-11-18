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
    - dia.application
    - dia.streams
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
- DeferredStepModule: SimModule that hosts the scheduler on SimPU

## Non-Responsibilities

- Deriving step keys (the consumer's clock supplies them via IStepKeySource)
- Owning the job system (consumed from JobSystemService)

## Manifest Ordering

Two ordering rules govern DeferredStepModule's placement in the application manifest:

1. **IStepKeySource must advance before DeferredStepModule updates.** The module that provides the IStepKeySource (typically a clock or world-sim module) must be listed BEFORE DeferredStepModule so that its key advance is visible to the scheduler in the same sim step.

2. **Consumer channels must unregister before DeferredStepModule stops.** Modules that register channels on the scheduler must be listed AFTER DeferredStepModule so that when phases stop (in reverse manifest order), consumers stop first and have a chance to unregister their channels while the scheduler is still alive.

**Conflict resolution:** A module that is both the IStepKeySource and a channel consumer must be listed BEFORE DeferredStepModule (satisfying rule 1). Because stop runs in reverse manifest order, such a module stops after DeferredStepModule, so it must tolerate its channels having already been discarded by DeferredStepModule::DoStop.

**Enforcement:** Both rules are enforced by `dia validate manifest` through the optional module-declaration field `requires_tick_after`: an array of module `instance_id`s that must appear EARLIER in the same processing unit's module list. The validator errors if a referenced instance is missing from that PU, is listed at or after the declaring module, or the value is not an array of strings. Absent field means no constraint; the C++ manifest loader ignores the key.

```json
{ "instance_id": "DeferredStepModule", "type_id": "DeferredStepModule",
  "requires_tick_after": ["DeferredStepClockModule"], ... },
{ "instance_id": "MyConsumerModule", "type_id": "MyConsumerModule",
  "requires_tick_after": ["DeferredStepModule"], ... }
```

Declare the constraint on `DeferredStepModule` (after its key source) and on every module that registers a channel on it (after `DeferredStepModule`). See F4 Open Design Question 1.
