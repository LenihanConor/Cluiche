#pragma once
#ifndef DIA_ENTITYSPAWNER_ENTITYSPAWNERMODULE_H
#define DIA_ENTITYSPAWNER_ENTITYSPAWNERMODULE_H

#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/Streams/EventStreamWriter.h>
#include <DiaEntitySpawner/EntitySpawnerImpl.h>
#include <DiaEntitySpawner/Health/EntitySpawnerHealth.h>
#include <DiaEntitySpawner/SpawnEmitterComponent.h>
#include <DiaEntitySpawner/SpawnerTypes.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>

namespace Dia { namespace Entity {
    class Domain;
    class IBlueprintLoader;
} }

namespace Dia { namespace ApplicationFlow { class Application; } }
namespace Dia { namespace MessageBus { class Bus; } }

namespace Dia::EntitySpawner {

// ---------------------------------------------------------------------------
// EntitySpawnerModule
//
// ApplicationFlow::Module that drives EntitySpawnerImpl each frame.
// Responsibilities:
//   - Tick SpawnEmitterComponent token accumulation and burst logic.
//   - Evaluate lifetime and radius despawn conditions each Update.
//   - Publish EntitySpawnedEvent and EntityDespawnedEvent on sim-thread streams
//     AND directly on the shared Dia::MessageBus::Bus (same-thread subscribers).
//   - Subscribe to EntityDestroyedMessage to stay consistent with domain destroys.
//   - Expose IEntitySpawner& via GetSpawner() for game code.
// ---------------------------------------------------------------------------
class EntitySpawnerModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kInstanceId;

    // Domain&, IBlueprintLoader&, and Bus& must all outlive this module.
    EntitySpawnerModule(Dia::Entity::Domain& domain,
                        Dia::Entity::IBlueprintLoader& loader,
                        Dia::MessageBus::Bus& bus);

    // Exposes the underlying IEntitySpawner for caller-driven Spawn/Despawn.
    Dia::Entity::IEntitySpawner& GetSpawner();

    // Returns true if the spawner has a blueprint loader wired up.
    bool HasBlueprintLoader() const;

protected:
    // Called once before dedicated threads start; wires stream handles.
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    // Run token accumulation + burst for all active SpawnEmitterComponents.
    void TickEmitters(float dt);

    // Try to spawn one entity from an emitter, enforcing cap via FIFO overflow.
    void TrySpawnFromEmitter(Dia::Entity::Entity emitterEntity,
                             const SpawnEmitterComponent& comp,
                             EmitterState& state);

    // Tick lifetime + radius despawn conditions for all tracked children.
    void TickDespawnConditions(float dt);

    // Static callback forwarded to EntitySpawnerImpl::SetDespawnCallback.
    static void OnDespawnCallback(void* ctx,
                                  Dia::Entity::Entity entity,
                                  Dia::Entity::DespawnReason reason);

    Dia::Entity::Domain&      mDomain;
    EntitySpawnerImpl         mSpawner;

    // Shared message bus — same-thread Broadcast<T>() delivery path, alongside
    // the cross-PU EventStreamWriters below. Only this module layer touches
    // the bus; EntitySpawnerImpl has zero bus dependency.
    Dia::MessageBus::Bus&     mBus;

    // Event stream writers — connected in OnConnectStreams.
    Dia::ApplicationFlow::EventStreamWriter<Dia::Entity::EntitySpawnedEvent>
        mSpawnedWriter{this, Dia::Core::StringCRC("spawner.entity-spawned")};

    Dia::ApplicationFlow::EventStreamWriter<Dia::Entity::EntityDespawnedEvent>
        mDespawnedWriter{this, Dia::Core::StringCRC("spawner.entity-despawned")};

    // Subscription for EntityDestroyedMessage from the domain mailbox.
    Dia::Mailbox::SubscriptionHandle mDestroyedSub;

    // Metrics — registered in DoStart, nulled in DoStop.
    Dia::Observation::Metric::Gauge*   mMetricActiveCount       = nullptr;
    Dia::Observation::Metric::Counter* mMetricSpawnRate         = nullptr;
    Dia::Observation::Metric::Counter* mMetricDespawnLifetime   = nullptr;
    Dia::Observation::Metric::Counter* mMetricDespawnRadius     = nullptr;
    Dia::Observation::Metric::Counter* mMetricDespawnCap        = nullptr;
    Dia::Observation::Metric::Counter* mMetricDespawnExplicit   = nullptr;

    // Health reporter — registered in DoStart, unregistered in DoStop.
    EntitySpawnerHealth mHealthReporter{*this};
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_ENTITYSPAWNERMODULE_H
