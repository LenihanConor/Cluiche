#include <DiaEntitySpawner/EntitySpawnerModule.h>
#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/IBlueprintLoader.h>
#include <DiaEntity/Messages/EntityDestroyedMessage.h>
#include <DiaEntity/EntityAddress.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaObservation/Log/DiaLog.h>
#include <tuple>

namespace Dia::EntitySpawner {

const Dia::Core::StringCRC EntitySpawnerModule::kInstanceId("entity-spawner-module");

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
EntitySpawnerModule::EntitySpawnerModule(Dia::Entity::Domain& domain,
                                         Dia::Entity::IBlueprintLoader& loader)
    : Module(kInstanceId)
    , mDomain(domain)
    , mSpawner(domain)
{
    mSpawner.SetBlueprintLoader(&loader);
}

// ---------------------------------------------------------------------------
// GetSpawner
// ---------------------------------------------------------------------------
Dia::Entity::IEntitySpawner& EntitySpawnerModule::GetSpawner()
{
    return mSpawner;
}

// ---------------------------------------------------------------------------
// OnConnectStreams
// ---------------------------------------------------------------------------
void EntitySpawnerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mSpawnedWriter.Connect(app);
    mDespawnedWriter.Connect(app);
}

// ---------------------------------------------------------------------------
// DoStart
// ---------------------------------------------------------------------------
Dia::ApplicationFlow::StartResult EntitySpawnerModule::DoStart()
{
    // Subscribe to EntityDestroyedMessage to handle external entity destroys.
    // Use the module's kInstanceId CRC value as the subscriber ID.
    Dia::Mailbox::SubscriberId moduleSubscriberId;
    moduleSubscriberId.value = static_cast<uint64_t>(kInstanceId.Value());

    mDestroyedSub = mDomain.GetMailbox()
        .Subscribe<Dia::Entity::EntityDestroyedMessage>(moduleSubscriberId);

    if (!mDestroyedSub.IsValid())
    {
        DIA_LOG_WARNING("DiaEntitySpawner",
            "EntitySpawnerModule::DoStart: failed to subscribe to EntityDestroyedMessage "
            "(type may not be registered yet — drain will be a no-op until registered)");
    }

    // Wire the despawn callback so the impl can notify us when it despawns.
    mSpawner.SetDespawnCallback(&EntitySpawnerModule::OnDespawnCallback, this);

    return Dia::ApplicationFlow::StartResult::kReady;
}

// ---------------------------------------------------------------------------
// DoUpdate
// ---------------------------------------------------------------------------
void EntitySpawnerModule::DoUpdate(float deltaTime)
{
    // 1. Drain EntityDestroyedMessage — handle external destroys first.
    mDomain.GetMailbox().Drain<Dia::Entity::EntityDestroyedMessage>(
        [this](const Dia::Mailbox::Address& /*addr*/,
               const Dia::Entity::EntityDestroyedMessage& msg)
        {
            mSpawner.HandleExternalDestroy(msg.destroyed);
        });

    // 2. Tick despawn conditions (lifetime + radius).
    TickDespawnConditions(deltaTime);

    // 3. Tick emitters — token accumulation, burst, cap enforcement, spawn.
    TickEmitters(deltaTime);
}

// ---------------------------------------------------------------------------
// DoStop
// ---------------------------------------------------------------------------
Dia::ApplicationFlow::StopResult EntitySpawnerModule::DoStop()
{
    // Despawn all tracked children before shutting down.
    mSpawner.DespawnAll();

    // Unsubscribe from EntityDestroyedMessage.
    if (mDestroyedSub.IsValid())
    {
        mDomain.GetMailbox().Unsubscribe(mDestroyedSub);
    }

    // Clear the callback to avoid dangling pointer after module lifetime ends.
    mSpawner.SetDespawnCallback(nullptr, nullptr);

    return Dia::ApplicationFlow::StopResult::kDone;
}

// ---------------------------------------------------------------------------
// TickEmitters
// ---------------------------------------------------------------------------
void EntitySpawnerModule::TickEmitters(float dt)
{
    auto view = mDomain.Query<SpawnEmitterComponent>();
    for (auto entry : view)
    {
        Dia::Entity::Entity emitterEntity = entry.entity;
        SpawnEmitterComponent* comp = std::get<0>(entry.components);
        if (!comp || !comp->active)
        {
            continue;
        }

        EmitterState& state = mSpawner.GetOrCreateEmitterState(emitterEntity);

        // Burst on first activation.
        if (!state.burstFired && comp->burstCount > 0)
        {
            for (int i = 0; i < comp->burstCount; ++i)
            {
                TrySpawnFromEmitter(emitterEntity, *comp, state);
            }
            state.burstFired = true;
        }

        // Rate-based spawning: accumulate tokens.
        // rate=0 skips the while loop entirely.
        if (comp->rate > 0.0f)
        {
            state.tokenAccumulator += comp->rate * dt;
            while (state.tokenAccumulator >= 1.0f)
            {
                TrySpawnFromEmitter(emitterEntity, *comp, state);
                state.tokenAccumulator -= 1.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// TrySpawnFromEmitter
// ---------------------------------------------------------------------------
void EntitySpawnerModule::TrySpawnFromEmitter(Dia::Entity::Entity emitterEntity,
                                               const SpawnEmitterComponent& comp,
                                               EmitterState& state)
{
    // Cap enforcement (FIFO overflow): despawn oldest child before spawning.
    if (comp.cap > 0 && static_cast<int>(state.children.Size()) >= comp.cap)
    {
        mSpawner.DespawnOldestChild(emitterEntity);
    }

    // Build the spawn request.
    Dia::Entity::SpawnRequest req;
    req.blueprintId = comp.blueprintId;
    req.parent      = emitterEntity;
    req.tag         = comp.blueprintId;
    // Position: use zero for now; test or caller can override via position field.
    req.position    = Dia::Maths::Vector2D(0.0f, 0.0f);

    Dia::Entity::SpawnResult result = mSpawner.Spawn(req);

    if (result.error == Dia::Entity::SpawnError::None && result.entity.IsValid())
    {
        // Publish EntitySpawnedEvent.
        Dia::Entity::EntitySpawnedEvent ev;
        ev.entity      = result.entity;
        ev.blueprintId = comp.blueprintId;
        ev.tag         = comp.blueprintId;
        mSpawnedWriter.Send(ev);
    }
}

// ---------------------------------------------------------------------------
// TickDespawnConditions
// ---------------------------------------------------------------------------
void EntitySpawnerModule::TickDespawnConditions(float dt)
{
    // Age all tracked children; despawn those that exceed lifetime.
    mSpawner.TickChildAges(dt);

    // Despawn children that have moved beyond their emitter's despawnRadius.
    mSpawner.TickChildRadii();
}

// ---------------------------------------------------------------------------
// OnDespawnCallback  (static)
// ---------------------------------------------------------------------------
void EntitySpawnerModule::OnDespawnCallback(void* ctx,
                                             Dia::Entity::Entity entity,
                                             Dia::Entity::DespawnReason reason)
{
    EntitySpawnerModule* self = static_cast<EntitySpawnerModule*>(ctx);

    Dia::Entity::EntityDespawnedEvent ev;
    ev.entity = entity;
    ev.reason = reason;
    self->mDespawnedWriter.Send(ev);
}

} // namespace Dia::EntitySpawner
