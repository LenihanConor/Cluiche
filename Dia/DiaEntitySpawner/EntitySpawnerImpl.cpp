#include <DiaEntitySpawner/EntitySpawnerImpl.h>
#include <DiaEntitySpawner/SpawnerTypes.h>
#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/IBlueprintLoader.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::EntitySpawner {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
EntitySpawnerImpl::EntitySpawnerImpl(Dia::Entity::Domain& domain)
    : mDomain(domain)
    , mLoader(nullptr)
{
}

// ---------------------------------------------------------------------------
// SetBlueprintLoader
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::SetBlueprintLoader(Dia::Entity::IBlueprintLoader* loader)
{
    mLoader = loader;
}

// ---------------------------------------------------------------------------
// SetDespawnCallback
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::SetDespawnCallback(DespawnCallback cb, void* ctx)
{
    mDespawnCallback    = cb;
    mDespawnCallbackCtx = ctx;
}

// ---------------------------------------------------------------------------
// GetOrCreateEmitterState
// ---------------------------------------------------------------------------
EmitterState& EntitySpawnerImpl::GetOrCreateEmitterState(Dia::Entity::Entity emitter)
{
    if (!mEmitterTable.ContainsKey(emitter))
    {
        EmitterState newState;
        newState.emitterEntity = emitter;
        mEmitterTable.Add(emitter, newState);
    }
    return mEmitterTable.GetItem(emitter);
}

// ---------------------------------------------------------------------------
// Spawn
// ---------------------------------------------------------------------------
Dia::Entity::SpawnResult EntitySpawnerImpl::Spawn(const Dia::Entity::SpawnRequest& request)
{
    if (!mLoader)
    {
        DIA_LOG_WARNING("DiaEntitySpawner", "Spawn: no IBlueprintLoader set — cannot load blueprint '%s'",
                        request.blueprintId.AsChar());
        return Dia::Entity::SpawnResult{ Dia::Entity::Entity{}, Dia::Entity::SpawnError::BlueprintNotFound };
    }

    Dia::Entity::Entity entity = mDomain.CreateEntity();
    if (!entity.IsValid())
    {
        DIA_LOG_WARNING("DiaEntitySpawner", "Spawn: Domain is full — cannot create entity for blueprint '%s'",
                        request.blueprintId.AsChar());
        return Dia::Entity::SpawnResult{ Dia::Entity::Entity{}, Dia::Entity::SpawnError::DomainFull };
    }

    // Build a minimal blueprint JSON that the loader can interpret by blueprint ID.
    Json::Value bpJson;
    bpJson["blueprintId"] = request.blueprintId.AsChar();
    mLoader->Load(mDomain, bpJson);

    // Commit component additions.
    mDomain.EndOfFrame();

    // --- Track child ---
    SpawnedChildData childData;
    childData.emitterEntity  = request.parent;
    childData.spawnPosition  = request.position;
    childData.spawnTime      = 0.0f;
    childData.age            = 0.0f;
    mChildTable.Add(entity, childData);
    if (!mTrackedChildrenList.IsFull())
    {
        mTrackedChildrenList.Add(entity);
    }

    // --- Track in emitter table ---
    if (request.parent.IsValid())
    {
        EmitterState& state = GetOrCreateEmitterState(request.parent);
        if (!state.children.IsFull())
        {
            state.children.Add(entity);
        }
        else
        {
            DIA_LOG_WARNING("DiaEntitySpawner",
                            "Spawn: emitter children list full (cap %u) — oldest child will be despawned",
                            kMaxChildrenPerEmitter);
            DespawnOldestChild(request.parent);
            // After despawn frees a slot, add the new child.
            if (mEmitterTable.ContainsKey(request.parent))
            {
                EmitterState& updatedState = mEmitterTable.GetItem(request.parent);
                if (!updatedState.children.IsFull())
                {
                    updatedState.children.Add(entity);
                }
            }
        }
    }

    return Dia::Entity::SpawnResult{ entity, Dia::Entity::SpawnError::None };
}

// ---------------------------------------------------------------------------
// Despawn (public — explicit caller-driven despawn)
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::Despawn(Dia::Entity::Entity entity)
{
    DespawnInternal(entity, Dia::Entity::DespawnReason::Explicit, /*destroyInDomain=*/true);
}

// ---------------------------------------------------------------------------
// HandleExternalDestroy
// Called when EntityDestroyedMessage arrives — domain is already destroying
// the entity so we only remove it from our tracking tables.
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::HandleExternalDestroy(Dia::Entity::Entity entity)
{
    DespawnInternal(entity, Dia::Entity::DespawnReason::Explicit, /*destroyInDomain=*/false);
}

// ---------------------------------------------------------------------------
// DespawnOldestChild
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::DespawnOldestChild(Dia::Entity::Entity emitterEntity)
{
    if (!mEmitterTable.ContainsKey(emitterEntity))
    {
        return;
    }

    EmitterState& state = mEmitterTable.GetItem(emitterEntity);
    if (state.children.IsEmpty())
    {
        return;
    }

    // FIFO: oldest child is at front (index 0).
    Dia::Entity::Entity oldest = state.children[0];
    DespawnInternal(oldest, Dia::Entity::DespawnReason::Cap, /*destroyInDomain=*/true);
}

