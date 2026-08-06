# DiaEntitySpawner — Implementation Plan

**Spec:** @docs/specs/applications/dia/systems/diaentityspawner/diaentityspawner.md
**Status:** In Progress

---

## Implementation Patterns

### Module scaffold pattern
`dia scaffold module DiaEntity EntitySpawner --layer simulation` produces the header, cpp, module.md skeleton, and vcxproj entries. All subsequent tasks edit the scaffolded files rather than creating new ones.

### IModule lifecycle pattern
```cpp
class EntitySpawnerModule : public IModule {
public:
    static constexpr StringCRC kUniqueId = StringCRC("EntitySpawnerModule");
    void Init(ModuleContext& ctx) override;
    void Update(float dt, ModuleContext& ctx) override;
    void Shutdown(ModuleContext& ctx) override;
    IEntitySpawner& GetSpawner();
private:
    EntitySpawnerImpl mSpawner;
};
```
`EntitySpawnerImpl` holds the tracking table and implements `IEntitySpawner`. Module wraps it and wires streams/loader refs.

### IBlueprintLoader lazy-cache pattern
```cpp
IBlueprintLoader* mBlueprintLoader = nullptr;

IBlueprintLoader& GetLoader(ModuleContext& ctx) {
    if (!mBlueprintLoader)
        mBlueprintLoader = ctx.GetModule<BlueprintLoaderModule>().GetLoader();
    return *mBlueprintLoader;
}
```
Cache invalidated in `Shutdown()`. Same pattern as DiaEntitySpatial module ref.

### Internal tracking table
Two tables in `EntitySpawnerImpl`:
- `HashTable<Entity, SpawnedChildData>` — keyed by child entity; holds `emitterEntity`, `spawnTime`, `age`.
- `HashTable<Entity, EmitterState>` — keyed by emitter entity; holds `tokenAccumulator`, `DynamicArrayC<Entity> children` (FIFO order for cap overflow).

All tables use DiaCore containers (no STL). `Entity` key uses generational handle equality.

### SpawnEmitterComponent token accumulation pattern
```cpp
state.tokenAccumulator += comp.rate * dt;
while (state.tokenAccumulator >= 1.0f) {
    TrySpawn(emitterEntity, comp, ctx);
    state.tokenAccumulator -= 1.0f;
}
```
Fractional tokens carry across frames. `rate=0` skips the while loop entirely.

### Cap enforcement (FIFO overflow)
Before each new spawn, if `children.Size() >= comp.cap && comp.cap > 0`: despawn `children[0]` with reason `Cap`, remove from front.

