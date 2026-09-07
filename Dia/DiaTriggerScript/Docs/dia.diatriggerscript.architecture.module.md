---
schema: dia.module.v1
module_id: dia.triggerscript
name: DiaTriggerScript
parent_module_id: dia.condition
layer: domain/gameplay/core
path: Dia/DiaTriggerScript
status: active
maturity: dev
namespace: Dia::TriggerScript

dependencies:
  required:
    - dia.condition
    - dia.core
    - dia.geometry2d
    - dia.entityspatial
    - dia.streams
    - dia.application
    - dia.observation
  forbidden: []
---

# DiaTriggerScript

Data-driven scripted level events. `TriggerScriptModule` owns a flat list of `TriggerDef`s per level, polls them each tick, and dispatches actions when conditions fire.

## Public API

- `TriggerDef` — immutable data record (id, trigger type + params, action list, one-shot/repeating flag)
- `TriggerFiredEvent` — published on sim DiaStreams channel on each trigger fire
- `ITriggerActionHandler` — dispatch interface; caller registers named handlers
- `TriggerActionRegistry` — named handler registration and dispatch; explicitly constructed
- `TriggerScriptModule` — IModule on SimPU; LoadFromJson, Update, IncrementCount, IsFired/IsActive

## Dependencies

- DiaCondition — ConditionExpr, IConditionContext, ConditionRegistry
- DiaCore — StringCRC, DynamicArrayC, Json, DIA_ASSERT
- DiaGeometry2D — AABB (spatial trigger region)
- DiaEntitySpatial — IEntitySpatialQuery (entity-in-region queries)
- DiaStreams — sim channel write for TriggerFiredEvent
- DiaApplicationFlow — IModule, SimModule, EventStreamWriter
- DiaObservation — logging
