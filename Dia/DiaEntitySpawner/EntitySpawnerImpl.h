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
    Dia::Entity::Entity emitterEntity;
    float               spawnTime = 0.0f;
    float               age       = 0.0f;
};

// ---------------------------------------------------------------------------
// EmitterState — per-emitter tracking entry (keyed by emitter entity).
// ---------------------------------------------------------------------------
struct EmitterState {
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

private:
    // Internal despawn that lets the caller choose whether to actually destroy
    // the entity in the domain (external destroy skips this step).
    void DespawnInternal(Dia::Entity::Entity entity, Dia::Entity::DespawnReason reason, bool destroyInDomain);

    Dia::Entity::Domain&          mDomain;
    Dia::Entity::IBlueprintLoader* mLoader = nullptr;

    using ChildTable   = Dia::Core::Containers::HashTableC<
        Dia::Entity::Entity, SpawnedChildData, EntityHashFunctor,
        kMaxTrackedEntities, kMaxTrackedEntities>;

    using EmitterTable = Dia::Core::Containers::HashTableC<
        Dia::Entity::Entity, EmitterState, EntityHashFunctor,
        kMaxTrackedEntities, kMaxTrackedEntities>;

    ChildTable   mChildTable;
    EmitterTable mEmitterTable;
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H
