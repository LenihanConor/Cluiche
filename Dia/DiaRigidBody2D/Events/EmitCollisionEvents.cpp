#include "DiaRigidBody2D/Events/EmitCollisionEvents.h"

namespace Dia::RigidBody2D {

void EmitCollisionEvents(
    const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>&       currentContacts,
    Dia::Core::Containers::HashTable<BodyPairKey, CollisionPairState>&        activePairs,
    Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents>& outEvents,
    Dia::Core::ObserverSubject&                                               subject)
{
    outEvents.RemoveAll();

    // Mark all existing pairs as unseen this step
    for (unsigned int i = 0; i < activePairs.Size(); ++i)
        activePairs.GetItemByIndex(i).wasContacting = false;

    // Process current contacts → Enter or Stay
    for (unsigned int ci = 0; ci < currentContacts.Size(); ++ci)
    {
        const Contact& c = currentContacts[ci];
        const Body2DBase* bodyA = c.bodyA;
        const Body2DBase* bodyB = c.bodyB;
        if (!bodyA || !bodyB) continue;

        BodyPairKey pairKey(bodyA, bodyB);

        CollisionPairState* existing = activePairs.TryGetItem(pairKey);
        CollisionEvent evt;
        evt.bodyA = bodyA;
        evt.bodyB = bodyB;

        if (existing)
        {
            existing->wasContacting = true;
            evt.type = CollisionEventType::kStay;
        }
        else
        {
            CollisionPairState newState;
            newState.wasContacting = true;
            activePairs.Add(pairKey, newState);
            evt.type = CollisionEventType::kEnter;
        }

        if (!outEvents.IsFull())
            outEvents.Add(evt);
        subject.NotifyObservers(static_cast<int>(evt.type));
    }

    // Collect keys for pairs that are no longer active (must not modify during iteration)
    Dia::Core::Containers::DynamicArrayC<BodyPairKey, kMaxContacts> exitKeys;
    {
        auto iter    = activePairs.Begin();
        auto iterEnd = activePairs.End();
        while (iter != iterEnd)
        {
            if (!iter.Value().wasContacting)
                exitKeys.Add(iter.Key());
            ++iter;
        }
    }

    // Emit Exit and remove stale pairs
    for (unsigned int i = 0; i < exitKeys.Size(); ++i)
    {
        const BodyPairKey& exitKey = exitKeys[i];
        CollisionEvent evt;
        evt.type  = CollisionEventType::kExit;
        evt.bodyA = exitKey.ptrLo;
        evt.bodyB = exitKey.ptrHi;

        if (!outEvents.IsFull())
            outEvents.Add(evt);
        subject.NotifyObservers(static_cast<int>(CollisionEventType::kExit));
        activePairs.Remove(exitKey);
    }
}

} // namespace Dia::RigidBody2D
