#pragma once

#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaRigidBody2D/Events/CollisionEvent.h"
#include "DiaRigidBody2D/World/BodyPairKey.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Containers/HashTables/HashTable.h"
#include "DiaCore/Architecture/Observer.h"

namespace Dia::RigidBody2D {

// Compares current contacts against activePairs to classify Enter/Stay/Exit.
// Fills outEvents, updates activePairs, and notifies observers (int = CollisionEventType).
void EmitCollisionEvents(
    const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>&       currentContacts,
    Dia::Core::Containers::HashTable<BodyPairKey, CollisionPairState>&        activePairs,
    Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents>& outEvents,
    Dia::Core::ObserverSubject&                                               subject);

} // namespace Dia::RigidBody2D
