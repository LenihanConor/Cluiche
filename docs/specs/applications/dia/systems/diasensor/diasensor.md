# System Spec: DiaSensor

**Parent:** @docs/specs/applications/dia/dia.md  
**Status:** Approved

---

## Summary

DiaSensor is a standardised entity perception framework. Sensor components tick at configurable rates and gather raw perceptual data (sight, sound, damage, proximity) from the game world. Raw results are written into a per-entity `SensorResultsComponent`. A module-driven `SensorBlackboardAdapter` pass then distils those raw results into AI concept slots on the entity's `BlackboardComponent` (e.g. `ThreatBoard`, `AwarenessBoard`). The decision layer (DiaCondition / DiaRules / DiaUtilityAI) reads only the blackboard — it has no knowledge that sensors exist.

---

## Architecture

```
DiaEntitySpatial              DamageReceivedComponent    SoundEventList
(spatial queries)             (combat system writes)     (frame-local list)
        │                              │                        │
        │ QuerySector / QueryCircle    │ poll                   │ poll + spatial filter
        ▼                              ▼                        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Sensor Components  (tick at configurable rates, per entity)                │
│                                                                             │
│  SightSensorComponent      — QuerySector over DiaEntitySpatial              │
│  ProximitySensorComponent  — QueryCircle over DiaEntitySpatial              │
│  DamageSensorComponent     — polls DamageReceivedComponent                  │
│  SoundSensorComponent      — polls SoundEventList, spatial radius filter    │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ write raw results
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  SensorResultsComponent   (owned by DiaSensor, attached to sensing entity)  │
│                                                                             │
│  DynamicArrayC<SightResult, 8>     { entity, distance, angle, timestamp }   │
│  DynamicArrayC<ProximityResult,16> { entity, distance }                     │
│  DynamicArrayC<DamageEvent, 4>     { source, amount, timestamp }            │
│  DynamicArrayC<SoundEvent, 4>      { position, type, timestamp }            │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ module-driven pass (after all sensors tick)
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  SensorBlackboardAdapter  (virtual, one per entity archetype)               │
│                                                                             │
│  Reads SensorResultsComponent, writes distilled AI concepts to blackboard   │
│  e.g. "nearest visible enemy" → ThreatBoard.nearestThreat                   │
│       "enemy count in sight"  → AwarenessBoard.knownEnemyCount              │
│       "took damage this frame"→ ThreatBoard.underAttack                     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ writes distilled facts
                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  BlackboardComponent  (DiaBlackboard — shared AI contract)                  │
│                                                                             │
│  ThreatBoard    { nearestThreat, threatCount, underAttack, lastHitTime }    │
│  AwarenessBoard { knownEnemyCount, lastKnownEnemyPosition, alertLevel }     │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ reads conditions
                                       ▼
                         Decision Layer
                         DiaCondition / DiaRules / DiaUtilityAI
```

### Frame ordering

Within a single SimPU frame:

1. `SensorModule::Update()` — ticks all sensor components at their configured rates; writes raw results into `SensorResultsComponent`
2. `SensorModule::RunAdapters()` — iterates all entities with `SensorResultsComponent` + `BlackboardComponent`; calls `SensorBlackboardAdapter::Distil()` on each
3. Decision systems read blackboard — always see a fully-updated view

The `SensorBlackboardAdapter` is virtual. Different entity archetypes carry different adapter subclasses. The module drives the call; the entity controls the distillation logic.

### Sensor tick rates

Each sensor component carries a `tickInterval` (in frames). The module tracks a per-component `tickCountdown` and only runs the sense logic when it reaches zero. Sensors do not all tick every frame. `SensorResultsComponent` retains stale results between ticks — consumers see the last known state, not empty arrays.

### SoundEventList

A frame-local `DynamicArrayC<SoundEvent, kMaxSoundEventsPerFrame>` owned by `SensorModule`. Systems that generate sounds (footsteps, explosions, ability casts) call `SensorModule::EmitSound(position, type, radius)` to register an event. The list is cleared at the start of each frame before sensors tick. `SoundSensorComponent` queries this list spatially during its tick.

---

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| SensorResultsComponent | Per-entity raw result store: sight, proximity, damage, sound results with timestamps | Draft |
| SightSensorComponent | Configurable cone sensor using `EntitySpatialModule::QuerySector`; configurable range, half-angle, layer mask, tick rate | Draft |
| ProximitySensorComponent | Configurable radius sensor using `EntitySpatialModule::QueryCircle`; configurable radius, layer mask, tick rate | Draft |
| DamageSensorComponent | Reactive sensor: polls `DamageReceivedComponent` each tick; records source + amount + timestamp | Draft |
| SoundSensorComponent | Spatial sensor: polls `SoundEventList` within a configurable radius each tick | Draft |
| SensorBlackboardAdapter | Abstract base + default implementation; distils `SensorResultsComponent` into `ThreatBoard` + `AwarenessBoard` slots on the entity's blackboard | Draft |
| SensorModule | IModule: owns `SoundEventList`; drives sensor tick countdown; runs adapter pass in correct frame order | Draft |
| Test Utilities | Helpers to construct a domain with sensor-equipped entities, inject synthetic sight/damage/sound results, and assert blackboard slot values | Draft |

