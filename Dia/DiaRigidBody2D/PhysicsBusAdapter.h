#pragma once

#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaRigidBody2D/Messages/physics_messages.h"
#include <DiaCore/Architecture/Observer.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMessageBus/IFlushAdapter.h>

namespace Dia::MessageBus { class Bus; }

namespace Dia::RigidBody2D {

class PhysicsWorld;

// -----------------------------------------------------------------------
// PhysicsBusAdapter
//
// Bridges PhysicsWorld's collision-event ObserverSubject onto
// Dia::MessageBus::Bus as snapshot-safe Messages::PhysicsCollisionEvent
// values. Never forwards a raw Body2DBase* — a bus-queued message can sit
// until the next flush pass, by which time a physics body pointer could be
// stale, dangling, or reused (ABA).
//
// Identity: Body2DBase exposes no entity-handle identity (DiaRigidBody2D
// does not depend on DiaEntity — see the forbidden list in
// dia.rigidbody2d.architecture.module.md), so there is no per-entity
// address to route through Bus::kEntityRouterId. Each event is instead
// Bus::Broadcast-ed, carrying GetUniqueId() (the PhysicsWorld-assigned
// stable per-instance id that BodyPairKey itself hashes on — see
// World/BodyPairKey.h) plus GetId() (a StringCRC label) for both bodies.
//
// Why Observer + internal buffer instead of reading
// PhysicsWorld::GetLastCollisionEvents() lazily in Flush(): that accessor
// only reflects PhysicsWorld's most recent EmitCollisionEvents() call.
// PhysicsWorld::Update() can run multiple fixed-timestep sub-steps per
// frame (accumulator-based), and EmitCollisionEvents() clears/rebuilds its
// buffer on every call — so reading it once per bus tick would silently
// drop every sub-step's events except the last. Subscribing as an Observer
// and buffering inside ObserverNotification (called synchronously, once
// per sub-step, during the physics step) captures every sub-step; Flush()
// then just drains that buffer once per bus tick, decoupled from physics's
// own step timing.
//
// Dia::Core::ObserverSubject::NotifyObservers(int) only carries the
// CollisionEventType (Enter/Stay/Exit), not which bodies collided.
// EmitCollisionEvents() (Events/EmitCollisionEvents.cpp) always Add()s the
// triggering CollisionEvent to PhysicsWorld's outEvents buffer immediately
// before calling NotifyObservers() for that same event, so at the moment
// ObserverNotification fires, world.GetLastCollisionEvents().Back() is
// exactly the event that produced this notification. (Known edge case,
// inherited from EmitCollisionEvents' own pre-existing behaviour: if
// outEvents is at capacity, the event is dropped but NotifyObservers still
// fires; this adapter then has nothing new to read and simply forwards
// nothing for that notification.)
// -----------------------------------------------------------------------
class PhysicsBusAdapter : public Dia::MessageBus::IFlushAdapter, public Dia::Core::Observer {
public:
    // Registers Messages::PhysicsCollisionEvent with bus and subscribes to
    // world's collision-event ObserverSubject. Caller (the physics-owning
    // module) keeps both world and bus alive for this adapter's lifetime.
    PhysicsBusAdapter(PhysicsWorld& world, Dia::MessageBus::Bus& bus);
    ~PhysicsBusAdapter() override;

    PhysicsBusAdapter(const PhysicsBusAdapter&)            = delete;
    PhysicsBusAdapter& operator=(const PhysicsBusAdapter&) = delete;

    // Dia::Core::Observer — fires synchronously during PhysicsWorld's step.
    void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

    // Dia::MessageBus::IFlushAdapter — drains the buffer built up since the
    // last Flush() onto bus, then clears it.
    void Flush(Dia::MessageBus::Bus& bus) override;

private:
    PhysicsWorld& mWorld;
    Dia::Core::Containers::DynamicArrayC<Messages::PhysicsCollisionEvent, kMaxCollisionEvents> mPending;
};

} // namespace Dia::RigidBody2D