### EntityDestroyedMessage subscription
`Init()` subscribes to `EntityDestroyedMessage` on the domain stream. Handler removes the destroyed entity from both tracking tables and fires `EntityDespawnedEvent` with reason `Explicit` (external destroy is treated as explicit from the spawner's perspective, since the spawner did not initiate it — alternatively use a dedicated `ExternalDestroy` reason; see Task 3 note).

### Event publishing pattern
```cpp
ctx.GetStream<EntitySpawnedEvent>().Push({ entity, request.blueprintId, request.tag });
ctx.GetStream<EntityDespawnedEvent>().Push({ entity, reason });
```
Published on sim-thread `FrameStream` after the domain `EndOfFrame()` commit.

### DiaObservation instrumentation sites
- `DIA_LOG_INFO("EntitySpawner", "Spawned entity={} blueprint={} pos={}", ...)` — after successful spawn
- `DIA_LOG_INFO("EntitySpawner", "Despawned entity={} reason={}", ...)` — on every despawn
- `DIA_LOG_ERROR("EntitySpawner", "Blueprint not found: {}", blueprintId)` — on `BlueprintNotFound`
- `DIA_TRACE_ZONE("EntitySpawner::Spawn")` — inside `Spawn()` critical path
- `DIA_TRACE_ZONE("EntitySpawner::Update")` — inside `Update()` emitter loop
- `DIA_PROFILE_SCOPE("EntitySpawnerModule::Update")` — outer Update scope
- Metrics registered in `Init()`: `spawner.active_count` (Gauge), `spawner.spawn_rate` (Counter), `spawner.despawn_reason.lifetime/radius/cap/explicit` (Counters)
- Health: `ReportHealthStatus(HealthStatus::Degraded, "IBlueprintLoader unavailable")` if loader is null at Update time

### GoogleTest structure
Single file `GoogleTests/DiaEntity/TestEntitySpawner.cpp`. Test fixture owns a `Domain` + `EntitySpawnerModule` initialized in `SetUp()`. Blueprint stubs registered via a `MockBlueprintLoader`. Tests step `Update(dt)` manually to control time.

### CluicheTest stage structure
`dia scaffold stage Spawner` produces the 9 touch points. Stage sets up 4 emitter entities in `InitPhase`, each with a distinct `SpawnEmitterComponent` policy. `VelocityComponent` on spawned entities moves them outward. DiaVisualDebugger overlay registered in `InitPhase`. DiaAutomation checkpoints registered in `InitPhase`, evaluated in `UpdatePhase`.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Scaffold EntitySpawner module — `dia scaffold module`, vcxproj entries, module.md YAML | `dia run googletest` compiles clean | Done | haiku | DiaEntitySpawner.vcxproj, .filters, 6 source stubs, Docs/md, sln entry (3.0-Gameplay), GoogleTests wired; build verified |
| 2 | Core types — `SpawnRequest`, `SpawnResult`, `SpawnError`, `DespawnReason`, `IEntitySpawner`, `EntitySpawnedEvent`, `EntityDespawnedEvent` | Compile only | Done | sonnet | SpawnerTypes.h; all 7 types in Dia::Entity:: namespace; 7693 tests pass |
| 3 | `SpawnEmitterComponent` — `DIA_COMPONENT` + all `FIELD` declarations; `EmitterState` internal struct | Compile only | Not Started | sonnet | Note: decide here whether external-destroy fires `Explicit` or a new `ExternalDestroy` reason value |
| 4 | `EntitySpawnerImpl` — tracking tables, `Spawn()` + `Despawn()` impl, lazy `IBlueprintLoader` cache, `EntityDestroyedMessage` subscription | Unit: SpawnRequest API cases (valid spawn, BlueprintNotFound, DomainFull) | Not Started | sonnet | Core logic task; no module wiring yet |
| 5 | `EntitySpawnerModule` — IModule wrapper: `Init()`, `Update(dt)` emitter loop (token accumulation + cap + despawn conditions), `Shutdown()` | Unit: rate/burst/cap/lifetime/radius/events/lifecycle/tag/parent test cases | Not Started | sonnet | Wires impl into module lifecycle; most of the spec's GoogleTest suite proven here |
| 6 | DiaObservation pass — all log/trace/profile/metric/health instrumentation sites | `dia run googletest` green; metrics appear in session output | Not Started | sonnet | Follow instrumentation sites listed in Implementation Patterns |
| 7 | GoogleTest suite — exhaustive `TestEntitySpawner.cpp` per spec test table | `dia run googletest --filter="EntitySpawner*"` all green | Not Started | sonnet | MockBlueprintLoader stub; manual `Update(dt)` stepping |
| 8 | CluicheTest SpawnerStage — 4 emitters, VelocityComponent, DiaVisualDebugger overlay, 4 DiaAutomation checkpoints | `dia run cluichetest` reaches SpawnerStage; visual + checkpoint pass | Not Started | sonnet | `dia scaffold stage Spawner`; confirms E2E integration |
| 9 | Module doc + registry — finalise `dia.entity.entityspawner.architecture.module.md` YAML frontmatter; `dia docs registry` | `dia check deps` clean | Not Started | haiku | Fill deps, public headers, layer assignment |
| 10 | Spec-done — `dia docs spec-done`; update plan status to Done | — | Not Started | haiku | Final housekeeping commit |
