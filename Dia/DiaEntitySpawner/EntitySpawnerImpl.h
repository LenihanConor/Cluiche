#pragma once
#ifndef DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H
#define DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTableC.h>
#include <DiaCore/Containers/HashTables/HashTableHashFunctionData.h>
#include <DiaEntity/Entity.h>
#include <DiaEntitySpawner/SpawnerTypes.h>

namespace Dia::Entity {
    class Domain;
    class IBlueprintLoader;
}

namespace Dia::EntitySpawner {

static constexpr unsigned int kMaxChildrenPerEmitter = 128;
static constexpr unsigned int kMaxTrackedEntities    = Dia::Entity::kMaxEntitiesPerDomain;

// ---------------------------------------------------------------------------
// SpawnedChildData — per-child tracking entry (keyed by child entity).
// ---------------------------------------------------------------------------
struct SpawnedChildData {
    Dia::Entity::Entity      emitterEntity;
    Dia::Maths::Vector2D     spawnPosition;   // world-space position at spawn time
    float                    spawnTime = 0.0f;
    float                    age       = 0.0f;
};

// ---------------------------------------------------------------------------
// EmitterState — per-emitter tracking entry (keyed by emitter entity).
// ---------------------------------------------------------------------------
struct EmitterState {
    Dia::Entity::Entity emitterEntity;
    float tokenAccumulator = 0.0f;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxChildrenPerEmitter> children;
    bool burstFired = false;
};

// ---------------------------------------------------------------------------
// EntityHashFunctor — hashes a Dia::Entity::Entity (a Handle<EntityTag>)
// by combining index and generation into a compact table index.
// ---------------------------------------------------------------------------
class EntityHashFunctor
{
public:
    typedef Dia::Entity::Entity Key;
    typedef Dia::Core::Containers::HashTableHashFunctionData TableData;

    unsigned int GetHashIndex(const Key& key, const TableData* tableData) const
    {
        // Combine index + generation into a single value, then mod table size.
        unsigned int raw = (key.GetIndex() * 2654435761u) ^ (key.GetGeneration() * 40503u);
        unsigned int tableSize = tableData ? tableData->GetTableSize() : kMaxTrackedEntities;
        if (tableSize == 0) return 0;
        return raw % tableSize;
    }
};

// ---------------------------------------------------------------------------
// EntitySpawnerImpl — holds tracking tables and implements IEntitySpawner.
// Domain& and IBlueprintLoader* are injected by the owning Module.
// ---------------------------------------------------------------------------
class EntitySpawnerImpl : public Dia::Entity::IEntitySpawner
{
public:
    explicit EntitySpawnerImpl(Dia::Entity::Domain& domain);

    // Called by EntitySpawnerModule before first Spawn/Despawn.
    void SetBlueprintLoader(Dia::Entity::IBlueprintLoader* loader);

    // IEntitySpawner
    Dia::Entity::SpawnResult Spawn(const Dia::Entity::SpawnRequest& request) override;
    void                     Despawn(Dia::Entity::Entity entity) override;

    // Called by the module when an EntityDestroyedMessage arrives from outside.
    // Removes the entity from tracking without calling domain.QueueDestroy.
    void HandleExternalDestroy(Dia::Entity::Entity entity);

    // Despawn the oldest child of an emitter (used by cap-overflow logic).
    void DespawnOldestChild(Dia::Entity::Entity emitterEntity);

    // Despawn all tracked children (called from module DoStop).
    void DespawnAll();

    // DespawnCallback — invoked by DespawnInternal for event publishing.
    // Set by the owning module to forward despawn events to the stream.
    using DespawnCallback = void(*)(void* ctx, Dia::Entity::Entity entity, Dia::Entity::DespawnReason reason);
    void SetDespawnCallback(DespawnCallback cb, void* ctx);

    // Get or create the EmitterState for the given emitter entity.
    // Returns the existing state if already tracked; creates a fresh one otherwise.
    EmitterState& GetOrCreateEmitterState(Dia::Entity::Entity emitter);

    // Tick all child ages by dt; despawns those that exceed their emitter's lifetime.
    // Requires SpawnEmitterComponent on the emitter entity.
    void TickChildAges(float dt);

    // Tick radius despawn — checks SpatialComponent distance for all tracked children.
    // No-op if either the child or its emitter lacks a SpatialComponent, or despawnRadius == 0.
    void TickChildRadii();

    // Returns true if a blueprint loader is currently set.
    bool HasBlueprintLoader() const { return mLoader != nullptr; }

    // Returns the number of currently tracked child entities.
    unsigned int GetTrackedChildCount() const { return mTrackedChildrenList.Size(); }

private:
    // Internal despawn that lets the caller choose whether to actually destroy
    // the entity in the domain (external destroy skips this step).
    void DespawnInternal(Dia::Entity::Entity entity, Dia::Entity::DespawnReason reason, bool destroyInDomain);

    Dia::Entity::Domain&          mDomain;
    Dia::Entity::IBlueprintLoader* mLoader = nullptr;

    DespawnCallback mDespawnCallback    = nullptr;
    void*           mDespawnCallbackCtx = nullptr;

    using ChildTable   = Dia::Core::Containers::HashTableC<
        Dia::Entity::Entity, SpawnedChildData, EntityHashFunctor,
        kMaxTrackedEntities, kMaxTrackedEntities>;

    using EmitterTable = Dia::Core::Containers::HashTableC<
        Dia::Entity::Entity, EmitterState, EntityHashFunctor,
        kMaxTrackedEntities, kMaxTrackedEntities>;

    using ChildEntityList = Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxTrackedEntities>;

    ChildTable      mChildTable;
    EmitterTable    mEmitterTable;
    ChildEntityList mTrackedChildrenList;  // flat list of all tracked children for O(n) iteration
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H
