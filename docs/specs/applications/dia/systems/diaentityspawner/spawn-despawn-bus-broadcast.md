# Feature Spec: spawn-despawn-bus-broadcast

**System:** DiaEntitySpawner
**App:** Dia
**Status:** Approved

## Summary

`EntitySpawnerModule` already publishes `EntitySpawnedEvent`/`EntityDespawnedEvent` via `Dia::ApplicationFlow::EventStreamWriter<T>` — the cross-PU stream system, aimed at consumers like RenderPU (e.g. spawning VFX). That mechanism stays untouched. This feature adds a direct `Bus::Broadcast<T>()` call for both events, right alongside the existing stream writes, so same-thread sim gameplay systems can subscribe without polling or without needing to consume a cross-PU stream meant for a different audience. There is no Observer/Listener to build here — the module already owns the moment of publication; it just needs a second delivery path.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | DiaEntitySpawner — [diaentityspawner.md](diaentityspawner.md) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` (`Bus::Broadcast`) |

## Goals

- Sim-thread gameplay systems can `Bus::Subscribe<EntitySpawnedEvent>`/`Subscribe<EntityDespawnedEvent>` without any cross-PU stream plumbing.
- The existing `EventStreamWriter` publish path is unchanged — both audiences (cross-PU stream consumers, same-thread bus subscribers) are served independently.

## Non-Goals

- Replacing `mSpawnedWriter`/`mDespawnedWriter` with the bus. Both stay.
- Touching `EntitySpawnerModule`'s existing subscription to `EntityDestroyedMessage` on Domain's private mailbox (`mDestroyedSub`) — that's a separate, pre-existing cross-boundary read not in scope here.
- Building any Observer/Listener interface — none is needed; the module already owns publication directly.

## Acceptance Criteria

- `EntitySpawnerModule::DoUpdate` calls `bus.Broadcast<EntitySpawnedEvent>(...)` and `bus.Broadcast<EntityDespawnedEvent>(...)` at the same point it currently writes to `mSpawnedWriter`/`mDespawnedWriter`.
- `EntitySpawnedEvent`/`EntityDespawnedEvent` (currently plain structs in `SpawnerTypes.h`) are declared in a `.diagamemessages` file and generated via `dia codegen messages`, giving them a `kTypeId` and bus registration wiring.
- `EntitySpawnerModule` gains access to the shared `Bus` — either via constructor injection or a wiring hook analogous to `OnConnectStreams` — without `EntitySpawnerImpl` (the lower-level implementation class) taking any bus dependency. Only the module layer touches the bus.
- A subscriber to `EntitySpawnedEvent`/`EntityDespawnedEvent` via `Bus::Subscribe` receives the event; the existing `EventStreamWriter` consumers are unaffected (regression check on any existing stream consumer).
- No change to `EntitySpawnerImpl`'s `DespawnCallback` internal bridging mechanism — it stays exactly as-is, feeding the module which now has two publish paths instead of one.

## Design

### Where the broadcast call goes

```cpp
// EntitySpawnerModule::DoUpdate — existing stream writes stay; add bus broadcasts alongside.
mSpawnedWriter.Write(spawnedEvent);
mBus->Broadcast(spawnedEvent);   // new

mDespawnedWriter.Write(despawnedEvent);
mBus->Broadcast(despawnedEvent); // new
```

### Bus access

`EntitySpawnerModule` needs a `Bus&`/`Bus*` member, wired the same way `mDomain`/`mLoader` already are (constructor injection, "must outlive this module") or via a wiring hook mirroring `OnConnectStreams`. Recommend constructor injection for consistency with the module's existing pattern (`Domain&`, `IBlueprintLoader&` are both constructor parameters already).

### Timing

No designated pre-Primary step is needed here — same reasoning as the DiaEconomy/DiaOrder/DiaBehaviourTree adapters. A one-tick lag between spawn/despawn and bus delivery is acceptable; nothing about "a new entity exists" or "an entity was removed" is latency-critical for subscribers.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntitySpawner/EntitySpawnerModule.h` | Add `Bus&`/`Bus*` member; add to constructor signature |
| `Dia/DiaEntitySpawner/EntitySpawnerModule.cpp` | Add `bus.Broadcast<T>()` calls in `DoUpdate`, alongside existing stream writes |
| `Dia/DiaEntitySpawner/Messages/entityspawner_messages.diagamemessages` | New — `EntitySpawnedEvent`/`EntityDespawnedEvent` declarations |
| `Dia/DiaEntitySpawner/DiaEntitySpawner.vcxproj` | Add new file; add `DiaMessageBus` reference |
| Stage/composition-root code that constructs `EntitySpawnerModule` | Pass the `Bus&` at construction |
| `Tests/GoogleTests/EntitySpawner/SpawnDespawnBusBroadcastTests.cpp` | New |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-004 | No STL in public APIs | `EntitySpawnedEvent`/`EntityDespawnedEvent` already plain value structs; no change needed there. |
| SD-MBX2-001 | Bus is sole subscriber for cross-system traffic | Only the module layer touches the bus; `EntitySpawnerImpl` stays bus-free, consistent with its existing separation from `EntitySpawnerModule`. |

## Open Design Questions

None — this is a small, additive feature with no unresolved design gaps.

## Status

`Approved`
