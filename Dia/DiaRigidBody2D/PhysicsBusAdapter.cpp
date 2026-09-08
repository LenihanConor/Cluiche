#include "DiaRigidBody2D/PhysicsBusAdapter.h"

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include <DiaMessageBus/Bus.h>

namespace Dia::RigidBody2D {

PhysicsBusAdapter::PhysicsBusAdapter(PhysicsWorld& world, Dia::MessageBus::Bus& bus)
    : mWorld(world)
{
    // RegisterMessages is a no-op (returns false, no assert) if
    // PhysicsCollisionEvent is already registered, so this is safe even if
    // something else already registered it.
    Messages::RegisterMessages(bus, Messages::Handlers{});
    mWorld.GetCollisionEvents().AttachToObserver(this);
}

PhysicsBusAdapter::~PhysicsBusAdapter()
{
    mWorld.GetCollisionEvents().DetachFromObserver(this);
}

void PhysicsBusAdapter::ObserverNotification(const Dia::Core::ObserverSubject* /*subject*/, int /*message*/)
{
    const auto& lastEvents = mWorld.GetLastCollisionEvents();
    if (lastEvents.IsEmpty())
        return; // overflow edge case in EmitCollisionEvents — nothing new to forward

    if (mPending.IsFull())
        return; // fixed-capacity buffer at kMaxCollisionEvents; drop, matching EmitCollisionEvents' own overflow behaviour

    const CollisionEvent& evt = lastEvents.Back();

    Messages::PhysicsCollisionEvent msg;
    msg.type          = evt.type;
    msg.bodyAUniqueId = evt.bodyA ? evt.bodyA->GetUniqueId() : 0;
    msg.bodyBUniqueId = evt.bodyB ? evt.bodyB->GetUniqueId() : 0;
    msg.bodyAId       = evt.bodyA ? evt.bodyA->GetId() : Dia::Core::StringCRC::kZero;
    msg.bodyBId       = evt.bodyB ? evt.bodyB->GetId() : Dia::Core::StringCRC::kZero;
    mPending.Add(msg);
}

void PhysicsBusAdapter::Flush(Dia::MessageBus::Bus& bus)
{
    for (unsigned int i = 0; i < mPending.Size(); ++i)
        bus.Broadcast(mPending[i]);
    mPending.RemoveAll();
}

} // namespace Dia::RigidBody2D