---

## Acceptance Criteria

1. `SensorResultsComponent` stores sight, proximity, damage, and sound results as fixed-capacity `DynamicArrayC` arrays; arrays are not cleared between ticks — stale results persist until overwritten by the next sensor tick
2. `SightSensorComponent` calls `EntitySpatialModule::QuerySector` with configurable range, half-angle, layer mask; results are written to `SensorResultsComponent::sightResults` each tick
3. `ProximitySensorComponent` calls `EntitySpatialModule::QueryCircle` with configurable radius, layer mask; results written to `SensorResultsComponent::proximityResults`
4. `DamageSensorComponent` polls `DamageReceivedComponent` each tick and appends new events to `SensorResultsComponent::damageEvents`; old events older than a configurable window are pruned
5. `SoundSensorComponent` filters `SoundEventList` by distance to the entity each tick; matching events written to `SensorResultsComponent::soundEvents`
6. All sensor components carry a `tickInterval` (frames); `SensorModule` tracks `tickCountdown` per component and skips the sense logic when not at zero
7. `SensorModule::Update()` ticks sensor components; `SensorModule::RunAdapters()` runs after all sensors have ticked in the same frame
8. `SensorBlackboardAdapter::Distil()` is called by `SensorModule` for every entity with both `SensorResultsComponent` and `BlackboardComponent`; it may not be called from sensor component code
9. The default `SensorBlackboardAdapter` registers `ThreatBoard` and `AwarenessBoard` on the blackboard; it writes nearest threat, threat count, under-attack flag, last hit time, known enemy count, last known enemy position
10. `SensorModule::EmitSound(position, type, radius)` appends to the frame-local `SoundEventList`; the list is cleared at the start of `Update()` before any sensor ticks
11. No sensor component has a direct dependency on `DiaBlackboard` — all blackboard writes go through the adapter only
12. No blackboard slot type (`ThreatBoard`, `AwarenessBoard`) has a dependency on DiaSensor — they are owned by DiaSensor but designed as pure data structs
13. Google Tests cover: sight query result population, proximity query result population, damage event recording + pruning, sound event spatial filtering, tick rate skipping, adapter distillation into blackboard slots, frame ordering (sensors tick before adapters run)

---

## Binding Decisions

| Decision | How this system complies |
|----------|--------------------------|
| PD-001 — StringCRC for IDs | All component type IDs and blackboard slot keys use `StringCRC` |
| PD-004 / AD-002 — No STL in public APIs | All result arrays use `DynamicArrayC`; no `std::vector` in any public header |
| AD-003 — Namespace `Dia::<Module>::` | All types live in `Dia::Sensor::` |
| PD-002 / AD-004 — PU/Phase/Module architecture | `SensorModule` implements `IModule`; owned by the SimPU |
| diaentitytemplate — component model | All sensor components use `DIA_COMPONENT` / `FIELD` macro pattern |

---

## Open Design Questions

1. **Damage source**: `DamageSensorComponent` polls `DamageReceivedComponent` — but that component is owned by whatever combat system is in play. Does DiaSensor define a minimal `DamageReceivedComponent` interface, or does the game side provide it and DiaSensor depends on a shared header? The latter couples DiaSensor to a combat system that doesn't exist yet.

2. **Adapter registration**: The default `SensorBlackboardAdapter` covers `ThreatBoard` + `AwarenessBoard`. How does a game register a custom adapter for a specific entity archetype — by attaching a different adapter subclass component, or by a registry keyed on entity type? The component approach (carry the adapter on the entity) is simpler and avoids a global registry.

3. **Multi-domain support**: `EntitySpatialModule` is per-domain. If a stage has two entity domains (e.g. friendly units + neutral objects), does `SightSensorComponent` query both, or only the domain its owning entity belongs to? The sensor would need to hold a reference to one or more `EntitySpatialModule` instances — this could be a module init-time configuration.

---

## Dependencies

- `DiaBlackboard` — per-entity blackboard for AI concept slots
- `DiaEntitySpatial` — spatial queries (`QuerySector`, `QueryCircle`) over entity domains
- `diaentitytemplate` — entity domain, component system, `DIA_COMPONENT` / `FIELD` macros
- `DiaCore` — `DynamicArrayC`, `StringCRC`, `DIA_ASSERT`
- `DiaMaths` — `Vec2`, distance/angle helpers
