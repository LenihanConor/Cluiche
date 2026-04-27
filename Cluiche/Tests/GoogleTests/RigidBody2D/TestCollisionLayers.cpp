#include <gtest/gtest.h>

#include <DiaRigidBody2D/Detection/DetectCollisions.h>
#include <DiaRigidBody2D/Detection/Contact.h>
#include <DiaRigidBody2D/Events/EmitCollisionEvents.h>
#include <DiaRigidBody2D/Events/CollisionEvent.h>
#include <DiaRigidBody2D/World/BodyPairKey.h>
#include <DiaRigidBody2D/World/PhysicsWorldCapacities.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Architecture/Observer.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

using ContactList = Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>;
using Grid        = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

// ---------------------------------------------------------------------------
// Fixture — two overlapping circles registered in a broad-phase
// ---------------------------------------------------------------------------

struct LayerFixture {
    Grid* grid = nullptr;

    Dia::Geometry2D::Transform tA;
    Dia::Geometry2D::Transform tB;
    Dia::Geometry2D::Circle  circA{1.0f, Vector2D::Zero()};
    Dia::Geometry2D::Circle  circB{1.0f, Vector2D::Zero()};

    PointBody2D* bodyA = nullptr;
    PointBody2D* bodyB = nullptr;

    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies> pts;
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies> rbs;
    ContactList contacts;

    LayerFixture(uint32_t layA, uint32_t maskA, uint32_t layB, uint32_t maskB)
    {
        tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
        tB.SetWorldPosition(Vector2D(0.5f, 0.0f));

        Grid::Def gd;
        gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
        gd.cellSize    = 10.0f;
        grid = new Grid(gd);

        PointBodyDef defA;
        defA.mass        = 1.0f;
        defA.transform   = &tA;
        defA.circleShape = &circA;
        defA.layer       = layA;
        defA.mask        = maskA;
        bodyA = new PointBody2D(defA);
        Dia::Geometry2D::AARect aabbA(Vector2D(-1.0f, -1.0f), Vector2D(1.0f, 1.0f));
        grid->Insert(static_cast<Body2DBase*>(bodyA), aabbA);

        PointBodyDef defB;
        defB.mass        = 1.0f;
        defB.transform   = &tB;
        defB.circleShape = &circB;
        defB.layer       = layB;
        defB.mask        = maskB;
        bodyB = new PointBody2D(defB);
        Dia::Geometry2D::AARect aabbB(Vector2D(-0.5f, -1.0f), Vector2D(1.5f, 1.0f));
        grid->Insert(static_cast<Body2DBase*>(bodyB), aabbB);

        bodyA->SetUniqueId(1);
        bodyB->SetUniqueId(2);
        pts.Add(bodyA);
        pts.Add(bodyB);
    }

    ~LayerFixture() { delete bodyA; delete bodyB; delete grid; }

    void Detect() { DetectCollisions(pts, rbs, grid, contacts); }
};

// ---------------------------------------------------------------------------
// Test 1 — Default bodies (kDefault layer, kAll mask) collide
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, DefaultBodies_Collide)
{
    LayerFixture f(Layers::kDefault, Layers::kAll, Layers::kDefault, Layers::kAll);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 1u);
}

// ---------------------------------------------------------------------------
// Test 2 — Compatible layers (Player vs Enemy both with kAll mask) collide
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, PlayerVsEnemy_Collide)
{
    LayerFixture f(Layers::kPlayer, Layers::kAll, Layers::kEnemy, Layers::kAll);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 1u);
}

// ---------------------------------------------------------------------------
// Test 3 — Projectile does not collide with other projectiles (mask excludes kProjectile)
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, Projectile_IgnoresProjectile)
{
    const uint32_t projMask = Layers::kAll & ~Layers::kProjectile;
    LayerFixture f(Layers::kProjectile, projMask, Layers::kProjectile, projMask);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 4 — Ghost (mask=0) collides with nothing
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, GhostBody_CollidesWithNothing)
{
    LayerFixture f(Layers::kDefault, Layers::kNone, Layers::kDefault, Layers::kAll);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 5 — Asymmetric: A accepts B, but B does not accept A — no contact
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, Asymmetric_NoContact)
{
    // A is on kDefault, mask includes kEnemy. B is on kEnemy, mask does NOT include kDefault.
    LayerFixture f(Layers::kDefault, Layers::kEnemy,
                   Layers::kEnemy,   Layers::kEnemy);  // B only accepts other enemies
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 6 — ShouldCollide filter prevents narrow-phase: verify no contact
//          (body positions unchanged = integration test of filter)
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, FilteredPair_NoContactProduced)
{
    // Mask = 0 means no collisions at all
    LayerFixture f(Layers::kDefault, Layers::kNone, Layers::kDefault, Layers::kNone);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 7 — No collision event emitted for filtered pair
// ---------------------------------------------------------------------------

class SimpleObserver : public Dia::Core::Observer {
public:
    int count = 0;
    void ObserverNotification(const Dia::Core::ObserverSubject*, int) override { ++count; }
};

TEST(RigidBody2D_CollisionLayers, FilteredPair_NoEventEmitted)
{
    LayerFixture f(Layers::kDefault, Layers::kNone, Layers::kDefault, Layers::kAll);
    f.Detect();  // produces no contacts

    Dia::Core::Containers::HashTable<BodyPairKey, CollisionPairState> pairs;
    pairs.SetSize(kMaxContacts, kMaxContacts * 2);
    Dia::Core::Containers::DynamicArrayC<CollisionEvent, kMaxCollisionEvents> events;
    Dia::Core::ObserverSubject subject;
    SimpleObserver obs; subject.AttachToObserver(&obs);

    EmitCollisionEvents(f.contacts, pairs, events, subject);

    EXPECT_EQ(obs.count, 0);
    EXPECT_EQ(events.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 8 — SetMask at runtime: filtered pair starts colliding after mask change
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, SetMask_RuntimeChange_EnablesCollision)
{
    LayerFixture f(Layers::kDefault, Layers::kNone, Layers::kDefault, Layers::kAll);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 0u);  // filtered initially

    // Now allow A to collide with everything
    f.bodyA->SetMask(Layers::kAll);
    f.contacts.RemoveAll();
    f.Detect();

    EXPECT_EQ(f.contacts.Size(), 1u);
}

// ---------------------------------------------------------------------------
// Test 9 — 32-layer setup: body on bit 31 collides with itself correctly
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionLayers, Layer32_CollidesCorrectly)
{
    constexpr uint32_t kLayer32 = 1u << 31;
    LayerFixture f(kLayer32, Layers::kAll, kLayer32, Layers::kAll);
    f.Detect();
    EXPECT_EQ(f.contacts.Size(), 1u);
}
