#include <DiaEntitySpawner/EntitySpawnerImpl.h>
#include <DiaEntitySpawner/SpawnerTypes.h>
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
    childData.emitterEntity = request.parent;
    childData.spawnTime     = 0.0f;
    childData.age           = 0.0f;
    mChildTable.Add(entity, childData);

    // --- Track in emitter table ---
    if (request.parent.IsValid())
    {
        if (!mEmitterTable.ContainsKey(request.parent))
        {
            EmitterState newState;
            mEmitterTable.Add(request.parent, newState);
        }
        EmitterState& state = mEmitterTable.GetItem(request.parent);
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
// DespawnInternal
// ---------------------------------------------------------------------------
void EntitySpawnerImpl::DespawnInternal(Dia::Entity::Entity entity,
                                         Dia::Entity::DespawnReason /*reason*/,
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

    // Optionally destroy the entity in the domain.
    if (destroyInDomain)
    {
        mDomain.QueueDestroy(entity);
        mDomain.EndOfFrame();
    }
}

} // namespace Dia::EntitySpawner