// ---------------------------------------------------------------------------
// DespawnAll
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::DespawnAll()
{
    // Collect all tracked children first, then despawn.
    // Copy list to avoid modifying mTrackedChildrenList while iterating.
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxTrackedEntities> toDestroy;
    for (unsigned int i = 0; i < mTrackedChildrenList.Size(); ++i)
    {
        toDestroy.Add(mTrackedChildrenList[i]);
    }

    for (unsigned int i = 0; i < toDestroy.Size(); ++i)
    {
        Dia::Entity::Entity entity = toDestroy[i];
        if (mChildTable.ContainsKey(entity))
        {
            DespawnInternal(entity, Dia::Entity::DespawnReason::Explicit, /*destroyInDomain=*/true);
        }
    }
}

// ---------------------------------------------------------------------------
// TickChildAges
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::TickChildAges(float dt)
{
    // Collect entities to despawn — cannot modify mTrackedChildrenList mid-loop.
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxTrackedEntities> toExpire;

    for (unsigned int i = 0; i < mTrackedChildrenList.Size(); ++i)
    {
        Dia::Entity::Entity child = mTrackedChildrenList[i];
        if (!mChildTable.ContainsKey(child))
        {
            continue;
        }

        SpawnedChildData& data = mChildTable.GetItem(child);
        data.age += dt;

        // Check lifetime against emitter's SpawnEmitterComponent.
        if (data.emitterEntity.IsValid())
        {
            SpawnEmitterComponent* comp =
                mDomain.GetComponent<SpawnEmitterComponent>(data.emitterEntity);
            if (comp && comp->lifetime > 0.0f && data.age >= comp->lifetime)
            {
                if (!toExpire.IsFull()) { toExpire.Add(child); }
            }
        }
    }

    for (unsigned int i = 0; i < toExpire.Size(); ++i)
    {
        DespawnInternal(toExpire[i], Dia::Entity::DespawnReason::Lifetime, /*destroyInDomain=*/true);
    }
}

// ---------------------------------------------------------------------------
// TickChildRadii
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::TickChildRadii()
{
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxTrackedEntities> toExpire;

    for (unsigned int i = 0; i < mTrackedChildrenList.Size(); ++i)
    {
        Dia::Entity::Entity child = mTrackedChildrenList[i];
        if (!mChildTable.ContainsKey(child))
        {
            continue;
        }

        const SpawnedChildData& data = mChildTable.GetItemConst(child);
        if (!data.emitterEntity.IsValid())
        {
            continue;
        }

        SpawnEmitterComponent* comp =
            mDomain.GetComponent<SpawnEmitterComponent>(data.emitterEntity);
        if (!comp || comp->despawnRadius <= 0.0f)
        {
            continue;
        }

        // Use SpatialComponent positions if available.
        Dia::EntitySpatial::SpatialComponent* emitterSpatial =
            mDomain.GetComponent<Dia::EntitySpatial::SpatialComponent>(data.emitterEntity);
        Dia::EntitySpatial::SpatialComponent* childSpatial =
            mDomain.GetComponent<Dia::EntitySpatial::SpatialComponent>(child);

        if (!emitterSpatial || !childSpatial)
        {
            // Fall back to spawn position vs. emitter position (not available without spatial).
            // Skip radius check if spatial components are absent.
            continue;
        }

        float distSq = emitterSpatial->position.SquareDistanceTo(childSpatial->position);
        float radiusSq = comp->despawnRadius * comp->despawnRadius;
        if (distSq > radiusSq)
        {
            if (!toExpire.IsFull()) { toExpire.Add(child); }
        }
    }

    for (unsigned int i = 0; i < toExpire.Size(); ++i)
    {
        DespawnInternal(toExpire[i], Dia::Entity::DespawnReason::Radius, /*destroyInDomain=*/true);
    }
}

// ---------------------------------------------------------------------------
// DespawnInternal
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::DespawnInternal(Dia::Entity::Entity entity,
                                         Dia::Entity::DespawnReason reason,
                                         bool destroyInDomain)
{
    if (!mChildTable.ContainsKey(entity))
    {
        DIA_LOG_WARNING("DiaEntitySpawner",
                        "Despawn: entity not found in child table — it was not spawned by this spawner");
        return;
    }

    const SpawnedChildData& childData = mChildTable.GetItemConst(entity);
    Dia::Entity::Entity emitter = childData.emitterEntity;

    // Remove child from emitter's children list.
    if (emitter.IsValid() && mEmitterTable.ContainsKey(emitter))
    {
        EmitterState& state = mEmitterTable.GetItem(emitter);
        for (unsigned int i = 0; i < state.children.Size(); ++i)
        {
            if (state.children[i] == entity)
            {
                state.children.RemoveAt(i);
                break;
            }
        }
    }

    // Remove from child table.
    mChildTable.Remove(entity);

    // Remove from flat tracking list.
    for (unsigned int i = 0; i < mTrackedChildrenList.Size(); ++i)
    {
        if (mTrackedChildrenList[i] == entity)
        {
            mTrackedChildrenList.RemoveAt(i);
            break;
        }
    }

    // Fire event callback before destroying in domain.
    if (mDespawnCallback)
    {
        mDespawnCallback(mDespawnCallbackCtx, entity, reason);
    }

    // Optionally destroy the entity in the domain.
    if (destroyInDomain)
    {
        mDomain.QueueDestroy(entity);
        mDomain.EndOfFrame();
    }
}

} // namespace Dia::EntitySpawner
