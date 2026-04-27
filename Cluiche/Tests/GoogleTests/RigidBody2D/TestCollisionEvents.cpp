#include <gtest/gtest.h>

#include <DiaRigidBody2D/Events/EmitCollisionEvents.h>
#include <DiaRigidBody2D/Events/CollisionEvent.h>
#include <DiaRigidBody2D/Detection/Contact.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/World/BodyPairKey.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Architecture/Observer.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

using ContactList = Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>;
using EventList   = Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents>;
using PairTable   = Dia::Core::Containers::HashTable<BodyPairKey, CollisionPairState>;

// Simple counting observer
class CountingObserver : public Dia::Core::Observer {
public:
    int enterCount = 0;
    int stayCount  = 0;
    int exitCount  = 0;
    int total      = 0;

    void ObserverNotification(const Dia::Core::ObserverSubject*, int message) override
    {
        ++total;
        switch (static_cast<CollisionEventType>(message))
        {
        case CollisionEventType::kEnter: ++enterCount; break;
        case CollisionEventType::kStay:  ++stayCount;  break;
        case CollisionEventType::kExit:  ++exitCount;  break;
        }
    }
};

// Build two PointBody2D bodies and a Contact pairing them
struct BodyPair {
    Dia::Geometry2D::Transform tA, tB;
    PointBody2D bodyA, bodyB;

    BodyPair()
        : bodyA([]{ PointBodyDef d; d.mass=1.0f; return d; }())
        , bodyB([]{ PointBodyDef d; d.mass=1.0f; return d; }())
    {
        bodyA.SetUniqueId(1);
        bodyB.SetUniqueId(2);
    }

    Contact MakeContact() const
    {
        Contact c;
        c.bodyA = const_cast<PointBody2D*>(&bodyA);
        c.bodyB = const_cast<PointBody2D*>(&bodyB);
        c.normal = Vector2D(1.0f, 0.0f);
        c.depth  = 0.1f;
        return c;
    }
};

// ---------------------------------------------------------------------------
// Test 1 — First overlap emits kEnter
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, FirstOverlap_EmitsEnter)
{
    BodyPair bp;
    ContactList contacts; contacts.Add(bp.MakeContact());
    PairTable   pairs;    pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList   events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs; subject.AttachToObserver(&obs);

    EmitCollisionEvents(contacts, pairs, events, subject);

    EXPECT_EQ(obs.enterCount, 1);
    EXPECT_EQ(obs.stayCount,  0);
    EXPECT_EQ(obs.exitCount,  0);
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, CollisionEventType::kEnter);
}

// ---------------------------------------------------------------------------
// Test 2 — Continued overlap emits kStay (not second kEnter)
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, ContinuedOverlap_EmitsStay)
{
    BodyPair bp;
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs; subject.AttachToObserver(&obs);

    // Step 1: Enter
    ContactList step1; step1.Add(bp.MakeContact());
    EmitCollisionEvents(step1, pairs, events, subject);
    EXPECT_EQ(obs.enterCount, 1);

    // Step 2: Stay
    subject.DetachFromObserver(&obs);
    obs = CountingObserver{};
    subject.AttachToObserver(&obs);
    ContactList step2; step2.Add(bp.MakeContact());
    EmitCollisionEvents(step2, pairs, events, subject);

    EXPECT_EQ(obs.stayCount,  1);
    EXPECT_EQ(obs.enterCount, 0);
    EXPECT_EQ(obs.exitCount,  0);
}

// ---------------------------------------------------------------------------
// Test 3 — Separation emits kExit
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, Separation_EmitsExit)
{
    BodyPair bp;
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs; subject.AttachToObserver(&obs);

    // Step 1: Enter
    ContactList step1; step1.Add(bp.MakeContact());
    EmitCollisionEvents(step1, pairs, events, subject);

    // Step 2: Empty contacts → Exit
    subject.DetachFromObserver(&obs);
    obs = CountingObserver{};
    subject.AttachToObserver(&obs);
    ContactList step2;  // empty
    EmitCollisionEvents(step2, pairs, events, subject);

    EXPECT_EQ(obs.exitCount,  1);
    EXPECT_EQ(obs.enterCount, 0);
    EXPECT_EQ(obs.stayCount,  0);
}

// ---------------------------------------------------------------------------
// Test 4 — Re-collision after exit emits new kEnter
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, ReCollision_AfterExit_EmitsEnterAgain)
{
    BodyPair bp;
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs; subject.AttachToObserver(&obs);

    ContactList withContact; withContact.Add(bp.MakeContact());
    ContactList noContact;

    EmitCollisionEvents(withContact, pairs, events, subject);  // Enter
    EmitCollisionEvents(noContact,   pairs, events, subject);  // Exit
    subject.DetachFromObserver(&obs);
    obs = CountingObserver{};
    subject.AttachToObserver(&obs);
    EmitCollisionEvents(withContact, pairs, events, subject);  // Enter again

    EXPECT_EQ(obs.enterCount, 1);
    EXPECT_EQ(obs.exitCount,  0);
}

// ---------------------------------------------------------------------------
// Test 5 — Multiple listeners all receive events
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, MultipleListeners_AllReceive)
{
    BodyPair bp;
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs1, obs2;
    subject.AttachToObserver(&obs1);
    subject.AttachToObserver(&obs2);

    ContactList contacts; contacts.Add(bp.MakeContact());
    EmitCollisionEvents(contacts, pairs, events, subject);

    EXPECT_EQ(obs1.enterCount, 1);
    EXPECT_EQ(obs2.enterCount, 1);
}

// ---------------------------------------------------------------------------
// Test 6 — Unsubscribed listener does not receive events
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, Unsubscribe_NoMoreEvents)
{
    BodyPair bp;
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs; subject.AttachToObserver(&obs);

    ContactList noContact;
    EmitCollisionEvents(noContact, pairs, events, subject);  // register Enter state on next call

    ContactList withContact; withContact.Add(bp.MakeContact());
    EmitCollisionEvents(withContact, pairs, events, subject);
    EXPECT_EQ(obs.enterCount, 1);

    subject.DetachFromObserver(&obs);
    obs = CountingObserver{};

    // Further events — observer detached; should receive nothing
    ContactList step2; step2.Add(bp.MakeContact());
    EmitCollisionEvents(step2, pairs, events, subject);

    EXPECT_EQ(obs.total, 0);
}

// ---------------------------------------------------------------------------
// Test 7 — No contacts → no events emitted
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, NoContacts_NoEvents)
{
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;
    CountingObserver obs; subject.AttachToObserver(&obs);

    ContactList empty;
    EmitCollisionEvents(empty, pairs, events, subject);

    EXPECT_EQ(obs.total,   0);
    EXPECT_EQ(events.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 8 — Event contains correct body pointers
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionEvents, Event_ContainsCorrectBodies)
{
    BodyPair bp;
    PairTable pairs; pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    EventList events;
    Dia::Core::ObserverSubject subject;

    ContactList contacts; contacts.Add(bp.MakeContact());
    EmitCollisionEvents(contacts, pairs, events, subject);

    ASSERT_EQ(events.Size(), 1u);
    const Body2DBase* a = events[0].bodyA;
    const Body2DBase* b = events[0].bodyB;
    EXPECT_TRUE(a == static_cast<const Body2DBase*>(&bp.bodyA) || a == static_cast<const Body2DBase*>(&bp.bodyB));
    EXPECT_TRUE(b == static_cast<const Body2DBase*>(&bp.bodyA) || b == static_cast<const Body2DBase*>(&bp.bodyB));
    EXPECT_NE(a, b);
}
